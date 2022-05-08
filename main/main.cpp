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
//    ESP_LOGI(TAG, "Read ADC value %f", sum);
    mqtt_send_msg("test/adc", val);
    sleep(10, 5000/portTICK_PERIOD_MS);
}

extern "C" void app_main(void)
{
    mqtt_app_start();
    xTaskCreate(adc_task, "ADC task", 1024*2, (void *)0, 10, NULL);
}
