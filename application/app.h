#pragma once

#include <stdint.h>
#include "firmware/appq.h"

int  app_init();
int  app_send(struct appq* data);
void app_thread(void* arg1, void* arg2, void* arg3);
