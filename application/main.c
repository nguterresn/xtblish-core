#include "wasm/wasm.h"
#include "network/wifi.h"
#include <zephyr/kernel.h>

int main(void)
{
	k_sleep(K_SECONDS(5));

	wifi_init();
	wifi_connect();
	// wasm_boot_app();

	while (1) {
		k_yield();
	}
}
