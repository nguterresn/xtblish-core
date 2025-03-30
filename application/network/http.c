#include "http.h"
#include "dns.h"
#include "socket.h"
#include "server.h"
#include <errno.h>
#include <string.h>
#include <sys/_stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/hostname.h>
#include <zephyr/net/http/client.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/printk.h>

static void http_response_cb(struct http_response* res,
                             enum http_final_call final_data, void* user_data);
static int  http_get_from_query(const char* hostname, const char* query,
                                http_res_handle_cb callback);

static uint8_t recv_buf[512] = { 0 };

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

static void http_init_request(struct http_request* req, enum http_method method,
                              const char* hostname, const char* query)
{
	req->method       = method;
	req->host         = hostname;
	req->url          = query;
	req->protocol     = SERVER_HTTP_PROTOCOL;
	req->response     = http_response_cb;
	req->recv_buf     = recv_buf;
	req->recv_buf_len = sizeof(recv_buf);

	memset(recv_buf, 0, sizeof(recv_buf));
}

/**
 * @brief HTTP GET from local OTA Server address 192.168.0.140
 *
 * @param query e.g. '/status'
 * @return int error if <0, 0>= success
 */
int http_get_from_local_server(const char* query, http_res_handle_cb callback)
{
	int                 error  = 0;
	int                 socket = -1;
	struct http_request req    = { 0 };

	http_init_request(&req, HTTP_GET, SERVER_IP, query);

	error = socket_connect(&socket,
	                       SOCK_STREAM,
	                       IPPROTO_TCP,
	                       SERVER_IP,
	                       SERVER_HTTP_PORT);
	if (error < 0) {
		return error;
	}

	error = http_client_req(socket, &req, SERVER_HTTP_TIMEOUT, callback);
	if (error < 0) {
		printk("Failed to perform an HTTP request! error %d\n", error);
		goto close_socket;
	}

	// Note: The following semaphore might be redundant... Check this out later on.
	if (k_sem_take(&http_sem, K_MSEC(SERVER_HTTP_TIMEOUT)) < 0) {
		error = -ENOLCK;
		printk("Failed to get a HTTP response!\n");
	}

close_socket:
	zsock_close(socket);
	return error;
}

static int http_get_from_query(const char* hostname, const char* query,
                               http_res_handle_cb callback)
{
	int                 error  = 0;
	int                 socket = -1;
	struct http_request req    = { 0 };

	http_init_request(&req, HTTP_GET, hostname, query);

	error = dns_resolve_ipv4(hostname, &dns_ctx);
	if (error) {
		printk("Failed to resolve DNS %d\n", error);
		return error;
	}

	error = socket_connect(&socket,
	                       SOCK_STREAM,
	                       IPPROTO_TCP,
	                       (struct sockaddr_in*)&dns_ctx.info.ai_addr,
	                       SERVER_HTTP_PORT);
	if (error < 0) {
		return error;
	}

	error = http_client_req(socket, &req, SERVER_HTTP_TIMEOUT, callback);
	if (error < 0) {
		printk("Failed to perform an HTTP request! error %d\n", error);
		goto close_socket;
	}

	if (k_sem_take(&http_sem, K_MSEC(SERVER_HTTP_TIMEOUT)) < 0) {
		error = -ENOLCK;
		printk("Failed to get a HTTP response!\n");
		goto close_socket;
	}

close_socket:
	zsock_close(socket);
	return error;
}

static void http_response_cb(struct http_response* res,
                             enum http_final_call final_data, void* user_data)
{
	if (final_data == HTTP_DATA_MORE) {
		printk("** HTTP_DATA_MORE ** (%u bytes)\n\n", (uint32_t)res->data_len);
	}
	else if (final_data == HTTP_DATA_FINAL) {
		printk("** HTTP_DATA_FINAL ** (%u bytes)\n\n", (uint32_t)res->data_len);
	}

	http_res_handle_cb callback = user_data;
	if (!callback) {
		printk("No callback attached to the response!\n");
		return;
	}

	callback(res, final_data);
	k_sem_give(&http_sem);
}
