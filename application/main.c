#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/sys/__assert.h>
#include "app.h"
#include "network/wifi.h"
#include "network/http.h"

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

static K_THREAD_STACK_DEFINE(wifi_stack, 4096);
static K_THREAD_STACK_DEFINE(http_stack, 8192);

static struct k_thread _wifi_thread;
static struct k_thread _http_thread;

// Note: https://docs.zephyrproject.org/latest/kernel/services/threads/system_threads.html
// Above is a brief about the main and the idle thread.

// NK_THREAD(_wifi_thread, 4096, wifi_thread);
// NK_THREAD(_app_thread, 4096, app_thread);
// NK_THREAD(_http_thread, 8192, http_thread);

// (!) Important Note:
// Due to the way the Wifi manages the heap, it should be correclty initialized
// before any runtime is set. Wifi heap can not allocate memory in an external RAM.

int main(void)
{
	int error = 0;

	__ASSERT(wifi_init() == 0, "WiFi has failed to initialized\n");
	__ASSERT(http_init() == 0, "HTTP has failed to initialized\n");

	__ASSERT(wifi_connect() == 0, "WiFi has failed to start a connection\n");

	// N_THREAD(_wifi_thread, wifi_stack, wifi_thread);
	// k_thread_name_set(&_wifi_thread, "wifi_thread");

	N_THREAD(_http_thread, http_stack, http_thread);
	k_thread_name_set(&_http_thread, "thread_thread");
}
