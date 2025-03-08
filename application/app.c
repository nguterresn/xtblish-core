#include "app.h"
#include "wasm/wasm.h"
#include "http.h"
#include <zephyr/kernel.h>
#include <zephyr/data/json.h>

extern struct k_sem new_ip;

static void app_http_status_callback(struct http_response* res,
                                     enum http_final_call  final_data);

extern struct sys_heap         _system_heap;
static struct sys_memory_stats stats;

struct http_status_response {
	bool     file_exists;
	char*    file_name;
	uint32_t file_size;
};

static const struct json_obj_descr http_status_response_descrp[] = {
	JSON_OBJ_DESCR_PRIM(struct http_status_response, file_exists,
	                    JSON_TOK_TRUE),
	JSON_OBJ_DESCR_PRIM(struct http_status_response, file_name,
	                    JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct http_status_response, file_size,
	                    JSON_TOK_NUMBER),
};

// (!) Important Note:
// Due to the way the Wifi manages the heap, it should be correclty initialized
// before any runtime is set. Wifi heap can not allocate memory in an external RAM.

struct k_sem new_wasm_app;

int app_init()
{
	return k_sem_init(&new_wasm_app, 0, 1);
}

void app_thread(void* arg1, void* arg2, void* arg3)
{
	printk("Start 'app_thread'\n");

	k_sem_take(&new_ip, K_FOREVER);

	printk("\n\n** START WASM_RUNTIME ! **\n");

	__ASSERT(wasm_boot_app() == 0, "Failed to boot app\n");
	sys_heap_runtime_stats_get(&_system_heap, &stats);

	printk("\n\n");
	printk("INFO: Allocated Heap = %zu\n", stats.allocated_bytes);
	printk("INFO: Free Heap = %zu\n", stats.free_bytes);
	printk("INFO: Max Allocated Heap = %zu\n", stats.max_allocated_bytes);
	printk("\n\n");

	for (;;) {
		printk("\n\nPeriodically (30s) check for status...\n\n");
		http_get_from_local_server("/status", app_http_status_callback);

		k_sleep(K_SECONDS(30));
	}
}

static void app_http_status_callback(struct http_response* res,
                                     enum http_final_call  final_data)
{
	struct http_status_response status = { 0 };

	printk("Response code: %u\n", res->http_status_code);

	if (200 <= res->http_status_code && res->http_status_code <= 299) {
		int error = json_obj_parse(res->body_frag_start,
		                           res->content_length,
		                           http_status_response_descrp,
		                           ARRAY_SIZE(http_status_response_descrp),
		                           &status);
		if (error < 0) {
			printk("\n [%d] Data Rcv: \n%s\n", error, res->recv_buf);
		}
		else {
			printk("\nJSON Data: "
			       "\nfile_exists=%u\nfile_name='%s'\nfile_size=%u\n",
			       status.file_exists,
			       status.file_name,
			       status.file_size);
		}
	}
}
