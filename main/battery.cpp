#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#define ECHO_TEST_TXD 17
#define ECHO_TEST_RXD 16
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      2
#define ECHO_UART_BAUD_RATE     57600

#define BUF_SIZE 1024

uint8_t hdq_read(uint8_t cmd){
    const uint8_t hi = 0xFE;
    const uint8_t lo = 0xC0;
    uint8_t buf[32];
    int i = 0;
    for(; i < 8; ++i){
        buf[i] = cmd&1 ? hi : lo;
        cmd >>= 1;
    }
    uart_write_bytes(ECHO_UART_PORT_NUM, buf, 8);
    int len = uart_read_bytes(ECHO_UART_PORT_NUM, buf, 32, 10/portTICK_PERIOD_MS);
    uint8_t tmp = 0;
    for(i = len-8; i < len; ++i){
        tmp >>= 1;
        if(buf[i] > 0xF8)
            tmp |= 0x80;
    }
    return tmp;
}

int16_t hdq_read16(uint8_t cmd){
    int16_t result = hdq_read(cmd + 1);
    result <<= 8;
    result |= hdq_read(cmd);
    return result;
}

void hdq_reset(void){
    uint8_t buf = 0xFF;
    uart_write_bytes_with_break(ECHO_UART_PORT_NUM, &buf, 1, 20);
    vTaskDelay(1);
}

void hdq_init(void)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));
}