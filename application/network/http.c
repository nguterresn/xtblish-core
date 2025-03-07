#include "http.h"
#include <errno.h>
#include <sys/_stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/sys/printk.h>
#include <zephyr/data/json.h>

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

#define HTTP_PORT        8080
#define EXAMPLE_hostname "http://example.com"
#define DNS_TIMEOUT      (3 * MSEC_PER_SEC)

static int connect_tcp_socket(sa_family_t family, struct sockaddr_in* addr,
                              int port, int* sock, socklen_t addr_len);

static int  dns_resolve_ipv4(const char* hostname);
static void dns_result_cb(enum dns_resolve_status status,
                          struct dns_addrinfo* info, void* user_data);
static void http_response_cb(struct http_response* rsp,
                             enum http_final_call final_data, void* user_data);

static struct dns_addrinfo dns_info = { 0 };
static struct sockaddr_in  ota_addr = { .sin_addr = {
	                                        .s4_addr = { 192, 168, 0, 140 } } };

static struct k_sem dns_sem;
static struct k_sem http_sem;

static int http_get_from_ip(struct sockaddr_in* addr, const char* query);
static int http_get_from_address(const char* hostname, const char* query);

int http_init()
{
	int error = k_sem_init(&dns_sem, 0, 1);
	if (error) {
		return error;
	}

	return k_sem_init(&http_sem, 0, 1);
}

void http_thread(void* arg1, void* arg2, void* arg3)
{
	printk("Start 'http_thread'\n");

	for (;;) {
		k_sleep(K_SECONDS(10));

		printk("\n\nAttempt to perform a HTTP GET request.\n\n");
		http_get_from_ip(&ota_addr, "/status");

		k_sleep(K_SECONDS(1000));
	}
}

static int http_get_from_ip(struct sockaddr_in* addr, const char* query)
{
	struct http_request req = { 0 };
	static uint8_t      recv_buf[512];
	static int          socket = -1;
	int                 error;

	req.method       = HTTP_GET;
	req.url          = query;
	req.host         = "192.168.0.140";
	req.protocol     = "HTTP/1.1";
	req.response     = http_response_cb;
	req.recv_buf     = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	error = connect_tcp_socket(AF_INET,
	                           (struct sockaddr_in*)addr,
	                           HTTP_PORT,
	                           &socket,
	                           sizeof(struct sockaddr_in));
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
	struct http_request req = { 0 };
	static uint8_t      recv_buf[512];
	static int          socket = -1;
	int                 error;

	req.method       = HTTP_GET;
	req.url          = query;
	req.host         = hostname;
	req.protocol     = "HTTP/1.1";
	req.response     = http_response_cb;
	req.recv_buf     = recv_buf;
	req.recv_buf_len = sizeof(recv_buf);

	error = dns_resolve_ipv4(hostname);
	if (error) {
		return error;
	}

	error = k_sem_take(&dns_sem, K_SECONDS(5));
	if (error < 0) {
		printk("Failed to resolve DNS [%d]!\n", error);
		return -ENOLCK;
	}

	error = connect_tcp_socket(AF_INET,
	                           (struct sockaddr_in*)&dns_info.ai_addr,
	                           HTTP_PORT,
	                           &socket,
	                           sizeof(struct sockaddr_in));
	if (error < 0) {
		return error;
	}

	error = http_client_req(socket, &req, 2000, NULL);
	if (error < 0) {
		printk("Failed to perform an HTTP request! error %d\n", error);
		goto close_socket;
	}

	if (k_sem_take(&http_sem, K_SECONDS(1)) < 0) {
		error = -ENOLCK;
		printk("Failed to get a HTTP response!\n");
		goto close_socket;
	}

close_socket:
	zsock_close(socket);
	return error;
}

static int connect_tcp_socket(sa_family_t family, struct sockaddr_in* addr,
                              int port, int* sock, socklen_t addr_len)
{
	addr->sin_family = AF_INET;
	addr->sin_port   = htons(port);

	// printk("\n\nADDR [%03d:%03d:%03d:%03d] PORT %d FAMILY %d\n\n",
	//        addr->sin_addr.s4_addr[0],
	//        addr->sin_addr.s4_addr[1],
	//        addr->sin_addr.s4_addr[2],
	//        addr->sin_addr.s4_addr[3],
	//        addr->sin_port,
	//        addr->sin_family);

	*sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*sock < 0) {
		printk("Failed to create %s HTTP socket (%d)", "IPv4", -errno);
		return -errno;
	}

	int ret = zsock_connect(*sock, (struct sockaddr*)addr, addr_len);
	if (ret < 0) {
		printk("Cannot connect to %s remote (%d)", "IPv4", -errno);
		zsock_close(*sock);
		*sock = -1;
		return -errno;
	}

	return 0;
}

static int dns_resolve_ipv4(const char* hostname)
{
	static uint16_t dns_id = 0;

	int ret = dns_get_addr_info(hostname,
	                            DNS_QUERY_TYPE_A,
	                            &dns_id,
	                            dns_result_cb,
	                            (void*)hostname,
	                            DNS_TIMEOUT);
	ret < 0 ? printk("Cannot resolve IPv4 address (%d)\n", ret)
	        : printk("DNS id %u\n", dns_id);
	return ret;
}

static void dns_result_cb(enum dns_resolve_status status,
                          struct dns_addrinfo* info, void* user_data)
{
	char  hr_addr[NET_IPV4_ADDR_LEN];
	void* addr;

	printk("DNS status %d\n", status);

	if (status != DNS_EAI_INPROGRESS || !info || info->ai_family != AF_INET) {
		return;
	}

	memcpy(&dns_info, info, sizeof(struct dns_addrinfo));
	printk("[%s] %s address: %s",
	       __func__,
	       user_data ? (char*)user_data : "<null>",
	       net_addr_ntop(info->ai_family, addr, hr_addr, sizeof(hr_addr)));

	k_sem_give(&dns_sem);
}

static void http_response_cb(struct http_response* rsp,
                             enum http_final_call final_data, void* user_data)
{
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

	k_sem_give(&http_sem);
}
