#include "wasm/wasm.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/storage/flash_map.h>

/**
 * @brief Port console.log to the native side. Provide a way to log from WASM
 * modules.
 * @note String argument is UTF-16 based.
 *
 * @param exec_env
 * @param str UTF-16 string
 */

int main(void)
{
	wasm_boot_app();
	return 0;
}
