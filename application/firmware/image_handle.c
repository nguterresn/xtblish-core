#include "image_handle.h"
#include <zephyr/sys/printk.h>

void image_handle_firmware_available(struct appq* data)
{
	int error = 0;
	// flash_util_init(&flash_ctx);
	printk("[%s] error=%d\n", __func__, error);
}
