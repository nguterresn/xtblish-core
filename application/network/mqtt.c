#include "mqtt.h"
#include "zephyr/kernel.h"
#include "zephyr/sys/printk.h"
#include <stdint.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/posix/arpa/inet.h>

/* Buffers for MQTT client. */
static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];

static bool          connected;
static struct pollfd fds[1];

// Only one client and only one broker!
static struct mqtt_client      client_ctx;
static struct sockaddr_storage broker;

static bool do_publish   = false;
static bool do_subscribe = false;

static void broker_init(void);
static int  wait(int timeout);
static void mqtt_evt_handler(struct mqtt_client*    client,
                             const struct mqtt_evt* evt);

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

int mqtt_sub(const char* topic_name, void (*cb)())
{
	(void)cb;

	int                                 ret;
	struct mqtt_topic                   topics[] = { {
		                  .topic = { .utf8 = topic_name, .size = strlen(topic_name) },
		                  .qos = 1,
    } };
	const struct mqtt_subscription_list sub_list = {
		.list       = topics,
		.list_count = ARRAY_SIZE(topics),
		.message_id = 1u, // May need to change this id.
	};

	printk("Subscribing to %hu topic(s)", sub_list.list_count);

	ret = mqtt_subscribe(&client_ctx, &sub_list);
	if (ret != 0) {
		printk("Failed to subscribe to topics: %d", ret);
	}

	return ret;
}

int mqtt_open(void)
{
	int error = mqtt_connect(&client_ctx);
	if (error) {
		return error;
	}

	if (client_ctx.transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds[0].fd = client_ctx.transport.tcp.sock;
	}
#if defined(CONFIG_MQTT_LIB_TLS)
	else if (client_ctx.transport.type == MQTT_TRANSPORT_SECURE) {
		fds[0].fd = client_ctx.transport.tls.sock;
	}
#endif
	fds[0].events = POLLIN;

	int result = wait(5000);
	if (!result) {
		goto abort;
	}
	mqtt_input(&client_ctx);

	if (!connected) {
		goto abort;
	}

	return 0;

abort:
	mqtt_abort(&client_ctx);
	return -ENODATA;
}

int mqtt_close()
{
	int error = 0;
	if (client_ctx.transport.tcp.sock < 0) {
		return -EINVAL;
	}

	error = mqtt_disconnect(&client_ctx);
	if (error) {
		return error;
	}
	error = close(client_ctx.transport.tcp.sock);
	if (error) {
		return error;
	}

	return error;
}

// Note: The idea about this thread is to buffer MQTT but also to recover from a MQTT disconnection!
void mqtt_thread(void* arg1, void* arg2, void* arg3)
{
	int timeout = 0;
	int result  = 0;
	int error   = mqtt_open();
	if (error) {
		printk("Failed to open MQTT, error %d\n", error);
	}

	for (;;) {
		timeout = mqtt_keepalive_time_left(&client_ctx);
		if (timeout < 0) {
			printk("ERROR mqtt_keepalive_time_left: %d\n", timeout);
			break;
		}

		// Contiue here... Check for timeout.. wait.. blablabla

		// // Check if something happens at TCP sock level (the one used for MQTT)
		// result = poll(&fds, 1u, timeout);
		// if (result < 0) {
		// 	break;
		// }

		// if (fds.revents & POLLIN) {
		// 	result = mqtt_input(&client_ctx);
		// 	if (result != 0) {
		// 		printk("Failed to read MQTT input: %d", result);
		// 		break;
		// 	}
		// }

		// if (fds.revents & (POLLHUP | POLLERR)) {
		// 	printk("Socket closed/error");
		// 	break;
		// }

		// result = mqtt_live(&client_ctx);
		// if ((result != 0) && (result != -EAGAIN)) {
		// 	printk("Failed to live MQTT: %d", result);
		// 	break;
		// }
	}

	mqtt_close();
}

static void broker_init(void)
{
	struct sockaddr_in* broker4 = (struct sockaddr_in*)&broker;

	broker4->sin_family = AF_INET;
	broker4->sin_port   = htons(1883);
	inet_pton(AF_INET, "192.168.0.140", &broker4->sin_addr);
}

static int wait(int timeout)
{
	return poll(fds, 1, timeout);
}

static void mqtt_evt_handler(struct mqtt_client*    client,
                             const struct mqtt_evt* evt)
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
		printk("Some other type of MQTT event: %d\n", evt->type);
		break;
	}
}
