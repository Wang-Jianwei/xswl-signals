cmake_minimum_required(VERSION 3.15)
project(xswl-signals VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 检测是否作为子项目被包含
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(XSWL_SIGNALS_IS_TOPLEVEL ON)
else()
    set(XSWL_SIGNALS_IS_TOPLEVEL OFF)
endif()

# 选项：是否构建测试和示例
option(XSWL_SIGNALS_BUILD_TESTS "Build tests" ${XSWL_SIGNALS_IS_TOPLEVEL})
option(XSWL_SIGNALS_BUILD_EXAMPLES "Build examples" ${XSWL_SIGNALS_IS_TOPLEVEL})

# 单头文件库 - 仅需要header_only
add_library(xswl_signals INTERFACE)
add_library(xswl::signals ALIAS xswl_signals)
target_compile_features(xswl_signals INTERFACE cxx_std_11)
target_include_directories(xswl_signals INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 测试
if(XSWL_SIGNALS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 示例
if(XSWL_SIGNALS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# 安装规则
include(GNUInstallDirs)

install(TARGETS xswl_signals
    EXPORT xswl-signals-targets
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT xswl-signals-targets
    FILE xswl-signals-targets.cmake
    NAMESPACE xswl::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xswl-signals
)

# 生成配置文件
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/xswl-signals-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/xswl-signals-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xswl-signals
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/xswl-signals-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/xswl-signals-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/xswl-signals-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/xswl-signals
)