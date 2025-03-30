#pragma once

#include <stdint.h>

enum appm {
	APP_FIRMWARE_AVAILABLE,
	APP_FIRMWARE_READY,
};

struct appq_fw_available {
	char query[128];
};

struct appq {
	enum appm id;
	int       error;

	union {
		struct appq_fw_available fw_available;
	};
} __packed;
