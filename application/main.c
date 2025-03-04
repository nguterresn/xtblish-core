#include <zephyr/kernel.h>
#include <zephyr/sys/__assert.h>
#include "app.h"
#include "network/wifi.h"

#define NK_THREAD(name, stack_size, func) \
	K_THREAD_DEFINE(name, stack_size, func, NULL, NULL, NULL, 5, 0, 0)

// Note: https://docs.zephyrproject.org/latest/kernel/services/threads/system_threads.html
// Above is a brief about the main and the idle thread.

// NK_THREAD(_http_thread, 8192, http_thread);
NK_THREAD(_wifi_thread, 8192, wifi_thread);
// NK_THREAD(_app_thread, 8192, app_thread);

// (!) Important Note:
// Due to the way the Wifi manages the heap, it should be correclty initialized
// before any runtime is set. Wifi heap can not allocate memory in an external RAM.

int main(void)
{
	// app_init
	// wifi_init
}
