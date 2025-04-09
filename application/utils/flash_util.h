#pragma once
#include "stdint.h"
#include "stdbool.h"

#define FLASH_SECTOR_LEN 4096

struct flash_util {
	uint8_t  buffer[FLASH_SECTOR_LEN];
	uint32_t pos; // in bytes.
	uint32_t sectors_written;
	int (*write)(uint8_t*, uint32_t, uint32_t, uint32_t, bool);
};

void flash_util_init(struct flash_util* ctx,
                     int (*write)(uint8_t*, uint32_t, uint32_t, uint32_t,
                                  bool));
void flash_util_write(struct flash_util* ctx, const uint8_t* src, uint32_t len,
                      bool flush);
