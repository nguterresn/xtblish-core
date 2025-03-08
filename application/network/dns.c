#include "dns.h"
#include <zephyr/kernel.h>

static void dns_result_cb(enum dns_resolve_status status,
                          struct dns_addrinfo* info, void* user_data)
{
	struct dns_ctx* dns_ctx = (struct dns_ctx*)user_data;
	char            hr_addr[NET_IPV4_ADDR_LEN];
	void*           addr;

	printk("DNS status %d\n", status);

	if (status != DNS_EAI_INPROGRESS || !info || info->ai_family != AF_INET) {
		return;
	}

	memcpy(&dns_ctx->info, info, sizeof(struct dns_addrinfo));
	printk("[%s] %s address: %s",
	       __func__,
	       user_data ? (char*)user_data : "<null>",
	       net_addr_ntop(info->ai_family, addr, hr_addr, sizeof(hr_addr)));

	k_sem_give(&dns_ctx->sem);
}

int dns_init(struct dns_ctx* dns_ctx)
{
	if (!dns_ctx) {
		return -EINVAL;
	}

	return k_sem_init(&dns_ctx->sem, 0, 1);
}

int dns_resolve_ipv4(const char* hostname, struct dns_ctx* dns_ctx)
{
	if (!dns_ctx) {
		return -EINVAL;
	}

	int error = dns_get_addr_info(hostname,
	                              DNS_QUERY_TYPE_A,
	                              &dns_ctx->id,
	                              dns_result_cb,
	                              (struct dns_ctx*)&dns_ctx,
	                              1000);

	error < 0 ? printk("Cannot resolve IPv4 address (%d)\n", error)
	          : printk("DNS id %u\n", dns_ctx->id);

	if (error < 0) {
		return error;
	}

	if (k_sem_take(&dns_ctx->sem, K_MSEC(1000)) < 0) {
		error = -ENOLCK;
	}

	return error;
}
