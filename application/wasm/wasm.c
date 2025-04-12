#include "wasm.h"
#include "wasm_export.h"
#include <stdbool.h>
#include <sys/_stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdio.h>
#include "bindings.h"
#include "storage/flash.h"

#if defined(CONFIG_SOC_ESP32S3)
#include <spi_flash_mmap.h>
#else
#error "Memory mapping is not yet supported for other socs!"
#endif

struct wasm_file {
	uint8_t  hash[32];
	uint8_t  data[512];
	uint32_t size;
	uint32_t prefix;
	uint32_t version;
} __packed;

static NativeSymbol native_symbols[] = {
	{
	    "console.log",  // the name of WASM function name
	    bd_console_log, // the native function pointer
	    "($)"           // the function prototype signature
	},
	{
	    "abort",  // the name of WASM function name
	    bd_abort, // the native function pointer
	    "($$ii)"  // the function prototype signature
	}
};

static RuntimeInitArgs runtime_args = {
  .mem_alloc_type = Alloc_With_Allocator,
  .mem_alloc_option = {
    .allocator = {
      .malloc_func = k_malloc,
      .realloc_func= k_realloc,
      .free_func = k_free,
    }
  },
  .n_native_symbols = 0,
  .gc_heap_size = 0
};

extern struct sys_heap          _system_heap;
extern const struct flash_area* app0_wasm_area;
extern const struct flash_area* app1_wasm_area;

static struct sys_memory_stats stats;
static const uint32_t          stack_size = 4096;
static const uint32_t          heap_size  = 0;

static wasm_module_t      module      = NULL;
static wasm_module_inst_t module_inst = NULL;
static wasm_exec_env_t    exec_env    = NULL;

static int wasm_boot();

int wasm_init()
{
	int  error  = 0;
	bool result = false;

	result = wasm_runtime_full_init(&runtime_args);
	if (!result) {
		printk("Failed to initialize the runtime.\n");
		return -EPERM;
	}

	result = wasm_runtime_register_natives("env",
	                                       native_symbols,
	                                       sizeof(native_symbols) /
	                                           sizeof(NativeSymbol));
	if (!result) {
		printk("Failed to export native symbols!");
		return -EPERM;
	}

	return 0;
}

int wasm_replace(void)
{
	// Note: this takes some time to execute.
	if (exec_env) {
		wasm_runtime_destroy_exec_env(exec_env);
	}
	if (module_inst) {
		wasm_runtime_deinstantiate(module_inst);
	}
	if (module) {
		wasm_runtime_unload(module);
	}

	return wasm_boot();
}

static int wasm_boot()
{
	char error_buf[128] = { 0 };
	int  error          = 0;

	const void* mem_ptr = flash_get_app0();
	if (mem_ptr == NULL) {
		printk("Failed to get the app0 address\n");
		return -ENOMEM;
	}

	struct wasm_file* file = (struct wasm_file*)mem_ptr;

	printk("[WASM file] size: %u prefix: %u version: %u\n",
	       file->size,
	       file->prefix,
	       file->version);

	// 'wasm_binary_freeable' = true is necessary. It will cache a few things
	// locally, so that flash is no longer needed.
	// However, a better option may be to change to AOT instead.
	LoadArgs args = { .name = "main", .wasm_binary_freeable = true };
	module        = wasm_runtime_load_ex((uint8_t*)&file->prefix,
                                  file->size,
                                  &args,
                                  error_buf,
                                  sizeof(error_buf));
	if (module == NULL) {
		printk("Failed to load! Error: '%s' File size: %u\n",
		       error_buf,
		       file->size);
		return -EPERM;
	}

	/* create an instance of the WASM module (WASM linear memory is ready) */
	module_inst = wasm_runtime_instantiate(module,
	                                       0, // May be ignored.
	                                       heap_size,
	                                       error_buf,
	                                       sizeof(error_buf));
	if (module_inst == NULL) {
		printk("Failed to instantiate! Error: %s\n", error_buf);
		return -EPERM;
	}

	// The stack size is the amount of memory necessary to store
	// the WASM binary operations, like i32.add 1 or i32.const 1
	exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);
	if (exec_env == NULL) {
		printk("Failed to create the environment!\n");
		return -EPERM;
	}

	wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst,
	                                                         "hello_world");
	if (func == NULL) {
		printk("Failed to look up for 'hello_world'\n");
		return -EPERM;
	}

	if (!wasm_runtime_call_wasm(exec_env, func, 0, NULL)) {
		printk("Failed to call 'hello_world'\n");
		return -EPERM;
	}

	sys_heap_runtime_stats_get(&_system_heap, &stats);

	printk("INFO: Allocated Heap = %zu\n", stats.allocated_bytes);
	printk("INFO: Free Heap = %zu\n", stats.free_bytes);
	printk("INFO: Max Allocated Heap = %zu\n\n", stats.max_allocated_bytes);

	return 0;
}
