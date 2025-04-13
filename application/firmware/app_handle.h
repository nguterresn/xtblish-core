#pragma once
#include "appq.h"

void app_handle_init();
void app_handle_firmware_available(struct appq* data);
void app_handle_firmware_downloaded(struct appq* data);
void app_handle_firmware_verified(struct appq* data);
