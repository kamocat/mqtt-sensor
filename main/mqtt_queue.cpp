/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_sleep.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_queue.h"

xQueueHandle mqtt_inbox = NULL;
static SemaphoreHandle_t mqtt_outbox = NULL;

static const char *TAG = "MQTT_HANDLER";

static void log_error_if_nonzero(const char * message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static esp_err_t mqtt_event_handler_cb(void * arg)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) arg;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    struct msg m;
    BaseType_t xHigherPriorityTaskWoken = false;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGV(TAG, "MQTT_EVENT_CONNECTED");
            // FIXME: Add whatever topics you want to subscribe to
            //msg_id = esp_mqtt_client_subscribe(client, "test/setpoint", 0);
            ESP_LOGV(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGV(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGV(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGV(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGV(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            xSemaphoreGiveFromISR(mqtt_outbox, &xHigherPriorityTaskWoken);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            m.topic.erase();
            m.topic.assign(event->topic, event->topic_len);
            m.value.erase();
            m.value.assign(event->data, event->data_len);
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            xQueueSendFromISR(mqtt_inbox, &m, &xHigherPriorityTaskWoken);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGW(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

            }
            break;
        default:
            ESP_LOGD(TAG, "Other event id:%d", event->event_id);
            break;
    }
    if( xHigherPriorityTaskWoken ){
        portYIELD_FROM_ISR();
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static esp_mqtt_client_handle_t client;

void mqtt_app_start(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_log_level_set("*", ESP_LOG_DEBUG);
    /*
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_WARN);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_INFO);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_WARN);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_WARN);
    esp_log_level_set("TRANSPORT", ESP_LOG_WARN);
    esp_log_level_set("OUTBOX", ESP_LOG_WARN);
    */
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());
    mqtt_inbox = xQueueCreate( 10, sizeof( struct msg ));
    mqtt_outbox = xSemaphoreCreateCounting( 100, 0);
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = "192.168.1.110",
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void mqtt_send_msg( const char * topic, double value ){
    char buf[20];
    xSemaphoreTake(mqtt_outbox, 2); // delay up to two ticks
    snprintf(buf, 19, "%f", value);
    int msg_id = esp_mqtt_client_publish(client, topic, buf, 0, 1, 0);
    int outbox = uxSemaphoreGetCount(mqtt_outbox);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d, outbox count=%d", msg_id, outbox);
}

void sleep(uint32_t seconds, uint32_t timeout){
    // Make sure all the messages have been sent
    while(timeout && uxSemaphoreGetCount(mqtt_outbox)){
        --timeout;
        vTaskDelay(1); // delay one tick
    }
    // Power down for sleep
    esp_mqtt_client_stop(client);
    esp_wifi_stop();
    // Sleep for duration
    esp_sleep_enable_timer_wakeup( seconds * 1000000 );
    esp_deep_sleep_start();
    // After sleeping, the program will restart from the beginning
}

void test_mqtt_rate(int qty){
    struct msg m;
    for(int i = 0; i < qty; ++i){
        mqtt_send_msg("test/iter", i);
    }
}
