#pragma once

#include <stdint.h>

#define APPQ_FW_QUERY_FMT "/%s/%.*s/%.*s.bin"

enum appq_msg {
	APP_FIRMWARE_AVAILABLE,
	APP_FIRMWARE_READY,
};

// Example: /[prefix]/[id]/[uuid_a]-[uuid_b]-[uuid_c]-[uuid_d]-[uuid_e]
struct appq_fw_query {
	char prefix[16];
	char id[16];
	char version[36];
};

struct appq {
	enum appq_msg id;
	int           error;

	union {
		struct appq_fw_query fw_query;
	};
} __packed;
