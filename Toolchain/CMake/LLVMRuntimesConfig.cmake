# This file specifies the options used for building the various LLVM runtime libraries

# Note: We force the cmake module path for all dependent projects to include our custom directory
# That has the Platform/SerenityOS.cmake definition
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${SERENITY_MODULE_PATH}" CACHE STRING "Modules for CMake")

set(CMAKE_BUILD_TYPE Release CACHE STRING "")

set(LLVM_ENABLE_RUNTIMES "libcxx;libcxxabi;libunwind" CACHE STRING "")

set(CMAKE_SYSTEM_NAME SerenityOS CACHE STRING "")
set(target_triple ${SERENITY_TOOLCHAIN_ARCH}-pc-serenity)

IF(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON CACHE BOOL "")
ENDIF()

set(LLVM_ENABLE_PER_TARGET_RUNTIME_DIR ON CACHE BOOL "")
set(LLVM_ENABLE_BINDINGS OFF CACHE BOOL "")
set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "")
set(LLVM_BUILD_UTILS OFF CACHE BOOL "")
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "")
set(LLVM_BUILD_LLVM_DYLIB ON CACHE BOOL "")
set(LLVM_LINK_LLVM_DYLIB ON CACHE BOOL "")
set(LLVM_INSTALL_UTILS OFF CACHE BOOL "")
set(LLVM_INSTALL_TOOLCHAIN_ONLY ON CACHE BOOL "")
set(LLVM_INSTALL_BINUTILS_SYMLINKS OFF CACHE BOOL "")
set(LLVM_DEFAULT_TARGET_TRIPLE ${target_triple} CACHE STRING "")
set(LLVM_BINARY_DIR "${SERENITY_TOOLCHAIN_ROOT}" CACHE PATH "")

set(CMAKE_SYSROOT "${SERENITY_BUILD_DIR}/Root" CACHE PATH "")
set(compiler_flags "-nostdlib -nostdlib++")

set(CMAKE_C_COMPILER "${SERENITY_TOOLCHAIN_ROOT}/bin/clang" CACHE PATH "")
set(CMAKE_C_COMPILER_WORKS ON CACHE BOOL "")
set(CMAKE_CXX_COMPILER "${SERENITY_TOOLCHAIN_ROOT}/bin/clang++" CACHE PATH "")
set(CMAKE_CXX_COMPILER_WORKS ON CACHE BOOL "")
set(CMAKE_ASM_COMPILER "${SERENITY_TOOLCHAIN_ROOT}/bin/clang" CACHE PATH "")
set(CMAKE_ASM_COMPILER_WORKS ON CACHE BOOL "")
set(CMAKE_LINKER "${SERENITY_TOOLCHAIN_ROOT}/bin/ld.lld" CACHE PATH "")

set(CMAKE_ADDR2LINE "${SERENITY_TOOLCHAIN_ROOT}/bin/llvm-addr2line" CACHE PATH "")
set(CMAKE_AR "${SERENITY_TOOLCHAIN_ROOT}/bin/llvm-ar" CACHE PATH "")
set(CMAKE_NM "${SERENITY_TOOLCHAIN_ROOT}/bin/llvm-nm" CACHE PATH "")
set(CMAKE_OBJCOPY "${SERENITY_TOOLCHAIN_ROOT}/bin/llvm-objcopy" CACHE PATH "")
set(CMAKE_OBJDUMP "${SERENITY_TOOLCHAIN_ROOT}/bin/llvm-objdump" CACHE PATH "")
set(CMAKE_RANLIB "${SERENITY_TOOLCHAIN_ROOT}/bin/llvm-ranlib" CACHE PATH "")
set(CMAKE_READELF "${SERENITY_TOOLCHAIN_ROOT}/bin/llvm-readelf" CACHE PATH "")
set(CMAKE_STRIP "${SERENITY_TOOLCHAIN_ROOT}/bin/llvm-strip" CACHE PATH "")

set(CMAKE_C_COMPILER_TARGET ${target_triple} CACHE STRING "")
set(CMAKE_CXX_COMPILER_TARGET ${target_triple} CACHE STRING "")
set(CMAKE_ASM_COMPILER_TARGET ${target_triple} CACHE STRING "")

set(CMAKE_C_FLAGS ${compiler_flags} CACHE STRING "")
set(CMAKE_CXX_FLAGS ${compiler_flags} CACHE STRING "")

set(LIBCXX_USE_COMPILER_RT ON CACHE BOOL "")
set(LIBCXX_ENABLE_STATIC_ABI_LIBRARY ON CACHE BOOL "")
set(LIBCXX_INCLUDE_BENCHMARKS OFF CACHE BOOL "")
set(LIBCXXABI_USE_COMPILER_RT ON CACHE BOOL "")
set(LIBUNWIND_USE_COMPILER_RT ON CACHE BOOL "")

# Hardcode autodetection results for libm, libdl, and libpthread.
# This keeps us from accidentially detecting those libraries as being present
# if we build the toolchain with a populated sysroot (which features the
# compability linker scripts).
# TODO: Figure out if we can always build against the Stubs directory instead.
set(LIBCXXABI_HAS_DL_LIB OFF CACHE BOOL "")
set(LIBCXXABI_HAS_PTHREAD_LIB OFF CACHE BOOL "")
set(LIBCXX_HAS_M_LIB OFF CACHE BOOL "")
set(LIBCXX_HAS_PTHREAD_LIB OFF CACHE BOOL "")
set(LIBUNWIND_HAS_DL_LIB OFF CACHE BOOL "")
set(LIBUNWIND_HAS_PTHREAD_LIB OFF CACHE BOOL "")
