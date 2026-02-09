# Android NDK toolchain for macOS builds
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION 30)
set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM android-30)
set(ANDROID_STL c++_shared)

# Tell our CMakeLists.txt we're building for Android
set(ANDROID TRUE)

set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
set(CMAKE_ANDROID_NDK $ENV{HOME}/Library/Android/sdk/ndk/27.1.12297006)
set(ANDROID_NDK $ENV{HOME}/Library/Android/sdk/ndk/27.1.12297006)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
