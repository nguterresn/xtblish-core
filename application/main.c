#include "platform_common.h"
#include "wasm_export.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/storage/flash_map.h>
// #include "wasm_runtime.h"
// #include "wasm_runtime_common.h"
#include <stdint.h>
#include <stdio.h>

#define WASM_FILE_ADDRESS 0x00001000 // example for now.

/**
 * @brief Port console.log to the native side. Provide a way to log from WASM
 * modules.
 * @note String argument is UTF-16 based.
 *
 * @param exec_env
 * @param str UTF-16 string
 */
static void console_log(wasm_exec_env_t exec_env, const uint16_t* str);

static void _abort(wasm_exec_env_t exec_env, const uint16_t* msg, const uint16_t* filename,
                   uint32_t file_line, uint32_t col_line);

static const NativeSymbol native_symbols[] = { {
	                                               "console.log", // the name of WASM function name
	                                               console_log,   // the native function pointer
	                                               "($)" // the function prototype signature
	                                           },
	                                           {
	                                               "abort", // the name of WASM function name
	                                               _abort,  // the native function pointer
	                                               "($$ii)" // the function prototype signature
	                                           } };

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

// struct wasm_file {
// 	uint32_t* wasm_size;
// 	uint32_t* wasm_start;
// 	uint32_t* wasm_version;
// } __packed;

// static struct fs_mount_t mp = {
// 	.type        = FS_LITTLEFS,                             // File system type
// 	.mnt_point   = "/",                                  // Mount point
// 	.storage_dev = (void*)FLASH_AREA_ID(storage_partition), // Storage backend
// 	.fs_data     = &lfs_data,                               // File system data
// };

int main(void)
{
	char     error_buf[256] = { 0 };
	uint32_t stack_size = 4096, heap_size = 0;
	size_t   wasm_file_size = 0;

	bool result = wasm_runtime_full_init(&runtime_args);
	if (!result) {
		printk("Failed to initialize the runtime.\n");
	}
	wasm_runtime_set_log_level(WASM_LOG_LEVEL_VERBOSE);

	/* read WASM file into a memory buffer */
	uint32_t  wasm_size = *(uint32_t*)WASM_FILE_ADDRESS;
	uint32_t* wasm_ptr  = (uint32_t*)WASM_FILE_ADDRESS + 1;

	// if (!wasm_runtime_register_natives("env",
	//                                    native_symbols,
	//                                    sizeof(native_symbols) / sizeof(NativeSymbol))) {
	// 	printk("Failed to export native symbols!");
	// 	return 1;
	// }

	// // /* parse the WASM file from buffer and create a WASM module */
	// wasm_module_t module = wasm_runtime_load(wasm_ptr,
	//                                          wasm_file_size,
	//                                          error_buf,
	//                                          sizeof(error_buf));
	// if (module == NULL) {
	// 	printk("Failed to load! Error: %s\n", error_buf);
	// 	return 1;
	// }

	// /* create an instance of the WASM module (WASM linear memory is ready) */
	// wasm_module_inst_t module_inst = wasm_runtime_instantiate(module,
	//                                                           0, // May be ignored.
	//                                                           heap_size,
	//                                                           error_buf,
	//                                                           sizeof(error_buf));
	// if (module_inst == NULL) {
	// 	printk("Failed to instantiate! Error: %s\n", error_buf);
	// 	return 1;
	// }

	// // The stack size is the amount of memory necessary to store
	// // the WASM binary operations, like i32.add 1 or i32.const 1
	// wasm_exec_env_t exec_env = wasm_runtime_create_exec_env(module_inst, stack_size);
	// if (exec_env == NULL) {
	// 	printk("Failed to create the environment!\n");
	// 	return 1;
	// }

	// wasm_function_inst_t func = wasm_runtime_lookup_function(module_inst, "hello_world");
	// if (func == NULL) {
	// 	printk("Failed to look up for 'hello_world'\n");
	// 	return 1;
	// }

	// wasm_runtime_call_wasm(exec_env, func, 0, NULL);

	return 0;
}

void console_log(wasm_exec_env_t exec_env, const uint16_t* str)
{
	printk("[Native]: ");
	while (*str) {
		printk("%c", (char)*str);
		str++;
	}

	printk("\n");
}

void _abort(wasm_exec_env_t exec_env, const uint16_t* msg, const uint16_t* filename,
            uint32_t file_line, uint32_t col_line)
{
	printk("[Native]: WASM module abort line %d col %d", file_line, col_line);
}

