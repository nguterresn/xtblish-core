#include "app_handle.h"
#include "app.h"
#include "http.h"
#include "wasm/wasm.h"
#include "zephyr/sys/__assert.h"
#include "zephyr/sys/printk.h"
#include "storage/flash.h"

static struct flash_ctx flash_ctx = { 0 };
static void             app_http_download_callback(struct http_response* res,
                                                   enum http_final_call  final_data);

void app_handle_firmware_available(struct appq* data)
{
	flash_util_init(&flash_ctx, flash_write_app1_sector_callback);
	int error = http_get_from_local_server(data->url,
	                                       app_http_download_callback);
	printk("[%s] error=%d\n", __func__, error);
}

void app_handle_firmware_downloaded(struct appq* data)
{
	int error = flash_app1_to_app0(data->bytes_to_write);
	printk("[%s] error=%d\n", __func__, error);
}

void app_handle_firmware_verified(struct appq* data)
{
	int error = wasm_replace();
	printk("[%s] error=%d\n", __func__, error);
}

static void app_http_download_callback(struct http_response* res,
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
		struct appq data = { .id             = APP_DOWNLOADED,
			                 .bytes_to_write = flash_ctx.bytes_written };
		app_send(&data);
	}
}
