# Custom cross-compile toolchain for Android ARM64 target
# Uses host aarch64-linux clang with Android NDK sysroot
#
# This is a workaround for the lack of official NDK builds for aarch64 Linux hosts.
# We use the x86_64 NDK's sysroot (headers + target libs are platform-independent)
# with the host system's clang as a cross-compiler.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Tell our CMakeLists.txt we're building for Android
set(ANDROID TRUE)
set(ANDROID_NDK /opt/android-ndk-r27c)

set(NDK_SYSROOT ${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot)
set(NDK_CLANG_RT ${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/lib/clang/18/lib/linux)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET aarch64-linux-android30)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-android30)
set(CMAKE_SYSROOT ${NDK_SYSROOT})

set(CMAKE_FIND_ROOT_PATH ${NDK_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

# Link paths: NDK API-level libs + clang runtime libs (for libgcc.a stub -> builtins)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=lld -L${NDK_SYSROOT}/usr/lib/aarch64-linux-android/30 -L${NDK_CLANG_RT}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld -L${NDK_SYSROOT}/usr/lib/aarch64-linux-android/30 -L${NDK_CLANG_RT}")
