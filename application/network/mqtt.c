#include "mqtt.h"
#include <stdint.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/posix/arpa/inet.h>

/* Buffers for MQTT client. */
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];

static bool connected;

static struct mqtt_client      client_ctx;
static struct sockaddr_storage broker;

static struct pollfd fds[1];
static int           nfds;

void mqtt_evt_handler(struct mqtt_client* client, const struct mqtt_evt* evt);

static void broker_init(void)
{
	struct sockaddr_in* broker4 = (struct sockaddr_in*)&broker;

	broker4->sin_family = AF_INET;
	broker4->sin_port   = htons(1883);
	inet_pton(AF_INET, "192.168.0.140", &broker4->sin_addr);
}

int mqtt_init()
{
	mqtt_client_init(&client_ctx);
	broker_init();

	/* MQTT client configuration */
	client_ctx.broker           = &broker;
	client_ctx.evt_cb           = mqtt_evt_handler;
	client_ctx.client_id.utf8   = (uint8_t*)"zephyr_mqtt_client";
	client_ctx.client_id.size   = sizeof("zephyr_mqtt_client") - 1;
	client_ctx.password         = NULL;
	client_ctx.user_name        = NULL;
	client_ctx.protocol_version = MQTT_VERSION_3_1_1;
	client_ctx.transport.type   = MQTT_TRANSPORT_NON_SECURE;

	/* MQTT buffers configuration */
	client_ctx.rx_buf      = rx_buffer;
	client_ctx.rx_buf_size = sizeof(rx_buffer);
	client_ctx.tx_buf      = tx_buffer;
	client_ctx.tx_buf_size = sizeof(tx_buffer);

	return 0;
}

static void prepare_fds(struct mqtt_client* client)
{
	if (client->transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds[0].fd = client->transport.tcp.sock;
	}
#if defined(CONFIG_MQTT_LIB_TLS)
	else if (client->transport.type == MQTT_TRANSPORT_SECURE) {
		fds[0].fd = client->transport.tls.sock;
	}
#endif

	fds[0].events = POLLIN;
	nfds          = 1;
}

static int wait(int timeout)
{
	int ret = 0;

	if (nfds > 0) {
		ret = poll(fds, nfds, timeout);
		if (ret < 0) {
			printk("poll error: %d", errno);
		}
	}

	return ret;
}

static int try_to_connect(struct mqtt_client* client)
{
	int rc, i = 0;

	while (i++ < 5 && !connected) {
		rc = mqtt_connect(client);
		if (rc != 0) {
			printk("mqtt_connect %d\n", rc);
			k_sleep(K_MSEC(3000));
			continue;
		}

		prepare_fds(client);

		if (wait(5000)) {
			mqtt_input(client);
		}

		if (!connected) {
			mqtt_abort(client);
		}
	}

	if (connected) {
		return 0;
	}

	return -EINVAL;
}

void mqtt_evt_handler(struct mqtt_client* client, const struct mqtt_evt* evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result != 0) {
			printk("MQTT connect failed %d", evt->result);
			break;
		}
		connected = true;
		printk("MQTT client connected!");

		break;

	case MQTT_EVT_DISCONNECT:
		printk("MQTT client disconnected %d", evt->result);
		connected = false;
		nfds      = 0;
		break;

	case MQTT_EVT_PUBACK:
		if (evt->result != 0) {
			printk("MQTT PUBACK error %d", evt->result);
			break;
		}

		printk("PUBACK packet id: %u", evt->param.puback.message_id);
		break;

	case MQTT_EVT_PUBREC:
		if (evt->result != 0) {
			printk("MQTT PUBREC error %d", evt->result);
			break;
		}

		printk("PUBREC packet id: %u", evt->param.pubrec.message_id);

		const struct mqtt_pubrel_param rel_param = {
			.message_id = evt->param.pubrec.message_id
		};

		int err = mqtt_publish_qos2_release(client, &rel_param);
		if (err != 0) {
			printk("Failed to send MQTT PUBREL: %d", err);
		}

		break;

	case MQTT_EVT_PUBCOMP:
		if (evt->result != 0) {
			printk("MQTT PUBCOMP error %d", evt->result);
			break;
		}
		printk("PUBCOMP packet id: %u", evt->param.pubcomp.message_id);
		break;

	case MQTT_EVT_PINGRESP:
		printk("PINGRESP packet");
		break;

	default:
		break;
	}
}
