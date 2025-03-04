#include "app.h"
#include "network/wifi.h"
#include "wasm/wasm.h"
#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

extern struct k_sem    net_sem;
extern struct sys_heap _system_heap;

static struct sys_memory_stats stats;

void app_thread(void* arg1, void* arg2, void* arg3)
{
	for (;;) {
		if (k_sem_take(&net_sem, K_FOREVER)) {
			printk("Boot WASM app\n");

			__ASSERT(wasm_boot_app() == 0, "Failed to boot app\n");
			sys_heap_runtime_stats_get(&_system_heap, &stats);

			printk("\n\n");
			printk("INFO: Allocated Heap = %zu\n", stats.allocated_bytes);
			printk("INFO: Free Heap = %zu\n", stats.free_bytes);
			printk("INFO: Max Allocated Heap = %zu\n",
			       stats.max_allocated_bytes);
			printk("\n\n");
		}
	}
}
