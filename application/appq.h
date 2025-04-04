#pragma once

#include "platform_common.h"
#include <stdint.h>

enum appq_msg {
	APP_FIRMWARE_AVAILABLE,
	APP_FIRMWARE_DOWNLOADED,
	APP_FIRMWARE_VERIFIED
};

struct appq {
	enum appq_msg id;
	int           error;

	union {
		char     url[96];
		uint32_t app1_write_len;
	};
} __packed;
