#include <zephyr/kernel.h>
#include <zephyr/sys/__assert.h>
#include "app.h"
#include "network/wifi.h"
#include "network/http.h"

#define NK_THREAD(name, stack_size, func) \
	K_THREAD_DEFINE(name, stack_size, func, NULL, NULL, NULL, 5, 0, 0)

// Note: https://docs.zephyrproject.org/latest/kernel/services/threads/system_threads.html
// Above is a brief about the main and the idle thread.

NK_THREAD(_wifi_thread, 4096, wifi_thread);
NK_THREAD(_app_thread, 4096, app_thread);
NK_THREAD(_http_thread, 8192, http_thread);

// (!) Important Note:
// Due to the way the Wifi manages the heap, it should be correclty initialized
// before any runtime is set. Wifi heap can not allocate memory in an external RAM.

int main(void)
{
	// This code will run before any thread has started.

	int error = 0;

	error = wifi_init();
	if (error) {
		return -1;
	}

	error = http_init();
	if (error) {
		return -1;
	}
}
