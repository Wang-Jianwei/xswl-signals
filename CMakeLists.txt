cmake_minimum_required(VERSION 3.15)
project(xswl-signals VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 单头文件库 - 仅需要header_only
add_library(xswl_signals INTERFACE)
target_compile_features(xswl_signals INTERFACE cxx_std_11)
target_include_directories(xswl_signals INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 测试
enable_testing()
add_subdirectory(tests)

# 示例
add_subdirectory(examples)