#ifndef MQTT_QUEUE_HEADER
#define MQTT_QUEUE_HEADER

#include "freertos/queue.h"
#include <string>

struct msg {
    std::string topic;
    std::string value;
};

extern xQueueHandle mqtt_inbox;
extern xQueueHandle mqtt_outbox;

void mqtt_send_msg( const char * topic, double value );
void mqtt_app_start(void);


#endif
