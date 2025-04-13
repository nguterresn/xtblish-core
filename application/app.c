#include "app.h"
#include "firmware/appq.h"
#include "firmware/app_handle.h"
#include "firmware/image_handle.h"
#include "stdio.h"
#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <zephyr/net/http/client.h>
#include <zephyr/sys/printk.h>

K_MSGQ_DEFINE(appq, sizeof(struct appq), 5, 1); // len: 3

static void app_handle_message(struct appq* data);

// (!) Important Note:
// Due to the way the Wifi manages the heap, it should be correclty initialized
// before any runtime is set. Wifi heap can not allocate memory in an external RAM.

int app_init()
{
	return 0;
}

int app_send(struct appq* data)
{
	return k_msgq_put(&appq, data, K_MSEC(1000));
}

void app_thread(void* arg1, void* arg2, void* arg3)
{
	struct appq data = { 0 };

	printk("Start 'app_thread'\n");

	/// ------ ///

	for (;;) {
		if (k_msgq_get(&appq, &data, K_FOREVER) == 0) {
			app_handle_message((struct appq*)&data);
		}
	}
}

static void app_handle_message(struct appq* data)
{
	switch (data->id) {
	case APP_AVAILABLE:
		app_handle_firmware_available(data);
		break;
	case APP_DOWNLOADED:
		app_handle_firmware_downloaded(data);
		break;
	case APP_VERIFIED:
		app_handle_firmware_verified(data);
		break;
	case IMAGE_AVAILABLE:
		image_handle_firmware_available(data);
		break;
	}
}

