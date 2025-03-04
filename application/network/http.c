#include "http.h"
#include <errno.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/http/client.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/sys/printk.h>

#define HTTP_PORT        8080
#define EXAMPLE_hostname "http://example.com"
#define DNS_TIMEOUT      (2 * MSEC_PER_SEC)

static int  http_init(void);
static int  connect_socket(sa_family_t family, const char* server, int port,
                           int* sock, struct sockaddr* addr, socklen_t addr_len);
static int  dns_resolve_ipv4(const char* hostname);
static void dns_result_cb(enum dns_resolve_status status,
                          struct dns_addrinfo* info, void* user_data);
static void http_response_cb(struct http_response* rsp,
                             enum http_final_call final_data, void* user_data);

static struct dns_addrinfo dns_info = { 0 };

static struct k_sem dns_sem;
static struct k_sem http_sem;

void http_thread(void* arg1, void* arg2, void* arg3)
{
	int error = http_init();
	if (error) {
		k_thread_abort(NULL);
	}

	printk("Start 'http_thread'\n");

	for (;;) {
		k_sleep(K_SECONDS(10));

		printk("Attempt to perform a HTTP GET request.\n");
		http_get("www.example.com", "/");
	}
}

int http_get(const char* hostname, const char* query)
{
	struct sockaddr_in  addr4  = { 0 };
	static int          socket = -1;
	struct http_request req    = { 0 };
	static uint8_t      recv_buf[512];
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

	if (k_sem_take(&dns_sem, K_SECONDS(3))) {
		printk("Failed to rsolve DNS!\n");
		return -ENOLCK;
	}

	error = connect_socket(AF_INET,
	                       dns_info.ai_addr.data,
	                       HTTP_PORT,
	                       &socket,
	                       (struct sockaddr*)&addr4,
	                       sizeof(addr4));
	if (error < 0) {
		return error;
	}

	error = http_client_req(socket, &req, 3000, NULL);
	if (error < 0) {
		printk("Failed to perform an HTTP request!\n");
		goto close_socket;
	}

	if (k_sem_take(&http_sem, K_SECONDS(3)) < 0) {
		error = -ENOLCK;
		printk("Failed to get a HTTP response!\n");
		goto close_socket;
	}

close_socket:
	zsock_close(socket);
	return error;
}

static int http_init(void)
{
	int error = k_sem_init(&dns_sem, 0, 1);
	if (error) {
		return error;
	}

	return k_sem_init(&http_sem, 0, 1);
}

static int connect_socket(sa_family_t family, const char* server, int port,
                          int* sock, struct sockaddr* addr, socklen_t addr_len)
{
	memset(addr, 0, addr_len);

	net_sin(addr)->sin_family = AF_INET;
	net_sin(addr)->sin_port   = htons(port);
	inet_pton(AF_INET, server, &net_sin(addr)->sin_addr);

	*sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (*sock < 0) {
		printk("Failed to create %s HTTP socket (%d)", "IPv4", -errno);
		return -errno;
	}

	int ret = connect(*sock, addr, addr_len);
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
	ret < 0 ? printk("Cannot resolve IPv4 address (%d)", ret)
	        : printk("DNS id %u", dns_id);
	return ret;
}

static void dns_result_cb(enum dns_resolve_status status,
                          struct dns_addrinfo* info, void* user_data)
{
	char  hr_addr[NET_IPV6_ADDR_LEN];
	char* hr_family;
	void* addr;

	switch (status) {
	case DNS_EAI_CANCELED:
		printk("DNS query was canceled");
		return;
	case DNS_EAI_FAIL:
		printk("DNS resolve failed");
		return;
	case DNS_EAI_NODATA:
		printk("Cannot resolve address");
		return;
	case DNS_EAI_ALLDONE:
		printk("DNS resolving finished");
		return;
	case DNS_EAI_INPROGRESS:
		break;
	default:
		printk("DNS resolving error (%d)", status);
		return;
	}

	if (!info) {
		return;
	}

	if (info->ai_family == AF_INET) {
		hr_family = "IPv4";
		addr      = &net_sin(&info->ai_addr)->sin_addr;
	}
	else {
		printk("Invalid IP address family %d", info->ai_family);
		return;
	}

	memcpy(&dns_info, info, sizeof(struct dns_addrinfo));
	printk("%s %s address: %s",
	       user_data ? (char*)user_data : "<null>",
	       hr_family,
	       net_addr_ntop(info->ai_family, addr, hr_addr, sizeof(hr_addr)));

	k_sem_give(&dns_sem);
}

static void http_response_cb(struct http_response* rsp,
                             enum http_final_call final_data, void* user_data)
{
	if (final_data == HTTP_DATA_MORE) {
		printk("Partial data received (%u bytes)\n", (uint32_t)rsp->data_len);
	}
	else if (final_data == HTTP_DATA_FINAL) {
		printk("All the data received (%u bytes)\n", (uint32_t)rsp->data_len);
	}

	printk("Response status %s\n", rsp->http_status);
	printk("Recv Buffer Length %zd\n", rsp->recv_buf_len);
	printk("\nData Rcv: %s\n", rsp->recv_buf);

	k_sem_give(&http_sem);
}
