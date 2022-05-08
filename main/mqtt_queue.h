#ifndef MQTT_QUEUE_HEADER
#define MQTT_QUEUE_HEADER

#include "freertos/queue.h"
#include <string>

struct msg {
    std::string topic;
    std::string value;
};

extern xQueueHandle mqtt_inbox;

void mqtt_send_msg( const char * topic, double value );
void mqtt_app_start(void);
void sleep(uint32_t seconds, uint32_t timeout);
void test_mqtt_rate(int qty);

#endif
