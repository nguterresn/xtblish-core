#pragma once

int  mqtt_init(void);
void mqtt_thread(void* arg1, void* arg2, void* arg3);
int  mqtt_open(void);
int  mqtt_sub(const char* topic_name, void (*cb)());
