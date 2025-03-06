#include "app.h"
#include "wasm/wasm.h"
#include "network/wifi.h"
#include <zephyr/kernel.h>

extern struct sys_heap         _system_heap;
static struct sys_memory_stats stats;

// (!) Important Note:
// Due to the way the Wifi manages the heap, it should be correclty initialized
// before any runtime is set. Wifi heap can not allocate memory in an external RAM.

extern struct k_sem net_sem;

void app_thread(void* arg1, void* arg2, void* arg3)
{
	for (;;) {
		k_sem_take(&net_sem, K_FOREVER);

		printk("** START **\n");

		__ASSERT(wasm_boot_app() == 0, "Failed to boot app\n");
		sys_heap_runtime_stats_get(&_system_heap, &stats);

		printk("\n\n");
		printk("INFO: Allocated Heap = %zu\n", stats.allocated_bytes);
		printk("INFO: Free Heap = %zu\n", stats.free_bytes);
		printk("INFO: Max Allocated Heap = %zu\n", stats.max_allocated_bytes);
		printk("\n\n");
	}
}
