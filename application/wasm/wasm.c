#include "wasm.h"
#include "wasm_export.h"
#include <sys/_stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/storage/flash_map.h>
#include <stdio.h>
#include "bindings.h"

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

// Storage partition has size of 0x6000, thus 6144 words.
__aligned(4) uint8_t wasm_file[WASM_FILE_MAX_SIZE * 4] = { 0 };

static const uint32_t stack_size = 4096;
static const uint32_t heap_size  = 0;

static wasm_module_t      module      = NULL;
static wasm_module_inst_t module_inst = NULL;
static wasm_exec_env_t    exec_env    = NULL;

static const struct flash_area* wasm_area = NULL;

int wasm_boot_app(bool skip_runtime_init)
{
	char error_buf[128] = { 0 };
	int  error          = 0;

	if (!skip_runtime_init) {
		bool result = wasm_runtime_full_init(&runtime_args);
		if (!result) {
			printk("Failed to initialize the runtime.\n");
			return -EPERM;
		}

		if (!wasm_runtime_register_natives("env",
		                                   native_symbols,
		                                   sizeof(native_symbols) /
		                                       sizeof(NativeSymbol))) {
			printk("Failed to export native symbols!");
			return -EPERM;
		}

		error = flash_area_open(FIXED_PARTITION_ID(storage_partition),
		                        &wasm_area);
		if (error) {
			printk("Error while opening flash partition 'storage_partition'");
			return error;
		}
		printk("Part offset 0x%08x part size %u\n",
		       (uint32_t)wasm_area->fa_off,
		       (uint32_t)wasm_area->fa_size);
	}

	error = flash_area_read(wasm_area, 0, wasm_file, wasm_area->fa_size);
	if (error) {
		printk("Error while reading the partition. %d \n", error);
		return error;
	}

	struct wasm_file* file = (struct wasm_file*)wasm_file;

	printk("[WASM file] size: %u prefix: %u version: %u\n",
	       file->size,
	       file->prefix,
	       file->version);

	// /* parse the WASM file from buffer and create a WASM module */
	module = wasm_runtime_load((uint8_t*)&file->prefix,
	                           file->size,
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

	return 0;
}

int wasm_replace_app(uint8_t* src, uint32_t len)
{
	int error = 0;
	error     = flash_area_erase(wasm_area, 0, WASM_FILE_MAX_SIZE * 4);
	if (error < 0) {
		printk("Failed to erase the flash! [%d]\n", error);
		return error;
	}

	error = flash_area_write(wasm_area, 0, (uint32_t*)src, len);
	if (error < 0) {
		printk("Failed to write new app onto the flash! [%d]\n", error);
		return error;
	}

	wasm_runtime_destroy_exec_env(exec_env);
	wasm_runtime_deinstantiate(module_inst);
	wasm_runtime_unload(module);

	return wasm_boot_app(true);
}
