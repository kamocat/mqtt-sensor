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
#include "esp_log.h"
#include "mqtt_queue.h"
#include "driver/adc.h"
#include "battery.h"

#define TOPIC "rm_E_big_veggie"


static const char * TAG = "ADC";
const int oversample = 128;
const double atten = 1.0 / (4096 * oversample);

void adc_task(void *pvParameters)
{
	adc1_config_width(ADC_WIDTH_BIT_12);	
	adc1_config_channel_atten(ADC1_CHANNEL_7,ADC_ATTEN_DB_6); // measure up to 1750mV
    int32_t sum = 0;
    for(int i = 0; i < oversample; ++i){
      sum += adc1_get_raw(ADC1_CHANNEL_7);
    }
    double val = atten * sum;
    ESP_LOGI(TAG, "Read ADC value %f", val);
    mqtt_send_msg(TOPIC "/sun", val);
    hdq_reset(); 
    int celcius = (hdq_read16(0x6) - 2731); // Kelvin to Celcius
    val = celcius*(0.18) + 32; // Celcius to Farenheit
    mqtt_send_msg(TOPIC "/temp", val);
    ESP_LOGI("Temperature", "%.1f", val);
    val = hdq_read16(0x10) * 0.001;
    mqtt_send_msg(TOPIC "/charge", val);
    ESP_LOGI("Charge", "%.3fAh", val);
    val = hdq_read16(0x08) * 0.001;
    mqtt_send_msg(TOPIC "/voltage", val);
    ESP_LOGI("Voltage", "%.3fV", val);
    val = hdq_read16(0x14) * 0.001;
    mqtt_send_msg(TOPIC "/rate", val);
    ESP_LOGI("Rate", "%.3fA", val);
    ESP_LOGI("Stack usage", "%d bytes", uxTaskGetStackHighWaterMark(NULL));
    sleep(300, 5000/portTICK_PERIOD_MS);
}

extern "C" void app_main(void)
{
    hdq_init(); // Needs to happen before wifi so hdq chip has time to initialize
    mqtt_app_start();
    xTaskCreate(adc_task, "ADC task", 1024*4, (void *)0, 10, NULL);
}
