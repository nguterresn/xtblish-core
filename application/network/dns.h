#pragma once
#include <zephyr/net/dns_resolve.h>

struct dns_ctx {
	uint16_t            id;
	struct dns_addrinfo info;
	struct k_sem        sem;
};

int dns_init(struct dns_ctx* dns_ctx);
int dns_resolve_ipv4(const char* hostname, struct dns_ctx* dns_ctx);
