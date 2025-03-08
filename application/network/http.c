#include "http.h"
#include "dns.h"
#include "socket.h"
#include <errno.h>
#include <string.h>
#include <sys/_stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/hostname.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/printk.h>

#define HTTP_PORT        8080
#define EXAMPLE_hostname "http://example.com"
#define DNS_TIMEOUT      (3 * MSEC_PER_SEC)

static void http_response_cb(struct http_response* res,
                             enum http_final_call final_data, void* user_data);
static int  http_get_from_address(struct http_ctx* http_ctx,
                                  const char* hostname, const char* query);

static uint8_t            recv_buf[512] = { 0 };
static struct sockaddr_in ota_addr      = { .sin_addr = {
	                                            .s4_addr = { 192, 168, 0, 140 } } };

static struct k_sem   http_sem;
static struct dns_ctx dns_ctx = { 0 };

extern struct k_sem new_ip;

int http_init()
{
	int error = dns_init(&dns_ctx);
	if (error) {
		return error;
	}

	return k_sem_init(&http_sem, 0, 1);
}

static void http_init_request(struct http_ctx* http_ctx,
                              enum http_method method, const char* hostname,
                              const char* query, http_response_cb_t response_cb)
{
	struct http_request* req = &http_ctx->request;

	req->method       = method;
	req->host         = hostname;
	req->url          = query;
	req->protocol     = "HTTP/1.1";
	req->response     = response_cb;
	req->recv_buf     = recv_buf;
	req->recv_buf_len = sizeof(recv_buf);

	memset(recv_buf, 0, sizeof(recv_buf));
	http_ctx->socket = -1;
}

/**
 * @brief HTTP GET from local OTA Server address 192.168.0.140
 *
 * @param query e.g. '/status'
 * @return int
 */
int http_get_from_local_server(struct http_ctx* http_ctx, const char* query)
{
	int error = 0;

	http_init_request(http_ctx,
	                  HTTP_GET,
	                  "192.168.0.140",
	                  query,
	                  http_response_cb);

	error = socket_connect(&http_ctx->socket,
	                       SOCK_STREAM,
	                       IPPROTO_TCP,
	                       (struct sockaddr_in*)&ota_addr,
	                       HTTP_PORT);
	if (error < 0) {
		return error;
	}

	error = http_client_req(http_ctx->socket,
	                        &http_ctx->request,
	                        3000,
	                        http_ctx);
	if (error < 0) {
		printk("Failed to perform an HTTP request! error %d\n", error);
		goto close_socket;
	}

	// Note: The following semaphore might be redundant... Check this out later on.
	if (k_sem_take(&http_sem, K_MSEC(3000)) < 0) {
		error = -ENOLCK;
		printk("Failed to get a HTTP response!\n");
	}

close_socket:
	zsock_close(http_ctx->socket);
	return error;
}

static int http_get_from_address(struct http_ctx* http_ctx,
                                 const char* hostname, const char* query)
{
	int error;

	http_init_request(http_ctx, HTTP_GET, hostname, query, http_response_cb);

	error = dns_resolve_ipv4(hostname, &dns_ctx);
	if (error) {
		printk("Failed to resolve DNS %d\n", error);
		return error;
	}

	error = socket_connect(&http_ctx->socket,
	                       SOCK_STREAM,
	                       IPPROTO_TCP,
	                       (struct sockaddr_in*)&dns_ctx.info.ai_addr,
	                       HTTP_PORT);
	if (error < 0) {
		return error;
	}

	error = http_client_req(http_ctx->socket, &http_ctx->request, 2000, NULL);
	if (error < 0) {
		printk("Failed to perform an HTTP request! error %d\n", error);
		goto close_socket;
	}

	if (k_sem_take(&http_sem, K_MSEC(1000)) < 0) {
		error = -ENOLCK;
		printk("Failed to get a HTTP response!\n");
		goto close_socket;
	}

close_socket:
	zsock_close(http_ctx->socket);
	return error;
}

static void http_response_cb(struct http_response* res,
                             enum http_final_call final_data, void* user_data)
{
	if (final_data == HTTP_DATA_MORE) {
		printk("** HTTP_DATA_MORE ** (%u bytes)\n\n", (uint32_t)res->data_len);
	}
	else if (final_data == HTTP_DATA_FINAL) {
		printk("** HTTP_DATA_FINAL **(%u bytes)\n\n", (uint32_t)res->data_len);
	}

	struct http_ctx* http_ctx = (struct http_ctx*)user_data;

	http_ctx->callback(&http_ctx->request, res, final_data);
	k_sem_give(&http_sem);
}
