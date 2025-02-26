#include "wifi.h"
#include "cred.h"
#include <sys/_intsup.h>
#include <zephyr/kernel.h>
#include <zephyr/net/wifi_mgmt.h>

#define NET_EVENT_WIFI_MASK                                               \
	(NET_EVENT_WIFI_CONNECT_RESULT | NET_EVENT_WIFI_DISCONNECT_RESULT |   \
	 NET_EVENT_WIFI_AP_ENABLE_RESULT | NET_EVENT_WIFI_AP_DISABLE_RESULT | \
	 NET_EVENT_WIFI_AP_STA_CONNECTED | NET_EVENT_WIFI_AP_STA_DISCONNECTED)

#if !defined(WIFI_SSID) || !defined(WIFI_PSK)
#error "Define the WiFi credentials!"
#endif

static void wifi_event_handler(struct net_mgmt_event_callback* cb, uint32_t mgmt_event,
                               struct net_if* iface);
static struct net_mgmt_event_callback cb = { 0 };

void wifi_init()
{
	net_mgmt_init_event_callback(&cb, wifi_event_handler, NET_EVENT_WIFI_MASK);
	net_mgmt_add_event_callback(&cb);
}

int wifi_connect(void)
{
	struct net_if* iface = net_if_get_default();
	if (iface == NULL) {
		printk("Network interface is not defined!\n");
		return -EPERM;
	}

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
		printk("WiFi Connection Request Failed\n");
	}
}

static void wifi_event_handler(struct net_mgmt_event_callback* cb, uint32_t mgmt_event,
                               struct net_if* iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT:
		{
			printk("Connected to %s", WIFI_SSID);
			break;
		}
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		{
			printk("Disconnected from %s", WIFI_SSID);
			break;
		}
	case NET_EVENT_WIFI_AP_ENABLE_RESULT:
		{
			printk("AP Mode is enabled. Waiting for station to connect");
			break;
		}
	case NET_EVENT_WIFI_AP_DISABLE_RESULT:
		{
			printk("AP Mode is disabled.");
			break;
		}
	default:
		printk("Other wifi event %d", mgmt_event);
		break;
	}
}
