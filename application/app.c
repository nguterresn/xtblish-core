#include "app.h"
#include "wasm/wasm.h"
#include <zephyr/kernel.h>

extern struct sys_heap         _system_heap;
static struct sys_memory_stats stats;

// (!) Important Note:
// Due to the way the Wifi manages the heap, it should be correclty initialized
// before any runtime is set. Wifi heap can not allocate memory in an external RAM.

extern struct k_sem new_ip;

struct k_sem new_wasm_app;

int app_init()
{
	return k_sem_init(&new_wasm_app, 0, 1);
}

void app_thread(void* arg1, void* arg2, void* arg3)
{
	printk("Start 'app_thread'\n");

	k_sem_take(&new_ip, K_FOREVER);

	printk("\n\n** START WASM_RUNTIME ! **\n");

	__ASSERT(wasm_boot_app() == 0, "Failed to boot app\n");
	sys_heap_runtime_stats_get(&_system_heap, &stats);

	printk("\n\n");
	printk("INFO: Allocated Heap = %zu\n", stats.allocated_bytes);
	printk("INFO: Free Heap = %zu\n", stats.free_bytes);
	printk("INFO: Max Allocated Heap = %zu\n", stats.max_allocated_bytes);
	printk("\n\n");

	for (;;) {
		if (k_sem_take(&new_wasm_app, K_FOREVER)) {}
	}
}
