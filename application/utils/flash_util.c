#include "flash_util.h"
#include <string.h>
#include <zephyr/sys/__assert.h>
#include "zephyr/sys/printk.h"

static void flash_util_priv_write(struct flash_util* ctx, const uint8_t* src,
                                  uint32_t len, bool flush);
static void flash_util_buffer(struct flash_util* ctx, const uint8_t* src,
                              uint32_t len);

void flash_util_init(struct flash_util* ctx,
                     int (*write)(uint8_t*, uint32_t, uint32_t, uint32_t, bool))
{
	ctx->pos             = 0;
	ctx->sectors_written = 0;
	ctx->write           = write;
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void flash_util_write(struct flash_util* ctx, const uint8_t* src, uint32_t len,
                      bool flush)
{
	__ASSERT(ctx->pos <= FLASH_SECTOR_LEN,
	         "Current flash util position is bigger than permitted: %d\n",
	         ctx->pos);

	uint32_t available_space = FLASH_SECTOR_LEN - ctx->pos;
	if (len >= available_space) {
		// Received data is big enough to write to flash. May be buffered twice.
		flash_util_buffer(ctx, src, available_space);
		// Write the sector.
		flash_util_priv_write(ctx, src, available_space, flush);

		if (len > available_space) {
			// Buffer the remaining part.
			flash_util_buffer(ctx,
			                  src + available_space,
			                  len - available_space);
		}
	}
	else {
		// Received data is not big enough to write to flash.
		flash_util_buffer(ctx, src, len);
		__ASSERT(ctx->pos < FLASH_SECTOR_LEN,
		         "Flash util pos is equal of biffer than a flash sector: %d\n",
		         ctx->pos);

		if (flush) {
			flash_util_priv_write(ctx, src, len, flush);
		}
	}

	printk("len: %d pos: %d sectors written: %d flush: %d\n",
	       len,
	       ctx->pos,
	       ctx->sectors_written,
	       flush);
}

static void flash_util_buffer(struct flash_util* ctx, const uint8_t* src,
                              uint32_t len)
{
	__ASSERT(ctx->pos <= (ctx->buffer + FLASH_SECTOR_LEN - ctx->buffer),
	         "Flash util buffer has overflow, pos: %d\n",
	         ctx->pos);

	memcpy(ctx->buffer + ctx->pos, src, len);
	ctx->pos += len;
}

static void flash_util_priv_write(struct flash_util* ctx, const uint8_t* src,
                                  uint32_t len, bool flush)
{
	if (!flush) {
		__ASSERT(ctx->pos == FLASH_SECTOR_LEN,
		         "Flash util pos/len is not equal to a flash sector. Pos: %d "
		         "Len: "
		         "%d\n",
		         ctx->pos,
		         len);
	}

	ctx->write(ctx->buffer, FLASH_SECTOR_LEN, ctx->sectors_written, flush);
	ctx->sectors_written++;

	ctx->pos = 0;
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
}
