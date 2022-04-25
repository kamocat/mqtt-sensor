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

static xQueueHandle sonar = NULL;
const gpio_num_t TRIG = gpio_num_t(22);
const gpio_num_t ECHO = gpio_num_t(23);

static const char * TAG = "SPI_SONAR";

int32_t rise, fall, diff;
static void IRAM_ATTR sonar_pulse_handler(void * arg){
    int32_t tmp = mcpwm_capture_signal_get_value(MCPWM_UNIT_0, MCPWM_SELECT_CAP0); //get capture signal counter value
    if(mcpwm_capture_signal_get_edge(MCPWM_UNIT_0, MCPWM_SELECT_CAP0)){
        rise = tmp;
    } else{
        fall = tmp;
    }
    diff = fall - rise;
    xQueueSendFromISR(sonar, &diff, NULL);
}

double sonar_pulse(void){
    
    if(gpio_get_level(ECHO))
        return -3; // previous pulse did not finish
    //Send pulse
    gpio_set_level(TRIG, 0);
    gpio_set_level(TRIG, 1);

    int32_t a, b;
    vTaskDelay(3 / portTICK_PERIOD_MS);
    a = mcpwm_capture_signal_get_value(MCPWM_UNIT_0, MCPWM_SELECT_CAP0); //get capture signal counter value
    //Wait for echo
    vTaskDelay(10 / portTICK_PERIOD_MS);
    b = mcpwm_capture_signal_get_value(MCPWM_UNIT_0, MCPWM_SELECT_CAP0); //get capture signal counter value

    //double coef = 0.1715; // millimeters per microsecond
    double f = rtc_clk_apb_freq_get() / 1000000;
    return (b-a) / f;
}

static void example_gpio_init(void)
{
    ("initializing mcpwm gpio...\n");
    gpio_reset_pin(TRIG);
    gpio_reset_pin(ECHO);
    gpio_set_direction(TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO, GPIO_MODE_INPUT);
    gpio_pulldown_en(ECHO);
}

static void capture_config(void)
{
    example_gpio_init();
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM_CAP_0, ECHO);

    //In general practice you can connect Capture  to external signal, measure time between rising edge or falling edge and take action accordingly
    mcpwm_capture_enable(MCPWM_UNIT_0, MCPWM_SELECT_CAP0, MCPWM_BOTH_EDGE, 0);  //capture signal on rising edge, prescale = 0 i.e. capture every edge
    mcpwm_isr_register(MCPWM_UNIT_0, sonar_pulse_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);  //Set ISR Handler
}

static spi_device_handle_t sonar_spi_handle;

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
        .clock_speed_hz = 800, // 100kHz
    };
    //Add our device to the bus
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &sonar_spi_handle);
    ESP_ERROR_CHECK(ret);
}

uint32_t rx_dma_buffer[1024];

static double spi_sonar(void)
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
    int i, rise, fall, diff, tmp;
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
    return diff * 0.01; // Convert to milliseconds
}


extern "C" void app_main(void)
{
    sonar = xQueueCreate( 10, sizeof(int64_t));
    spi_config();
    while(1) {
        double dist = spi_sonar();
        printf("Pulse is %f ms long\n", dist);
        vTaskDelay(100 / portTICK_PERIOD_MS);

    }
}
