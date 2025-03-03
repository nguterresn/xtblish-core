#include "wifi.h"
#include "cred.h"
#include <sys/_intsup.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/printk.h>

#if !defined(WIFI_SSID) || !defined(WIFI_PSK)
#error "Define the WiFi credentials!"
#endif

static void mgmt_event_handler(struct net_mgmt_event_callback* cb,
                               uint32_t mgmt_event, struct net_if* iface);
static void handle_wifi_connect_result(struct net_mgmt_event_callback* cb);
static void handle_ipv4_result(struct net_if* iface);

static struct net_mgmt_event_callback cb = { 0 };
struct k_sem                          connection_sem;
struct k_sem                          ipv4_sem;

static struct wifi_connect_req_params wifi_params = {
	.ssid        = WIFI_SSID,
	.psk         = WIFI_PSK,
	.ssid_length = sizeof(WIFI_SSID),
	.psk_length  = sizeof(WIFI_PSK),
	.channel     = WIFI_CHANNEL_ANY,
	.security    = WIFI_SECURITY_TYPE_PSK,
	.band        = WIFI_FREQ_BAND_2_4_GHZ,
	.mfp         = WIFI_MFP_OPTIONAL
};

int wifi_init()
{
	int error = 0;

	error = k_sem_init(&connection_sem, 0, 1);
	if (error) {
		return error;
	}

	error = k_sem_init(&ipv4_sem, 0, 1);
	if (error) {
		return error;
	}

	net_mgmt_init_event_callback(&cb,
	                             mgmt_event_handler,
	                             NET_EVENT_WIFI_CONNECT_RESULT |
	                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_add_event_callback(&cb);

	return 0;
}

void wifi_connect(void)
{
	struct net_if* iface = net_if_get_default();

	// wifi_params.ssid        = WIFI_SSID;
	// wifi_params.psk         = WIFI_PSK;
	// wifi_params.ssid_length = strlen(WIFI_SSID);
	// wifi_params.psk_length  = strlen(WIFI_PSK);
	// wifi_params.channel     = WIFI_CHANNEL_ANY;
	// wifi_params.security    = WIFI_SECURITY_TYPE_PSK;
	// wifi_params.band        = WIFI_FREQ_BAND_2_4_GHZ;
	// wifi_params.mfp         = WIFI_MFP_OPTIONAL;

	printk("Connecting to SSID: %s ...\n", wifi_params.ssid);

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT,
	             iface,
	             &wifi_params,
	             sizeof(struct wifi_connect_req_params))) {
		printk("Failed to connected to SSID!\n");
	}
}

static void mgmt_event_handler(struct net_mgmt_event_callback* cb,
                               uint32_t mgmt_event, struct net_if* iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		printk("Connected to %s\n", WIFI_SSID);
		handle_wifi_connect_result(cb);
		break;

	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		printk("Disconnected from %s\n", WIFI_SSID);
		break;

	case NET_EVENT_IPV4_ADDR_ADD:
		handle_ipv4_result(iface);
		break;

	default:
		printk("Other wifi event %d\n", mgmt_event);
		break;
	}
}

static void handle_wifi_connect_result(struct net_mgmt_event_callback* cb)
{
	const struct wifi_status* status = (const struct wifi_status*)cb->info;

	if (status->status) {
		printk("Connection request failed (%d)\n", status->status);
	}
	else {
		printk("Connected\n");
		k_sem_give(&connection_sem);
	}
}

static void handle_ipv4_result(struct net_if* iface)
{
	int i = 0;

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type != NET_ADDR_DHCP) {
			continue;
		}

		printk("IPv4 address: %s\n",
		       net_addr_ntop(AF_INET,
		                     &iface->config.ip.ipv4->unicast[i]
		                          .ipv4.address.in_addr,
		                     buf,
		                     sizeof(buf)));
	}

	k_sem_give(&ipv4_sem);
}
