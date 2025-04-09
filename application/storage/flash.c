#include "flash.h"
#include <string.h>
#include "app.h"
#include "app/appq.h"
#include "errno.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/printk.h>
#include <zephyr/storage/flash_map.h>

#if defined(CONFIG_SOC_ESP32S3)
#include <spi_flash_mmap.h>
static spi_flash_mmap_handle_t app0_handle = 0;
static spi_flash_mmap_handle_t app1_handle = 0;
const void*                    app0_mem    = NULL;
const void*                    app1_mem    = NULL;
#else
#error "Memory mapping is not yet supported for other socs!"
#endif

const struct flash_area* app0_wasm_area = NULL;
const struct flash_area* app1_wasm_area = NULL;

static void flash_context_priv_write(struct flash_ctx* ctx, const uint8_t* src,
                                     uint32_t len, bool flush);
static void flash_context_buffer(struct flash_ctx* ctx, const uint8_t* src,
                                 uint32_t len);

int flash_init(void)
{
	int error = 0;

	error = flash_area_open(FIXED_PARTITION_ID(app0_partition),
	                        &app0_wasm_area);
	if (error < 0) {
		printk("Error while opening flash partition 'app0_partition'");
		return error;
	}

	error = flash_area_open(FIXED_PARTITION_ID(app1_partition),
	                        &app1_wasm_area);
	if (error < 0) {
		printk("Error while opening flash partition 'app1_partition'");
		return error;
	}

#if defined(CONFIG_SOC_ESP32S3)
	error = spi_flash_mmap(app0_wasm_area->fa_off,
	                       app0_wasm_area->fa_size,
	                       SPI_FLASH_MMAP_DATA,
	                       &app0_mem,
	                       &app0_handle);
	if (error) {
		return error;
	}

	error = spi_flash_mmap(app1_wasm_area->fa_off,
	                       app1_wasm_area->fa_size,
	                       SPI_FLASH_MMAP_DATA,
	                       &app1_mem,
	                       &app1_handle);
	if (error) {
		return error;
	}
#endif

	printk("app0_wasm_area off: 0x%08x size: %u\napp1_wasm_area off: 0x%08x "
	       "size: %u\n",
	       (uint32_t)app0_wasm_area->fa_off,
	       (uint32_t)app0_wasm_area->fa_size,
	       (uint32_t)app1_wasm_area->fa_off,
	       (uint32_t)app1_wasm_area->fa_size);

	return 0;
}

int flash_app1_write(uint8_t* src, uint32_t len, uint32_t sectors, bool force)
{
	__ASSERT(len == FLASH_SECTOR_LEN,
	         "len is different from FLASH_SECTOR_LEN: %d\n",
	         len);

	int      error  = 0;
	uint32_t offset = sectors * len;

	error = flash_area_erase(app1_wasm_area, offset, len);
	if (error) {
		printk("[%s] Failed to erase 'app1_wasm_area' offset: 0x%02x len: %d\n",
		       __func__,
		       offset,
		       len);
		return error;
	}

	error = flash_area_write(app1_wasm_area, offset, src, len);
	if (error) {
		printk("[%s] Failed to write to 'app1_wasm_area' offset: 0x%02x len: "
		       "%d\n",
		       __func__,
		       offset,
		       len);
		return error;
	}

	return 0;
}

int flash_app1_to_app0(uint32_t sectors)
{
	int      error         = 0;
	uint32_t bytes_written = sectors * FLASH_SECTOR_LEN;

	if (app1_mem == NULL) {
		printk("Failed to get app1 address\n");
		return -ENOMEM;
	}

	// Not sure this erases by section. Check that later.
	error = flash_area_erase(app0_wasm_area, 0, bytes_written);
	if (error) {
		printk("Failed to erase %d byted from app0 error=%d\n",
		       bytes_written,
		       error);
		return error;
	}

	// Write app1 -> app0
	error = flash_area_write(app0_wasm_area, 0, app1_mem, bytes_written);
	if (error) {
		printk("Failed to write %d byted to app0 error=%d\n",
		       bytes_written,
		       error);
		return error;
	}

	struct appq data = { .id = APP_FIRMWARE_VERIFIED, .app1_sectors = sectors };
	app_send(&data);

	return 0;
}

void flash_context_init(struct flash_ctx* ctx,
                        int (*write)(uint8_t*, uint32_t, uint32_t, bool))
{
	ctx->pos     = 0;
	ctx->sectors = 0;
	ctx->write   = write;
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void flash_context_write(struct flash_ctx* ctx, const uint8_t* src,
                         uint32_t len, bool flush)
{
	__ASSERT(ctx->pos <= FLASH_SECTOR_LEN,
	         "Current flash util position is bigger than permitted: %d\n",
	         ctx->pos);

	uint32_t available_space = FLASH_SECTOR_LEN - ctx->pos;
	if (len >= available_space) {
		// Received data is big enough to write to flash. May be buffered twice.
		flash_context_buffer(ctx, src, available_space);
		// Write the sector.
		flash_context_priv_write(ctx, src, available_space, flush);

		if (len > available_space) {
			// Buffer the remaining part.
			flash_context_buffer(ctx,
			                     src + available_space,
			                     len - available_space);
		}
	}
	else {
		// Received data is not big enough to write to flash.
		flash_context_buffer(ctx, src, len);
		__ASSERT(ctx->pos < FLASH_SECTOR_LEN,
		         "Flash util pos is equal of biffer than a flash sector: %d\n",
		         ctx->pos);

		if (flush) {
			flash_context_priv_write(ctx, src, len, flush);
		}
	}

	printk("len: %d pos: %d sectors written: %d flush: %d\n",
	       len,
	       ctx->pos,
	       ctx->sectors,
	       flush);
}

const void* flash_get_app0()
{
	return app0_mem;
}

static void flash_context_buffer(struct flash_ctx* ctx, const uint8_t* src,
                                 uint32_t len)
{
	__ASSERT(ctx->pos <= (ctx->buffer + FLASH_SECTOR_LEN - ctx->buffer),
	         "Flash util buffer has overflow, pos: %d\n",
	         ctx->pos);

	memcpy(ctx->buffer + ctx->pos, src, len);
	ctx->pos += len;
}

static void flash_context_priv_write(struct flash_ctx* ctx, const uint8_t* src,
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

	ctx->write(ctx->buffer, FLASH_SECTOR_LEN, ctx->sectors, flush);
	ctx->sectors++;

	ctx->pos = 0;
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
}
