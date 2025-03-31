#include "app.h"
#include "appq.h"
#include "wasm/wasm.h"
#include "http.h"
#include "stdio.h"
#include "zephyr/fatal_types.h"
#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <zephyr/net/http/client.h>
#include <zephyr/sys/printk.h>

K_MSGQ_DEFINE(appq, sizeof(struct appq), 5, 1); // len: 5

extern struct sys_heap _system_heap;
extern uint8_t         wasm_file[WASM_FILE_MAX_SIZE * 4];

static uint32_t                wasm_file_index = 0;
static struct sys_memory_stats stats;

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

	printk("\n\n** START WASM_RUNTIME ! **\n");

	__ASSERT(wasm_boot_app(false) == 0, "Failed to boot app\n");
	sys_heap_runtime_stats_get(&_system_heap, &stats);

	printk("\n\nINFO: Allocated Heap = %zu\n", stats.allocated_bytes);
	printk("INFO: Free Heap = %zu\n", stats.free_bytes);
	printk("INFO: Max Allocated Heap = %zu\n\n\n", stats.max_allocated_bytes);

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
		memset(wasm_file, 0, sizeof(wasm_file));
		return http_get_from_local_server(data->url,
		                                  app_http_download_callback);

	case APP_FIRMWARE_READY:
		error = wasm_replace_app(wasm_file, wasm_file_index);
		printk("Done. Error: %d\n", error);
		wasm_file_index = 0;
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

	memcpy(wasm_file + wasm_file_index,
	       res->body_frag_start,
	       res->body_frag_len);
	wasm_file_index += res->body_frag_len;

	if (final_data == HTTP_DATA_FINAL) {
		struct appq data = { .id = APP_FIRMWARE_READY };
		app_send(&data);
	}
}
