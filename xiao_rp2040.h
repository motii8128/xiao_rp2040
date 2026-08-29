#ifndef XIAO_RP2040_H_
#define XIAO_RP2040_H_

#include "can2040/src/can2040.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "ws2812.pio.h"

// static object for CAN communication
static struct {
    uint32_t pull_pos;
    volatile uint32_t push_pos;
    struct can2040_msg queue[128];
} MessageQueue;
static struct can2040 cbus;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CANトランシーバーと接続するピン番号
#define CAN_TX 7
#define CAN_RX 6
// CAN通信は1Mbpsにする
#define CAN_BIT_RATE 1000000
// メッセージキューの大きさ
#define QUEUE_SIZE 128 // Must be power of 2

// WS2812との通信に使うPIOの番号を0か1で指定する
// CAN通信をする場合はCAN通信のためにPIO0が使われるのでLEDはPIO1を使うので１にする
#define WS2812_PIO 1

// WS2812がつながっているピンを指定する
#define WS2812_PIN PICO_DEFAULT_WS2812_PIN

// CANを受信したときのコールバック関数
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define BLUE_LED 25
#define GREEN_LED 16
#define RED_LED 17

typedef struct WS2812
{
    PIO pio;
    uint sm;
    uint offset;
}WS2812;

typedef struct PWM
{
    uint slice_num;
    uint channel;
    uint max_duty;
}PWM;


/// @brief 内蔵LEDを初期化するように
/// @param ws2812 WS2812構造体のポインタを引数とす
void init_internal_led(WS2812* ws2812)
{
    gpio_init(BLUE_LED);
    gpio_set_dir(BLUE_LED, true);
    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, true);
    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, true);

    gpio_init(PICO_DEFAULT_WS2812_POWER_PIN);
    gpio_set_dir(PICO_DEFAULT_WS2812_POWER_PIN, true);
    gpio_put(PICO_DEFAULT_WS2812_POWER_PIN, true);

    if(WS2812_PIO == 0)
    {
        ws2812->pio = pio0;
    }
    else if(WS2812_PIO == 1)
    {
        ws2812->pio = pio1;
    }
    else
    {
        ws2812->pio = pio0;
    }

    ws2812->sm = 0;
    ws2812->offset = pio_add_program(ws2812->pio, &ws2812_program);

    ws2812_program_init(
        ws2812->pio, 
        ws2812->sm, 
        ws2812->offset, 
        WS2812_PIN, 
        800000, 
        false
    );
}

/// @brief 内蔵LEDの赤をコントロールする
/// @param enable trueなら点灯、falseなら消灯
void control_red(bool enable)
{
    gpio_put(RED_LED, enable);
}

/// @brief 内蔵LEDの緑をコントロールする
/// @param enable trueなら点灯、falseなら消灯
void control_green(bool enable)
{
    gpio_put(GREEN_LED, enable);
}

/// @brief 内蔵LEDの青をコントロールする
/// @param enable trueなら点灯、falseなら消灯
void control_blue(bool enable)
{
    gpio_put(BLUE_LED, enable);
}

/// @brief 内蔵WS2812をコントロールする
/// @param ws2812 構造体のポインタ
/// @param r 赤の強さ(0~255)
/// @param g 緑の強さ(0~255)
/// @param b 青の強さ(0~255)
void control_ws2812(WS2812* ws2812, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t rgb_code = 
        ((uint32_t) (r) << 8) |
        ((uint32_t) (g) << 16) |
        ((uint32_t) (0) << 24) |
        (uint32_t) (b);

    pio_sm_put_blocking(
        ws2812->pio,
        ws2812->sm,
        rgb_code << 8u
    );
}

/// @brief CAN通信における割り込み処理側にCANバス構造体を渡す。特に変えない
static void
PIOx_IRQHandler(void)
{
    can2040_pio_irq_handler(&cbus);
}

/// @brief CAN通信周りを初期化する。PIO0を使用する 
void canbus_setup(void)
{
    uint32_t pio_num = 0;
    uint32_t sys_clock = clock_get_hz(clk_sys), bitrate = CAN_BIT_RATE;
    uint32_t gpio_rx = CAN_RX, gpio_tx = CAN_TX;

    // Setup canbus
    can2040_setup(&cbus, pio_num);
    can2040_callback_config(&cbus, can2040_cb);

    // Enable irqs
    irq_set_exclusive_handler(PIO0_IRQ_0, PIOx_IRQHandler);
    irq_set_priority(PIO0_IRQ_0, 1);
    irq_set_enabled(PIO0_IRQ_0, 1);

    // Start canbus
    can2040_start(&cbus, sys_clock, bitrate, gpio_rx, gpio_tx);
}


/// @brief 指定されたピンをPWMとして扱う
/// @param gpio PWM出力したいピン
/// @param frequency PWM周波数を指定する
/// @return PWM構造体
PWM init_pwm(uint gpio, uint frequency)
{
    PWM pwm;
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    pwm.slice_num = pwm_gpio_to_slice_num(gpio);
    pwm.channel = pwm_gpio_to_channel(gpio);
    
    // 分周比は125で固定にしとく。だからカウントの総数は1MHzである
    pwm_set_clkdiv(pwm.slice_num, 125.0f);

    int wrap = 1000000 / frequency;

    pwm.max_duty = wrap-1;
    pwm_set_wrap(pwm.slice_num, wrap-1);

    pwm_set_chan_level(pwm.slice_num, pwm.channel, 0);

    pwm_set_enabled(pwm.slice_num, true);

    return pwm;
}

/// @brief PWMのデューティ比を設定する
/// @param pwm PWM構造体のポインタを渡す
/// @param duty デューティ比
void control_pwm(PWM* pwm, uint duty)
{
    if(duty > pwm->max_duty)
    {
        pwm_set_chan_level(
            pwm->slice_num, 
            pwm->channel,
            pwm->max_duty
        );
    }
    else
    {
        pwm_set_chan_level(
            pwm->slice_num, 
            pwm->channel,
            duty
        );
    }
}

#endif