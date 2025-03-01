#include "wasm/wasm.h"
#include "network/wifi.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

extern struct k_sem     wifi_sem;
extern struct sys_heap  _system_heap;
struct sys_memory_stats stats;

// Note: https://docs.zephyrproject.org/latest/kernel/services/threads/system_threads.html
// Above is a brief about the main and the idle thread.

int main(void)
{
	printk("\n\n** START **\n\n");

	if (wifi_init() || wifi_connect()) {
		return -1;
	}

	if (!k_sem_take(&wifi_sem, K_SECONDS(10))) {
		// If WiFi hasn't connected, quit.
		return -1;
	}

	sys_heap_runtime_stats_get(&_system_heap, &stats);

	printk("\n");
	printk("INFO: Allocated Heap = %zu\n", stats.allocated_bytes);
	printk("INFO: Free Heap = %zu\n", stats.free_bytes);
	printk("INFO: Max Allocated Heap = %zu\n", stats.max_allocated_bytes);
	printk("\n");

	printk("Boot WASM app\n");

	wasm_boot_app();
}
