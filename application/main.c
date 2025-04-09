#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/drivers/hwinfo.h>
#include "app.h"
#include "wasm/wasm.h"
#include "network/wifi.h"
#include "network/http.h"
#include "network/mqtt.h"
#include "zephyr/sys/printk.h"

#define N_THREAD(thread, stack, func)             \
	k_thread_create(&thread,                      \
	                stack,                        \
	                K_THREAD_STACK_SIZEOF(stack), \
	                func,                         \
	                NULL,                         \
	                NULL,                         \
	                NULL,                         \
	                5,                            \
	                0,                            \
	                K_NO_WAIT)

static K_THREAD_STACK_DEFINE(app_stack, 8192);
static K_THREAD_STACK_DEFINE(mqtt_stack, 4096);

static struct k_thread _app_thread;
static struct k_thread _mqtt_thread;

extern struct k_sem new_ip;

// Note: https://docs.zephyrproject.org/latest/kernel/services/threads/system_threads.html
// Above is a brief about the main and the idle thread.

// (!) Important Note:
// Due to the way the Wifi manages the heap, it should be correclty initialized
// before any runtime is set. Wifi heap can not allocate memory in an external RAM.

int main(void)
{
	int length = 0;

	uint8_t euid[8] = { 0 };
	length          = hwinfo_get_device_id(euid, sizeof(euid));
	printk("DEVICE UID: \n");
	for (uint8_t index = 0; index < length; index++) {
		printk("[%02x]", euid[index]);
	}
	printk("\n\n");

	__ASSERT(wifi_init() == 0, "WiFi has failed to initialized\n");
	__ASSERT(http_init() == 0, "HTTP has failed to initialized\n");
	__ASSERT(mqtt_init() == 0, "MQTT has failed to initialized\n");
	__ASSERT(wasm_init() == 0, "WASM has failed to initialized\n");

	__ASSERT(wifi_connect() == 0, "WiFi has failed to start a connection\n");

	k_sem_take(&new_ip, K_FOREVER);

	N_THREAD(_app_thread, app_stack, app_thread);
	k_thread_name_set(&_app_thread, "app_thread");
	N_THREAD(_mqtt_thread, mqtt_stack, mqtt_thread);
	k_thread_name_set(&_mqtt_thread, "mqtt_thread");
}
