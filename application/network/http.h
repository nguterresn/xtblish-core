#pragma once
#include <zephyr/net/http/client.h>

struct http_ctx {
	struct http_request request;
	int                 socket;
	void (*callback)(struct http_request* req, struct http_response* res,
	                 enum http_final_call final_data);
};

int  http_init();
void http_thread(void* arg1, void* arg2, void* arg3);
int  http_get_from_local_server(struct http_ctx* http_ctx, const char* query);
