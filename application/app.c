#include "app.h"
#include "appq.h"
#include "wasm/wasm.h"
#include "http.h"
#include "stdio.h"
#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <zephyr/net/http/client.h>
#include <zephyr/sys/printk.h>

K_MSGQ_DEFINE(appq, sizeof(struct appq), 5, 1); // len: 5

static uint32_t wasm_file_index = 0;

static int  app_handle_message(struct appq* data);
static void app_http_download_callback(struct http_response* res,
                                       enum http_final_call  final_data);

// (!) Important Note:
// Due to the way the Wifi manages the heap, it should be correclty initialized
// before any runtime is set. Wifi heap can not allocate memory in an external RAM.

int app_init()
{
	return 0;
}

int app_send(struct appq* data)
{
	return k_msgq_put(&appq, data, K_MSEC(1000));
}

void app_thread(void* arg1, void* arg2, void* arg3)
{
	int         error = 0;
	struct appq data  = { 0 };

	printk("Start 'app_thread'\n");

	/// ------ ///

	for (;;) {
		if (k_msgq_get(&appq, &data, K_FOREVER) == 0) {
			error = app_handle_message((struct appq*)&data);
			if (error) {
				// Handle this better in the future.
				k_sleep(K_MSEC(3000));
			}
		}
	}
}

static int app_handle_message(struct appq* data)
{
	int error = 0;

	switch (data->id) {
	case APP_FIRMWARE_AVAILABLE:
		return http_get_from_local_server(data->url,
		                                  app_http_download_callback);

	case APP_FIRMWARE_DOWNLOADED:
		return wasm_verify_and_copy(data->app1_write_len);
	case APP_FIRMWARE_VERIFIED:
		error = wasm_replace();
		printk("Done. Error: %d\n", error);
		return error;
	}

	return -EINVAL;
}

static void app_http_download_callback(struct http_response* res,
                                       enum http_final_call  final_data)
{
	if (199 > res->http_status_code || res->http_status_code >= 300) {
		return;
	}

	if (!res->body_found) {
		return;
	}

	int result = wasm_write_app1(res->body_frag_start,
	                             res->body_frag_len,
	                             final_data == HTTP_DATA_FINAL);
	if (result < 0) {
		printk("Failed to write to app1 for %d len %d\n",
		       result,
		       (int)res->body_frag_len);
		return;
	}

	if (final_data == HTTP_DATA_FINAL) {
		struct appq data = { .id             = APP_FIRMWARE_DOWNLOADED,
			                 .app1_write_len = result };
		app_send(&data);
	}
}
