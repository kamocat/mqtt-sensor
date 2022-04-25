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
#include "esp_timer.h"
#include "soc/rtc.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"

static xQueueHandle sonar = NULL;
const gpio_num_t TRIG = gpio_num_t(22);
const gpio_num_t ECHO = gpio_num_t(23);

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

    double coef = 0.1715; // millimeters per microsecond
    double f = rtc_clk_apb_freq_get() / 1000000;
    return (b-a) / f;
}

static void example_gpio_init(void)
{
    printf("initializing mcpwm gpio...\n");
    gpio_reset_pin(TRIG);
    gpio_reset_pin(ECHO);
    gpio_set_direction(TRIG, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO, GPIO_MODE_INPUT);
    gpio_pulldown_en(ECHO);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM_CAP_0, ECHO);
}


static void capture_config(void)
{
    example_gpio_init();

    //In general practice you can connect Capture  to external signal, measure time between rising edge or falling edge and take action accordingly
    mcpwm_capture_enable(MCPWM_UNIT_0, MCPWM_SELECT_CAP0, MCPWM_BOTH_EDGE, 0);  //capture signal on rising edge, prescale = 0 i.e. capture every edge
    mcpwm_isr_register(MCPWM_UNIT_0, sonar_pulse_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);  //Set ISR Handler
}


extern "C" void app_main(void)
{
    capture_config();
    sonar = xQueueCreate( 10, sizeof(int64_t));
    while(1) {
        double dist = sonar_pulse();
        printf("Object is %f mm away\n", dist);
        vTaskDelay(100 / portTICK_PERIOD_MS);

    }
}
