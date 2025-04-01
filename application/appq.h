#pragma once

#include <stdint.h>

enum appq_msg {
	APP_FIRMWARE_AVAILABLE,
	APP_FIRMWARE_BOOT,
};

struct appq {
	enum appq_msg id;
	int           error;

	union {
		char url[96];
	};
} __packed;
