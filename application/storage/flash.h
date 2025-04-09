#pragma once
#include "stdint.h"
#include "stdbool.h"

#define FLASH_SECTOR_LEN 4096

struct flash_ctx {
	uint8_t  buffer[FLASH_SECTOR_LEN];
	uint32_t pos; // in bytes.
	uint32_t sectors;
	int (*write)(uint8_t*, uint32_t, uint32_t, bool);
};

int  flash_init(void);
int  flash_app1_write(uint8_t* src, uint32_t len, uint32_t sectors, bool force);
int  flash_app1_to_app0(uint32_t sectors);
const void* flash_get_app0(void);
const void* flash_get_app1(void);
void flash_release_app0();
void flash_release_app1();

void flash_context_init(struct flash_ctx* ctx,
                        int (*write)(uint8_t*, uint32_t, uint32_t, bool));
void flash_context_write(struct flash_ctx* ctx, const uint8_t* src,
                         uint32_t len, bool flush);
