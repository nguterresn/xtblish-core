#include "wifi.h"
#include "cred.h"
#include <sys/_intsup.h>
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

static void mgmt_event_handler(struct net_mgmt_event_callback* cb,
                               uint32_t mgmt_event, struct net_if* iface);

static struct net_mgmt_event_callback cb       = { 0 };
static struct net_mgmt_event_callback other_cb = { 0 };

static struct sys_memory_stats stats;

struct k_sem net_sem;

static int wifi_connect(void)
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

	printk("Connecting to SSID: %s ...\n", wifi_params.ssid);

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT,
	             iface,
	             &wifi_params,
	             sizeof(struct wifi_connect_req_params))) {
		printk("Failed to connected to SSID!\n");
		return -1;
	}

	return 0;
}

static int wifi_init()
{
	int error = 0;

	error = k_sem_init(&net_sem, 0, 1);
	if (error) {
		return error;
	}

	net_mgmt_init_event_callback(&cb,
	                             mgmt_event_handler,
	                             NET_EVENT_WIFI_CONNECT_RESULT |
	                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_init_event_callback(&other_cb,
	                             mgmt_event_handler,
	                             NET_EVENT_IPV4_ADDR_ADD);

	net_mgmt_add_event_callback(&cb);
	net_mgmt_add_event_callback(&other_cb);

	return 0;
}

void wifi_thread(void* arg1, void* arg2, void* arg3)
{
	int error = wifi_init();
	if (error) {
		k_thread_abort(NULL);
	}

	error = wifi_connect();
	if (error) {
		k_thread_abort(NULL);
	}

	printk("Start 'wifi_thread'\n");

	for (;;) {
		k_yield();
	}
}

static void mgmt_event_handler(struct net_mgmt_event_callback* cb,
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

	case NET_EVENT_L4_CONNECTED:
		printk("(L4) Connected %s\n", WIFI_SSID);
		break;

	case NET_EVENT_IPV4_ADDR_ADD:
		k_sem_give(&net_sem);
		break;

	default:
		printk("Other wifi event %d\n", mgmt_event);
		break;
	}
}
