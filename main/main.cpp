/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "mqtt_queue.h"

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

uint8_t hdq_name(char * buf){
    uint8_t len = hdq_read(0x62);
    for(int i = 0; i < len; ++i){
        buf[i] = hdq_read(i+0x63);
    }
    return len;
}

// Requires 20 bytes in buffer
void bin16(uint16_t x, char * buf){
    for(int i = 4; i; --i){
        for(int j = 4; j; --j){
            *buf++ = x&0x8000 ? '1' : '0';
            x <<= 1;
        }
        *buf++ = ' ';
    }
    *buf++ = 0; // null termination
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

void charger_task(void *pvParameters)
{
    hdq_init();
    while(1){
        hdq_reset(); 
        int celcius = (hdq_read16(0x28) - 2731); // Kelvin to Celcius
        double val = celcius*(0.18) + 32; // Celcius to Farenheit
        mqtt_send_msg("bat/charger/temp", val);
        ESP_LOGI("Temperature", "%f", val);
        val = hdq_read16(0x0C) * 0.001;
        mqtt_send_msg("bat/charger/charge", val);
        ESP_LOGI("Charge", "%fAh", val);
        val = hdq_read16(0x08) * 0.001;
        mqtt_send_msg("bat/charger/voltage", val);
        ESP_LOGI("Voltage", "%fV", val);
        val = hdq_read16(0x14) * 0.001;
        mqtt_send_msg("bat/charger/rate", val);
        ESP_LOGI("Rate", "%fA", val);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        //sleep(300, 5000/portTICK_PERIOD_MS);
    }
}


extern "C" void app_main(void)
{
    mqtt_app_start();
    xTaskCreate(charger_task, "Charger Task", 1024*2, (void *)0, 10, NULL);
}
