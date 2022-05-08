/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "soc/rtc.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"
#include "mqtt_queue.h"

const gpio_num_t TRIG = gpio_num_t(22);
const gpio_num_t ECHO = gpio_num_t(23);

static const char * TAG = "SPI_SONAR";

static void example_gpio_init(void)
{
    ESP_LOGI(TAG, "initializing mcpwm gpio");
    gpio_reset_pin(TRIG);
    gpio_reset_pin(ECHO);
    gpio_set_direction(TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO, GPIO_MODE_INPUT);
    gpio_pulldown_en(ECHO);
}

spi_device_handle_t sonar_spi_handle = NULL;

static void spi_config(void)
{
    example_gpio_init();
    esp_err_t ret;
    ESP_LOGI(TAG, "Initializing bus SPI2...");
    spi_bus_config_t buscfg={
        .mosi_io_num = TRIG,
        .miso_io_num = ECHO,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4092,
    };
    //Initialize the SPI bus
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    spi_device_interface_config_t devcfg={
        .command_bits = 8,
        .address_bits = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_10M / 100, // 100kHz
        .queue_size = 2,
    };
    //Add our device to the bus
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &sonar_spi_handle);
    ESP_ERROR_CHECK(ret);
}

uint32_t rx_dma_buffer[1024];

static int spi_sonar(void)
{
    /* The sonar sensor isn't actually SPI-enabled, but we're using SPI
     * to measure the pulse width that it puts out
     */
    spi_transaction_t transaction = {
        .cmd = 0xFF,
        .length = 4000,
        .rx_buffer = rx_dma_buffer,
    };
    esp_err_t ret;
    ESP_LOGI(TAG, "Sending SPI packet");
    ret = spi_device_transmit( sonar_spi_handle, &transaction);
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "Finished SPI message");
    int i, rise, fall, diff;
    uint32_t tmp;
    //Find rise time
    for(i = 0; i < transaction.length/32; ++i){
        if(rx_dma_buffer[i])
            break;
    }
    //Increase accuracy
    tmp = __builtin_bswap32(rx_dma_buffer[i]);
    rise = i*32;
    for(; tmp; tmp>>=1){
        --rise;
    }
    //Find stop time
    for(; i < transaction.length/32; ++i){
        if(!rx_dma_buffer[i])
            break;
    }
    --i;
    //increase accuracy
    tmp = __builtin_bswap32(rx_dma_buffer[i]);
    fall = (i-1)*32;
    for(; tmp; tmp<<=1){
        ++fall;
    }

    diff = fall - rise;
    double coef = 323.0 / 2; // meters per second / 2
    return diff * 0.01 * coef; // Convert to milliseconds
}


void sonar_task(void * arg){
    spi_config();
    int dist = spi_sonar();
    mqtt_send_msg("waterwall/level", dist);
    printf("Distance is %d mm\n", dist);
    // Sleep for 5 minutes
    sleep(300, 5000/portTICK_PERIOD_MS);
}

extern "C" void app_main(void)
{
    mqtt_app_start();
    xTaskCreate(sonar_task, "Sonar task", 1024*2, (void *)0, 10, NULL);
}
