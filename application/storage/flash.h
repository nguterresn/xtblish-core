#pragma once
#include "stdint.h"
#include "stdbool.h"

#define FLASH_SECTOR_LEN 4096

struct flash_ctx {
	uint8_t  buffer[FLASH_SECTOR_LEN];
	uint32_t pos; // in bytes.
	uint32_t bytes_written;
	int (*flush)(uint8_t*, uint32_t, uint32_t);
};

int         flash_init(void);
int         flash_app1_to_app0(uint32_t bytes_to_write);
const void* flash_get_app0();

void flash_util_init(struct flash_ctx* ctx);
void flash_util_write(struct flash_ctx* ctx, const uint8_t* src, uint32_t len,
                      bool last);
