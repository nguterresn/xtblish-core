
set (WAMR_BUILD_PLATFORM "zephyr")

# Set WAMR_BUILD_TARGET, currently values supported:
# "X86_64", "AMD_64", "X86_32", "AARCH64[sub]", "ARM[sub]", "THUMB[sub]",
# "MIPS", "XTENSA", "RISCV64[sub]", "RISCV32[sub]"
set (WAMR_BUILD_TARGET "XTENSA")

# Enable Interpreter by default
set (WAMR_BUILD_INTERP 1)
# When interpreter is disabled
set (WAMR_BUILD_AOT 0)
set (WAMR_BUILD_JIT 0)
# Disable Fast JIT by default
set (WAMR_BUILD_FAST_JIT 0)
# Disable libc builtin support by default
set (WAMR_BUILD_LIBC_BUILTIN 0)
# Disable libc wasi support by default
set (WAMR_BUILD_LIBC_WASI 0)
# NOTE: the fast interpreter runs ~2X faster than classic interpreter, but consumes about 2X memory to hold the pre-compiled code.
set (WAMR_BUILD_FAST_INTERP 1)

set (WAMR_BUILD_MULTI_MODULE 0)
set (WAMR_BUILD_LIB_PTHREAD 0)
set (WAMR_BUILD_MINI_LOADER 0)
# Note: only supported in AOT mode x86-64 target.
set (WAMR_BUILD_SIMD 1)
# Enable reference types by default
set (WAMR_BUILD_REF_TYPES 0)
# Garbage Colector
set (WAMR_BUILD_GC 0)
