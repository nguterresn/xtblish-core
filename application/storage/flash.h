#pragma once
#include "stdint.h"
#include "stdbool.h"
#include "app/appq.h"

#define FLASH_SECTOR_LEN 4096

struct flash_ctx {
	uint8_t  buffer[FLASH_SECTOR_LEN];
	uint32_t pos; // in bytes.
	uint32_t sectors;
	uint32_t bytes_written;
	int (*flush)(uint8_t*, uint32_t, uint32_t);
};

int         flash_init(void);
int         flash_app1_write_sector_callback(uint8_t* src, uint32_t len,
                                             uint32_t sector_offset);
int         flash_app1_to_app0(struct appq_app1_flash* app1_flash);
const void* flash_get_app0();

void flash_context_init(struct flash_ctx* ctx,
                        int (*write)(uint8_t*, uint32_t, uint32_t));
void flash_context_write(struct flash_ctx* ctx, const uint8_t* src,
                         uint32_t len, bool flush);
