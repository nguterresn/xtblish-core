#include "wasm.h"
#include "utils/flash_util.h"
#include "wasm_export.h"
#include <stdbool.h>
#include <sys/_stdint.h>
#include <sys/errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/storage/flash_map.h>
#include <stdio.h>
#include "bindings.h"
#include "zephyr/sys/__assert.h"

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

extern struct sys_heap         _system_heap;
static struct sys_memory_stats stats;
static const uint32_t          stack_size = 4096;
static const uint32_t          heap_size  = 0;

static wasm_module_t      module      = NULL;
static wasm_module_inst_t module_inst = NULL;
static wasm_exec_env_t    exec_env    = NULL;

static const struct flash_area* app0_wasm_area = NULL;
static const struct flash_area* app1_wasm_area = NULL;
static int                      wasm_boot();

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

	error = flash_area_open(FIXED_PARTITION_ID(app0_partition),
	                        &app0_wasm_area);
	if (error < 0) {
		printk("Error while opening flash partition 'app0_partition'");
		return error;
	}

	error = flash_area_open(FIXED_PARTITION_ID(app1_partition),
	                        &app1_wasm_area);
	if (error < 0) {
		printk("Error while opening flash partition 'app1_partition'");
		return error;
	}

	printk("app0_wasm_area off: 0x%08x size: %u\napp1_wasm_area off: 0x%08x "
	       "size: %u\n",
	       (uint32_t)app0_wasm_area->fa_off,
	       (uint32_t)app0_wasm_area->fa_size,
	       (uint32_t)app1_wasm_area->fa_off,
	       (uint32_t)app1_wasm_area->fa_size);

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

int wasm_write_app1(uint8_t* src, uint32_t len, uint32_t sector_offset,
                    uint32_t sector_len, bool force)
{
	int      error  = 0;
	uint32_t offset = sector_offset * sector_len;

	if (!force) {
		__ASSERT(len == sector_len,
		         "Length is not equal to sector_len: %d\n",
		         len);
	}

	error = flash_area_erase(app1_wasm_area, offset, sector_len);
	if (error) {
		printk("[%s] Failed to erase 'app1_wasm_area' offset: 0x%02x len: %d\n",
		       __func__,
		       offset,
		       sector_len);
		return error;
	}

	error = flash_area_write(app1_wasm_area, offset, src, len);
	if (error) {
		printk("[%s] Failed to write to 'app1_wasm_area' offset: 0x%02x len: "
		       "%d\n",
		       __func__,
		       offset,
		       len);
		return error;
	}

	return 0;
}

int wasm_verify_and_copy(uint32_t app1_write_len)
{
	// Verify the contens of app1, e.g. signature.
	int error = 0;

#if defined(CONFIG_SOC_ESP32S3)
	const void*             app1_wasm_area_ptr = NULL;
	spi_flash_mmap_handle_t handle;
	// Note: Not really sure about this. It is technically possible to access the
	// memory from the CPU just by accessing the MMU cache. However, until I figure
	// something that works across different architectures, I'd prefer to keep
	// the memory address behind a function call.
	error = spi_flash_mmap(app1_wasm_area->fa_off,
	                       app1_wasm_area->fa_size,
	                       SPI_FLASH_MMAP_DATA,
	                       &app1_wasm_area_ptr,
	                       &handle);
	printk("memory-mapped pointer address: %p error=%d",
	       app1_wasm_area_ptr,
	       error);
	if (error) {
		return error;
	}
#else
	return -ENOTSUP;
#endif

	// Not sure this erases by section. Check that later.
	error = flash_area_erase(app0_wasm_area, 0, app1_write_len);
	if (error) {
		printk("Failed to erase %d byted from app0 error=%d\n",
		       app1_write_len,
		       error);
		return error;
	}

	// Write app1 -> app0
	error = flash_area_write(app0_wasm_area,
	                         0,
	                         app1_wasm_area_ptr,
	                         app1_write_len);
	if (error) {
		printk("Failed to write %d byted to app0 error=%d\n",
		       app1_write_len,
		       error);
		return error;
	}

	error = flash_area_erase(app1_wasm_area, 0, app1_write_len);
	if (error) {
		printk("Failed to erase %d byted from app1 error=%d\n",
		       app1_write_len,
		       error);
		return error;
	}

	return 0;
}

static int wasm_boot()
{
	char        error_buf[128] = { 0 };
	int         error          = 0;
	const void* mem_ptr        = NULL;

#if defined(CONFIG_SOC_ESP32S3)
	const void*             app0_wasm_area_ptr = NULL;
	spi_flash_mmap_handle_t handle;
	error = spi_flash_mmap(app0_wasm_area->fa_off,
	                       app0_wasm_area->fa_size,
	                       SPI_FLASH_MMAP_DATA,
	                       &app0_wasm_area_ptr,
	                       &handle);
	printk("memory-mapped pointer address: %p", app0_wasm_area_ptr);
	if (error) {
		return error;
	}
#else
	return -ENOTSUP;
#endif

	struct wasm_file* file = (struct wasm_file*)mem_ptr;

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

#if defined(CONFIG_SOC_ESP32S3)
	spi_flash_munmap(handle);
#else
	return -ENOTSUP;
#endif

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

	printk("\n\nINFO: Allocated Heap = %zu\n", stats.allocated_bytes);
	printk("INFO: Free Heap = %zu\n", stats.free_bytes);
	printk("INFO: Max Allocated Heap = %zu\n\n\n", stats.max_allocated_bytes);

	return 0;
}
