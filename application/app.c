#include "app.h"
#include "wasm/wasm.h"
#include "http.h"
#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <zephyr/net/http/client.h>
#include <zephyr/sys/printk.h>

extern struct k_sem    new_ip;
extern struct sys_heap _system_heap;
extern uint8_t         wasm_file[WASM_FILE_MAX_SIZE * 4];
static uint32_t        wasm_file_index = 0;

static void app_http_status_callback(struct http_response* res,
                                     enum http_final_call  final_data);
static void app_http_download_callback(struct http_response* res,
                                       enum http_final_call  final_data);

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

bool download_new_app = false;

int app_init()
{
}

void app_thread(void* arg1, void* arg2, void* arg3)
{
	printk("Start 'app_thread'\n");

	k_sem_take(&new_ip, K_FOREVER);

	printk("\n\n** START WASM_RUNTIME ! **\n");

	__ASSERT(wasm_boot_app(false) == 0, "Failed to boot app\n");
	sys_heap_runtime_stats_get(&_system_heap, &stats);

	printk("\n\nINFO: Allocated Heap = %zu\n", stats.allocated_bytes);
	printk("INFO: Free Heap = %zu\n", stats.free_bytes);
	printk("INFO: Max Allocated Heap = %zu\n\n\n", stats.max_allocated_bytes);

	/// ------ ///

	int error = 0;

	for (;;) {
		printk("\n\nPeriodically (30s) check for status...\n\n");
		error = http_get_from_local_server("/status", app_http_status_callback);

		if (error >= 0 && download_new_app) {
			printk("Download File....\n");
			memset(wasm_file, 0, sizeof(wasm_file));
			error = http_get_from_local_server("/download",
			                                   app_http_download_callback);
			if (error >= 0) {
				printk("\nReplacing app... with %d bytes\n", wasm_file_index);
			error = wasm_replace_app(wasm_file, wasm_file_index);
				printk("Done. Error: %d\n", error);
				wasm_file_index = 0;
			}
		}

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
			if (status.file_exists) {
				printk("File exists!\n");
				download_new_app = true;
			}
		}
	}
}

static void app_http_download_callback(struct http_response* res,
                                       enum http_final_call  final_data)
{
	printk("Response code: %u body len %d\n",
	       res->http_status_code,
	       (int)res->body_frag_len);

	if (200 <= res->http_status_code && res->http_status_code <= 299 &&
	    res->body_found) {
		memcpy(wasm_file + wasm_file_index,
		       res->body_frag_start,
		       res->body_frag_len);
		wasm_file_index += res->body_frag_len;

		if (final_data == HTTP_DATA_FINAL) {
			download_new_app = false;
			printk("New file downloaded size: %u\n", wasm_file_index);
		}
	}
}
