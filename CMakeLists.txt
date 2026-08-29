add_library(xiao_rp2040 STATIC
    ${CMAKE_CURRENT_LIST_DIR}/can2040/src/can2040.c
)

target_include_directories(xiao_rp2040
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/can2040/src/can2040.h
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(xiao_rp2040
    PUBLIC
        pico_stdlib
        hardware_pio 
        hardware_dma
        hardware_pwm
)