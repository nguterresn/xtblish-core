#include "wifi.h"
#include "cred.h"
#include <sys/_intsup.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/sys/printk.h>

#if !defined(WIFI_SSID) || !defined(WIFI_PSK)
#error "Define the WiFi credentials!"
#endif

static void wifi_event_handler(struct net_mgmt_event_callback* cb,
                               uint32_t mgmt_event, struct net_if* iface);

static struct net_mgmt_event_callback connection_cb;
static struct net_mgmt_event_callback ipv4_cb;
static struct k_sem                   connection_sem;
static struct k_sem                   ipv4_sem;

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

	net_mgmt_init_event_callback(&connection_cb,
	                             wifi_event_handler,
	                             NET_EVENT_WIFI_CONNECT_RESULT |
	                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
	net_mgmt_init_event_callback(&ipv4_cb,
	                             wifi_event_handler,
	                             NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&connection_cb);
	net_mgmt_add_event_callback(&ipv4_cb);

	return 0;
}

int wifi_connect(void)
{
	struct net_if*                 iface       = net_if_get_default();
	struct wifi_connect_req_params wifi_params = { 0 };

	wifi_params.ssid        = WIFI_SSID;
	wifi_params.psk         = WIFI_PSK;
	wifi_params.ssid_length = strlen(WIFI_SSID);
	wifi_params.psk_length  = strlen(WIFI_PSK);
	wifi_params.channel     = WIFI_CHANNEL_ANY;
	wifi_params.security    = WIFI_SECURITY_TYPE_PSK;
	wifi_params.band        = WIFI_FREQ_BAND_2_4_GHZ;
	wifi_params.mfp         = WIFI_MFP_OPTIONAL;

	printk("Connecting to SSID: %s\n", wifi_params.ssid);

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT,
	             iface,
	             &wifi_params,
	             sizeof(struct wifi_connect_req_params))) {
		printk("Failed to connected to SSID!\n");
		return -1;
	}

	if (!k_sem_take(&connection_sem, K_SECONDS(10))) {
		// If WiFi hasn't connected, quit.
		printk("Couldn't connect Wifi\n");
		return -ENOLCK;
	}

	if (!k_sem_take(&ipv4_sem, K_SECONDS(10))) {
		// If WiFi hasn't connected, quit.
		printk("Couldn't get an IP\n");
		return -ENOLCK;
	}

	return 0;
}

static void wifi_event_handler(struct net_mgmt_event_callback* cb,
                               uint32_t mgmt_event, struct net_if* iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		printk("Connected to %s\n", WIFI_SSID);
		k_sem_give(&connection_sem);
		break;

	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		printk("Disconnected from %s\n", WIFI_SSID);
		break;

	case NET_EVENT_IPV4_ADDR_ADD:
		printk("Interface name: %s\n", iface->config.name);
		printk("IPv4 address: %s\n",
		       net_addr_ntop(AF_INET,
		                     &iface->config.ip.ipv4->unicast[0].ipv4.address,
		                     buf,
		                     sizeof(buf)));
		k_sem_give(&ipv4_sem);
		break;

	default:
		printk("Other wifi event %d\n", mgmt_event);
		break;
	}
}
