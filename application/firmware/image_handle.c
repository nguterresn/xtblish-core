#include "image_handle.h"
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/printk.h>
#include <zephyr/dfu/mcuboot.h>
#include "app.h"
#include "firmware/appq.h"
#include "storage/flash.h"
#include "network/http.h"

static struct flash_ctx flash_ctx = { 0 };
static void             image_http_download_callback(struct http_response* res,
                                                     enum http_final_call  final_data);

void image_handle_firmware_available(struct appq* data)
{
	flash_util_init(&flash_ctx, flash_write_image1_sector_callback);
	int error = http_get_from_local_server(data->url,
	                                       image_http_download_callback);
	printk("[%s] error=%d\n", __func__, error);
}

void image_handle_firmware_downloaded(struct appq* data)
{
	(void)data;
	// Permanet for now. Not sure if a scratch partition is needed to use the other option.
	boot_request_upgrade(BOOT_UPGRADE_PERMANENT);
	sys_reboot(SYS_REBOOT_COLD);
}

static void image_http_download_callback(struct http_response* res,
                                         enum http_final_call  final_data)
{
	if (199 > res->http_status_code || res->http_status_code >= 300) {
		return;
	}

	if (!res->body_found || res->body_frag_start == NULL ||
	    !res->body_frag_len) {
		return;
	}

	__ASSERT(flash_ctx.flush != NULL,
	         "Flash util context is not initialized.\n");

	flash_util_write(&flash_ctx,
	                 res->body_frag_start,
	                 res->body_frag_len,
	                 final_data == HTTP_DATA_FINAL);

	if (final_data == HTTP_DATA_FINAL) {
		struct appq data = { .id             = IMAGE_DOWNLOADED,
			                 .bytes_to_write = flash_ctx.bytes_written };
		app_send(&data);
	}
}
