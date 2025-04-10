#pragma once

#include <stdint.h>

enum appq_msg {
	APP_FIRMWARE_AVAILABLE = 0x01,
	APP_FIRMWARE_DOWNLOADED,
	APP_FIRMWARE_VERIFIED
};

enum appq_error {
	ERROR_GENERAL = 0x0100,
	ERROR_OTHER
};

struct appq_app1_flash {
	uint32_t sectors;
	uint32_t bytes_written;
};

struct appq {
	union {
		enum appq_msg   id;
		enum appq_error error;
	};

	union {
		char                   url[96];
		struct appq_app1_flash app1_flash;
	};
};
