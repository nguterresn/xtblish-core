#include "wifi.h"
#include "cred.h"
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

#if !defined(WIFI_SSID) || !defined(WIFI_PSK)
#error "Define the WiFi credentials!"
#endif

extern struct sys_heap _system_heap;
struct k_sem           new_ip;

static struct net_mgmt_event_callback event_wifi = { 0 };
static struct net_mgmt_event_callback event_net  = { 0 };

static struct sys_memory_stats stats;

static void mgmt_event_net_handler(struct net_mgmt_event_callback* event_wifi,
                                   uint32_t mgmt_event, struct net_if* iface);

static void mgmt_event_wifi_handler(struct net_mgmt_event_callback* event_wifi,
                                    uint32_t mgmt_event, struct net_if* iface);

int wifi_connect(void)
{
	struct net_if* iface = net_if_get_default();

	struct wifi_connect_req_params wifi_params = {
		.ssid        = WIFI_SSID,
		.psk         = WIFI_PSK,
		.ssid_length = sizeof(WIFI_SSID),
		.psk_length  = sizeof(WIFI_PSK),
		.channel     = WIFI_CHANNEL_ANY,
		.security    = WIFI_SECURITY_TYPE_PSK,
		.band        = WIFI_FREQ_BAND_2_4_GHZ,
		.mfp         = WIFI_MFP_OPTIONAL
	};

	printk("Connecting to SSID: %s Interface: %s \n",
	       wifi_params.ssid,
	       iface->config.name);

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT,
	             iface,
	             &wifi_params,
	             sizeof(struct wifi_connect_req_params))) {
		printk("Failed to connected to SSID!\n");
		return -1;
	}

	return 0;
}

static void mgmt_event_net_handler(struct net_mgmt_event_callback* event_wifi,
                                   uint32_t mgmt_event, struct net_if* iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	switch (mgmt_event) {
	case NET_EVENT_IF_UP:
	case NET_EVENT_IF_DOWN:
	case NET_EVENT_L4_CONNECTED:
	case NET_EVENT_L4_DISCONNECTED:
	case NET_EVENT_DNS_SERVER_ADD:
	case NET_EVENT_IPV4_ADDR_DEL:
		break;

	case NET_EVENT_IPV4_ADDR_ADD:
		k_sem_give(&new_ip);
		break;

	default:
		printk("Other wifi event %d\n", mgmt_event);
		break;
	}
}

static void mgmt_event_wifi_handler(struct net_mgmt_event_callback* event_wifi,
                                    uint32_t mgmt_event, struct net_if* iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		printk("Wifi Connected to %s\n", WIFI_SSID);
		break;

	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		printk("Disconnected from %s\n", WIFI_SSID);
		break;

	default:
		printk("Other wifi event %d\n", mgmt_event);
		break;
	}
}

int wifi_init()
{
	int error = k_sem_init(&new_ip, 0, 1);
	if (error) {
		return error;
	}

	net_mgmt_init_event_callback(&event_net,
	                             mgmt_event_net_handler,
	                             NET_EVENT_IPV4_ADDR_ADD |
	                                 NET_EVENT_IPV4_ADDR_DEL);

	net_mgmt_init_event_callback(&event_wifi,
	                             mgmt_event_wifi_handler,
	                             NET_EVENT_WIFI_CONNECT_RESULT |
	                                 NET_EVENT_WIFI_DISCONNECT_RESULT);

	net_mgmt_add_event_callback(&event_net);
	net_mgmt_add_event_callback(&event_wifi);

	return 0;
}

