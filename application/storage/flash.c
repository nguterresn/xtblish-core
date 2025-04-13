#include "flash.h"
#include <string.h>
#include "app.h"
#include "errno.h"
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/printk.h>
#include <zephyr/storage/flash_map.h>
#include "firmware/appq.h"

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

static int  flash_app1_write_sector_callback(uint8_t* src, uint32_t len,
                                             uint32_t offset);
static void flash_util_flush(struct flash_ctx* ctx);
static void flash_util_copy(struct flash_ctx* ctx, const uint8_t* src,
                            uint32_t len);

//  Missing: mutex to protect `app1_mem` and `app0_mem`...

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
	if (error != 0) {
		return error;
	}

	error = spi_flash_mmap(app1_wasm_area->fa_off,
	                       app1_wasm_area->fa_size,
	                       SPI_FLASH_MMAP_DATA,
	                       &app1_mem,
	                       &app1_handle);
	if (error != 0) {
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

int flash_app1_to_app0(uint32_t bytes_to_write)
{
	int      error          = 0;
	uint32_t bytes_to_erase = bytes_to_write > FLASH_SECTOR_LEN
	                              ? bytes_to_write +
	                                    (FLASH_SECTOR_LEN -
	                                     bytes_to_write % FLASH_SECTOR_LEN)
	                              : FLASH_SECTOR_LEN;

	if (app1_mem == NULL) {
		printk("Failed to get app1 address\n");
		return -ENOMEM;
	}

	printk("Sectors to erase on app0: %d bytes: %d.\n",
	       bytes_to_erase / FLASH_SECTOR_LEN,
	       bytes_to_erase);

	error = flash_area_erase(app0_wasm_area, 0, bytes_to_erase);
	if (error) {
		printk("Failed to erase %d bytes from app0 error=%d\n",
		       bytes_to_erase,
		       error);
		return error;
	}

	printk("Bytes to write to app0: %d bytes.\n", bytes_to_write);

	// Write app1 -> app0
	error = flash_area_write(app0_wasm_area, 0, app1_mem, bytes_to_write);
	if (error) {
		printk("Failed to write %d byted to app0 error=%d\n",
		       bytes_to_write,
		       error);
		return error;
	}

	struct appq data = { .id = APP_VERIFIED };
	app_send(&data);

	return 0;
}

const void* flash_get_app0()
{
	return app0_mem;
}

/**
 * @brief Erase and write a sector into flash app1 partition
 *
 * @param src the data to be written to flash
 * @param len the amount of data to be written to flash in bytes
 * @param offset the offset in bytes
 * @return int
 */
static int flash_app1_write_sector_callback(uint8_t* src, uint32_t len,
                                            uint32_t offset)
{
	__ASSERT(offset % FLASH_SECTOR_LEN == 0,
	         "Offset mod != FLASH_SECTOR_LEN: %d\n",
	         offset);
	int error = 0;

	error = flash_area_erase(app1_wasm_area, offset, FLASH_SECTOR_LEN);
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

	printk("For an offset of %d bytes, erased a section of %d bytes and wrote "
	       "%d bytes.\n",
	       offset,
	       FLASH_SECTOR_LEN,
	       len);

	return 0;
}

void flash_util_init(struct flash_ctx* ctx)
{
	ctx->pos           = 0;
	ctx->bytes_written = 0;
	ctx->flush         = flash_app1_write_sector_callback;
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void flash_util_write(struct flash_ctx* ctx, const uint8_t* src, uint32_t len,
                      bool last)
{
	__ASSERT(ctx->pos <= FLASH_SECTOR_LEN,
	         "Current flash util position is bigger than permitted: %d\n",
	         ctx->pos);

	uint32_t available_space = FLASH_SECTOR_LEN - ctx->pos;
	if (len >= available_space) {
		// Received data is big enough to write to flash. May be buffered twice.
		flash_util_copy(ctx, src, available_space);
		// Write the sector.
		__ASSERT(ctx->pos == FLASH_SECTOR_LEN,
		         "Flash pos is not equal to FLASH_SECTOR_LEN: %d\n",
		         ctx->pos);
		flash_util_flush(ctx);

		if (len > available_space) {
			// Buffer the remaining part.
			flash_util_copy(ctx, src + available_space, len - available_space);
		}
	}
	else {
		// Received data is not big enough to write to flash.
		flash_util_copy(ctx, src, len);
	}

	if (last) {
		flash_util_flush(ctx);
	}
}

static void flash_util_copy(struct flash_ctx* ctx, const uint8_t* src,
                            uint32_t len)
{
	__ASSERT(ctx->pos <= (ctx->buffer + FLASH_SECTOR_LEN - ctx->buffer),
	         "Flash util buffer has overflow, pos: %d\n",
	         ctx->pos);

	memcpy(ctx->buffer + ctx->pos, src, len);
	ctx->pos += len;
}

static void flash_util_flush(struct flash_ctx* ctx)
{
	ctx->flush(ctx->buffer, ctx->pos, ctx->bytes_written);
	ctx->bytes_written += ctx->pos;

	ctx->pos = 0;
	memset(ctx->buffer, 0, sizeof(ctx->buffer));
}
