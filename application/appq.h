#pragma once

#include <stdint.h>

enum appm {
	APP_FIRMWARE_AVAILABLE,
	APP_FIRMWARE_READY,
};

struct appq_fw_available {
	char url[64];
};

struct appq_fw_ready {
	uint32_t length;
};

struct appq {
	enum appm id;
	int       error;

	union {
		struct appq_fw_available fw_available;
		struct appq_fw_ready     fw_ready;
	};
} __packed;
