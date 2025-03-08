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
#include <zephyr/net/http/client.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/printk.h>
#include <zephyr/data/json.h>

#define HTTP_PORT        8080
#define EXAMPLE_hostname "http://example.com"
#define DNS_TIMEOUT      (3 * MSEC_PER_SEC)

static void http_response_cb(struct http_response* rsp,
                             enum http_final_call final_data, void* user_data);
static int  http_get_from_ip(struct sockaddr_in* addr, const char* query);
static int  http_get_from_address(const char* hostname, const char* query);

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

void http_thread(void* arg1, void* arg2, void* arg3)
{
	printk("Start 'http_thread'\n");

	if (k_sem_take(&new_ip, K_SECONDS(20)) < 0) {
		__ASSERT(0, "Failed to a new IP Address!\n");
	}

	for (;;) {
		printk("\n\nPeriodically (30s) check for status...\n\n");
		http_get_from_ip(&ota_addr, "/status");

		k_sleep(K_SECONDS(30));
	}
}

static void http_init_request(struct http_request* req, enum http_method method,
                              const char* hostname, const char* query,
                              http_response_cb_t response_cb)
{
	req->method       = method;
	req->host         = hostname;
	req->url          = query;
	req->protocol     = "HTTP/1.1";
	req->response     = response_cb;
	req->recv_buf     = recv_buf;
	req->recv_buf_len = sizeof(recv_buf);

	memset(recv_buf, 0, sizeof(recv_buf));
}

static int http_get_from_ip(struct sockaddr_in* addr, const char* query)
{
	struct http_request req    = { 0 };
	static int          socket = -1;
	int                 error;

	http_init_request(&req, HTTP_GET, "192.168.0.140", query, http_response_cb);

	error = socket_connect(&socket,
	                       SOCK_STREAM,
	                       IPPROTO_TCP,
	                       (struct sockaddr_in*)addr,
	                       HTTP_PORT);
	if (error < 0) {
		return error;
	}

	error = http_client_req(socket, &req, 3000, NULL);
	if (error < 0) {
		printk("Failed to perform an HTTP request! error %d\n", error);
		goto close_socket;
	}

	// Note: The following semaphore might be redundant... Check this out later on.
	if (k_sem_take(&http_sem, K_SECONDS(1)) < 0) {
		error = -ENOLCK;
		printk("Failed to get a HTTP response!\n");
		goto close_socket;
	}

close_socket:
	zsock_close(socket);
	return error;
}

static int http_get_from_address(const char* hostname, const char* query)
{
	struct http_request req    = { 0 };
	static int          socket = -1;
	int                 error;

	http_init_request(&req, HTTP_GET, hostname, query, http_response_cb);

	error = dns_resolve_ipv4(hostname, &dns_ctx);
	if (error) {
		printk("Failed to resolve DNS %d\n", error);
		return error;
	}

	error = socket_connect(&socket,
	                       SOCK_STREAM,
	                       IPPROTO_TCP,
	                       (struct sockaddr_in*)&dns_ctx.info.ai_addr,
	                       HTTP_PORT);
	if (error < 0) {
		return error;
	}

	error = http_client_req(socket, &req, 2000, NULL);
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
	zsock_close(socket);
	return error;
}

static void http_response_cb(struct http_response* rsp,
                             enum http_final_call final_data, void* user_data)
{
	k_sem_give(&http_sem);

	if (final_data == HTTP_DATA_MORE) {
		printk("** HTTP_DATA_MORE ** (%u bytes)\n\n", (uint32_t)rsp->data_len);
	}
	else if (final_data == HTTP_DATA_FINAL) {
		printk("** HTTP_DATA_FINAL **(%u bytes)\n\n", (uint32_t)rsp->data_len);
	}

	struct http_status_response status = { 0 };

	printk("Response code: %u\n", rsp->http_status_code);

	if (200 <= rsp->http_status_code && rsp->http_status_code <= 299) {
		int error = json_obj_parse(rsp->body_frag_start,
		                           rsp->content_length,
		                           http_status_response_descrp,
		                           ARRAY_SIZE(http_status_response_descrp),
		                           &status);
		if (error < 0) {
			printk("\n [%d] Data Rcv: \n%s\n", error, rsp->recv_buf);
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
