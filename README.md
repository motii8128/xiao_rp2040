# xiao_rp2040
Pico-SDKのためのテンプレートライブラリ

## Install
Pico-SDKプロジェクト内にこのリポジトリをクローンする
```sh
git clone https://github.com/motii8128/xiao_rp2040.git
```

そしてプロジェクトのCMakeLists.txtに以下のように書く
```cmake
add_subdirectory(xiao_rp2040)

target_link_libraries(project_name
    # your dependencies
    xiao_rp2040
)

target_include_directories(project PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/xiao_rp2040/
)
```

## Example
```c
#include "math.h"
#include <pico/stdlib.h>
#include <stdio.h>
#include "xiao_rp2040.h"


int main(void)
{
    stdio_init_all();

    // 内蔵LEDを初期化
    WS2812 ws2812;
    init_internal_led(&ws2812);
    
    // GPIO3をPWM出力に設定する
    PWM pwm = init_pwm(3, 1000);

    // CANバスを初期化
    canbus_setup();

    
    double x = -M_PI / 2.0;

    struct can2040_msg msg = {
        .id = 0x200,
        .dlc = 8,
        .data = {0, 0, 0, 0, 0, 0, 0, 0}
    };

    int16_t current = 500;
    msg.data[0] = (current >> 8) & 0xFF;
    msg.data[1] = current & 0xFF;

    // Main loop
    for (;;) {
        if (can2040_check_transmit(&cbus) > 0) {
            int status = can2040_transmit(&cbus, &msg);

            // 送信キューへの追加が成功した場合
            if (status == 0) {
                int b = 100*fabs(sin(x));
                // int r = 255*fabs(sin(x+M_PI/2.0));
                int g = 100*fabs(sin(x+M_PI/4.0));
                
                control_ws2812(&ws2812, 0, g, b);

                uint16_t duty = 999 * fabs(sin(x));
                control_pwm(&pwm, duty);

                x += 0.1;
            }
        } else {
            // 送信バッファが詰まっている場合のデバッグ出力
            printf("CAN Tx buffer full or error\n");
        }

        uint32_t push_pos = MessageQueue.push_pos;
        uint32_t pull_pos = MessageQueue.pull_pos;
        if (push_pos == pull_pos)
            // No new messages read.
            continue;

        // Pop message from local receive queue
        struct can2040_msg *qmsg = &MessageQueue.queue[pull_pos % QUEUE_SIZE];
        struct can2040_msg msg = *qmsg;
        MessageQueue.pull_pos++;    
        
        int16_t angle = msg.data[2] << 8 | msg.data[3];

        printf("msg: id=0x%x dlc=%d data=%d\n",
               msg.id, msg.dlc, angle);

        sleep_ms(20);
    }

    return 0;
}
```

CAN受信時のコールバック関数は``xiao_rp2040/xiao_rp2040.h``で編集してください
```c
static void
can2040_cb(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg)
{
    if (notify == CAN2040_NOTIFY_RX) {
        // Example message filter
        uint32_t id = msg->id;
        
        if(id == 0x201)
        {
            uint32_t push_pos = MessageQueue.push_pos;
            uint32_t pull_pos = MessageQueue.pull_pos;
            if (push_pos + 1 == pull_pos)
                // No space in queue
                return;
            MessageQueue.queue[push_pos % QUEUE_SIZE] = *msg;
            MessageQueue.push_pos = push_pos + 1;
        }
    }
}
```