#pragma once

#include <stdint.h>

enum appq_msg {
	APP_AVAILABLE = 0x01,
	APP_DOWNLOADED,
	APP_VERIFIED,
	IMAGE_AVAILABLE,
	IMAGE_DOWNLOADED,
};

enum appq_error {
	ERROR_GENERAL = 0x0100,
	ERROR_OTHER
};

struct appq {
	union {
		enum appq_msg   id;
		enum appq_error error;
	};

	union {
		char     url[96];
		uint32_t bytes_to_write;
	};
};
