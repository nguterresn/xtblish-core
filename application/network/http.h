#pragma once
#include <zephyr/net/http/client.h>

// Note: Unsure about the size of this array. Keeping it as 4kB as most flash
// sectors are erased by sector of 4kB.
#define HTTP_BLOCK_SIZE 4096

typedef void (*http_res_handle_cb)(struct http_response*, enum http_final_call);

int  http_init();
void http_thread(void* arg1, void* arg2, void* arg3);
int  http_get_from_local_server(const char* query, http_res_handle_cb callback);
