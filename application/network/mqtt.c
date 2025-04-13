#include "mqtt.h"
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/printk.h>
#include <zephyr/data/json.h>
#include <stdint.h>
#include <zephyr/net/mqtt.h>
#include <zephyr/posix/arpa/inet.h>
#include "app.h"
#include "firmware/appq.h"
#include "server.h"

/* Buffers for MQTT client. */
static uint8_t rx_buffer[SERVER_MQTT_BUFFER_SIZE];
static uint8_t tx_buffer[SERVER_MQTT_BUFFER_SIZE];

static bool          connected;
static struct pollfd fds[1];

// Only one client and only one broker!
static struct mqtt_client      client_ctx;
static struct sockaddr_storage broker;

struct mqtt_topic_app_image {
	const char* url;
};

static const struct json_obj_descr mqtt_topic_app_image_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct mqtt_topic_app_image, url, JSON_TOK_STRING),
};

static void mqtt_broker_init(void);
static int  mqtt_setup(void);
static int  mqtt_open(void);
static int  mqtt_sub(const char* topic_name);
static int  mqtt_close(void);

static void mqtt_evt_handler(struct mqtt_client*    client,
                             const struct mqtt_evt* evt);
static void mqtt_publish_handler(struct mqtt_client*    client,
                                 const struct mqtt_evt* evt);

int mqtt_init()
{
	mqtt_client_init(&client_ctx);
	mqtt_broker_init();

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

// Note: The idea about this thread is to buffer MQTT but also to recover from a MQTT disconnection!
void mqtt_thread(void* arg1, void* arg2, void* arg3)
{
	int timeout = 0;
	int error   = 0;

	for (;;) {
		// In case server closes the connection or is not yet connected.
		if (!connected) {
			error = mqtt_setup();
			if (error) {
				printk("MQTT couldn't establish connection to server! "
				       "error=%d\n",
				       error);
				k_sleep(K_MSEC(SERVER_MQTT_TIMEOUT));
				continue;
			}
		}

		// Connected.
		timeout = mqtt_keepalive_time_left(&client_ctx);
		error   = poll(fds, 1, timeout);
		// timeout (if error == 0) is also valid.
		__ASSERT(error >= 0,
		         "Failed to wait for something in the MQTT TCP socket, "
		         "error=%d\n",
		         error);

		if (fds[0].revents && error > 0) {
			if (fds[0].revents & POLLIN) {
				// If there's data, will trigger `mqtt_evt_handler`.
				error = mqtt_input(&client_ctx);
				if (!connected) {
					// In case server closes the connection.
					continue;
				}
				__ASSERT(error == 0,
				         "Failed to to run mqtt_input socket, error=%d\n",
				         error);
			}
			else if (fds[0].revents & (POLLHUP | POLLERR)) {
				connected = false;
				continue;
			}
		}

		// If there's nothing else to do -> keep the connection alive.
		error = mqtt_live(&client_ctx);
		__ASSERT(error == 0 || error == -EAGAIN,
		         "Faield to send mqtt hearbeat, error=%d\n",
		         error);
	}
}

static int mqtt_setup(void)
{
	int error = 0;
	error     = mqtt_open();
	if (error) {
		return error;
	}

	// TODO: replace '124' with device's id.
	error = mqtt_sub("app/124");
	if (error < 0) {
		return error;
	}

	error = mqtt_sub("image");
	if (error < 0) {
		return error;
	}

	return 0;
}

static void mqtt_broker_init(void)
{
	struct sockaddr_in* broker4 = (struct sockaddr_in*)&broker;

	broker4->sin_family = AF_INET;
	broker4->sin_port   = htons(SERVER_MQTT_PORT);
	inet_pton(AF_INET, SERVER_IP, &broker4->sin_addr);
}

static int mqtt_open(void)
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

	// error == 0 when timeout, error < 0 when error
	error = poll(fds, 1, SERVER_MQTT_TIMEOUT);
	if (error <= 0) {
		goto abort;
	}
	error = mqtt_input(&client_ctx);

	if (!connected || error) {
		goto abort;
	}

	return 0;

abort:
	error = mqtt_abort(&client_ctx);
	return error ? error : -ENODATA;
}

static int mqtt_sub(const char* topic_name)
{
	if (!topic_name) {
		return -EINVAL;
	}

	int                                 error    = 0;
	struct mqtt_topic                   topics[] = { {
		                  .topic = { .utf8 = topic_name, .size = strlen(topic_name) },
		                  .qos = 0,
    } };
	const struct mqtt_subscription_list sub_list = {
		.list       = topics,
		.list_count = ARRAY_SIZE(topics),
		.message_id = 1u, // TODO: May need to change this id.
	};

	printk("Subscribing to %hu topic(s) '%s'\n",
	       sub_list.list_count,
	       topic_name);

	error = mqtt_subscribe(&client_ctx, &sub_list);
	if (error < 0) {
		printk("Failed to subscribe to topics: %d\n", error);
	}

	return error;
}

static int mqtt_close(void)
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

static void mqtt_evt_handler(struct mqtt_client*    client,
                             const struct mqtt_evt* evt)
{
	switch (evt->type) {
	case MQTT_EVT_CONNACK:
		if (evt->result == 0) {
			connected = true;
		}
		printk("(MQTT_EVT_CONNACK) MQTT connect result %d\n", evt->result);
		break;

	case MQTT_EVT_DISCONNECT:
		printk("(MQTT_EVT_DISCONNECT) MQTT client disconnected %d\n",
		       evt->result);
		connected = false;
		break;

	case MQTT_EVT_PUBLISH:
		mqtt_publish_handler(client, evt);
		break;

	case MQTT_EVT_SUBACK:
		printk("(MQTT_EVT_SUBACK) error=%d message_id=%d\n",
		       evt->result,
		       evt->param.suback.message_id);
		break;

	default:
		printk("(OTHER) MQTT evt_type=%d error=%d\n", evt->type, evt->result);
		break;
	}
}

static void mqtt_publish_handler(struct mqtt_client*    client,
                                 const struct mqtt_evt* evt)
{
	printk("(MQTT_EVT_PUBLISH) error=%d topic='%s' qos=%d message_id=%d "
	       "len=%u\n",
	       evt->result,
	       evt->param.publish.message.topic.topic.utf8,
	       evt->param.publish.message.topic.qos,
	       evt->param.publish.message_id,
	       evt->param.publish.message.payload.len);

	if (evt->result != 0 || !evt->param.publish.message.payload.len) {
		return;
	}

	// Don't support duplication for now.
	if (evt->param.publish.dup_flag) {
		return;
	}

	char                        buf[evt->param.publish.message.payload.len];
	struct mqtt_topic_app_image topic_app = { 0 };
	struct appq                 data      = { 0 };

	int error = mqtt_read_publish_payload(client,
	                                      buf,
	                                      evt->param.publish.message.payload
	                                          .len);
	if (error < 0) {
		printk("Failed to read mqtt payload error=%d\n", error);
		return;
	}

	error = json_obj_parse(buf,
	                       evt->param.publish.message.payload.len,
	                       mqtt_topic_app_image_descr,
	                       ARRAY_SIZE(mqtt_topic_app_image_descr),
	                       &topic_app);
	if (error < 0) {
		printk("Failed to parse JSON payload error=%d\n", error);
		return;
	}

	if (strstr(evt->param.publish.message.topic.topic.utf8, "app") != NULL) {
		data.id = APP_AVAILABLE;
	}
	else if (strstr(evt->param.publish.message.topic.topic.utf8, "image") !=
	         NULL) {
		data.id = IMAGE_AVAILABLE;
	}
	else {
		return;
	}

	strcpy(data.url, topic_app.url);
	app_send(&data);

	printk("(mqtt_read_publish_payload) error=%d message='%s'\n\n", error, buf);
}
