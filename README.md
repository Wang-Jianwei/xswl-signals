# xswl-signals

一个现代化的 C++11 信号/槽库，提供类型安全、灵活且高效的事件处理机制。

Header-only, lightweight signals/slots library for modern C++ (C++11). Provides type-safe, flexible and efficient event handling with advanced features.

## ✨ 特性 / Features

- 📦 **Header-only** - 单头文件设计，易于集成 / Single-header design for easy integration
- 🔗 **连接管理** - block/unblock, scoped connections, connection groups, 标签断开 / Connection lifecycle management
- ⚡ **优先级调度** - 按优先级顺序执行槽函数 / Priority-based slot execution
- 🎯 **单次槽函数** - `connect_once` 自动断开 / Single-shot slots with automatic disconnection  
- 🔧 **参数适配** - 槽函数可接受更少的参数 / Slots can accept fewer parameters than signals (0-6)
- 🧬 **生命周期管理** - 成员函数自动跟踪对象生命周期 / Automatic lifetime tracking for member functions
- 🏷️ **标签连接** - 批量管理相关连接 / Tag-based batch connection management
- 🔒 **线程友好** - 使用原子操作和互斥锁 / Thread-safe with atomics and mutexes

## 📋 要求 / Requirements

- CMake >= 3.15
- C++11 编译器 / C++11 compiler (tested with GCC/Clang)

## 🚀 快速开始 / Quick Start

```cpp
#include "xswl/signals.hpp"
#include <iostream>

int main() {
    // 创建信号 / Create signal
    xswl::signal_t<int, std::string> on_message;

    // 普通连接 / Regular connection
    auto c1 = on_message.connect([](int id, const std::string& msg) {
        std::cout << "收到消息 / Received: " << id << " - " << msg << "\n";
    });

    // 单次连接 / Single-shot connection
    on_message.connect_once([](int id, const std::string& msg) {
        std::cout << "只执行一次 / Once: " << id << " - " << msg << "\n";
    });

    // 带优先级的连接 / Connection with priority
    on_message.connect([](int id, const std::string& msg) {
        std::cout << "高优先级 / High priority: " << id << " - " << msg << "\n";
    }, /*priority*/ 50);

    // 参数适配：槽函数只接受第一个参数 / Parameter adaptation
    on_message.connect([](int id) {
        std::cout << "仅ID / ID only: " << id << "\n";
    });

    // 发射信号 / Emit signal
    emit on_message(42, "hello");
    
    // 断开连接 / Disconnect
    c1.disconnect();
    
    return 0;
}
```

**输出 / Output:**
```
高优先级 / High priority: 42 - hello
收到消息 / Received: 42 - hello
只执行一次 / Once: 42 - hello
仅ID / ID only: 42
```

## 🔧 构建 & 测试 / Build & Test

```bash
./build.sh                  # 配置 & 构建 (Release) / configure & build (Release)
./build.sh build --debug    # Debug 构建 / Debug build
./build.sh test             # 运行 ctest / run ctest
./build.sh help             # 显示帮助 / show help
```

## 📦 集成 / Integration

### 方法 1 / Method 1: CMake FetchContent（推荐 / Recommended）
```cmake
include(FetchContent)

FetchContent_Declare(
    xswl-signals
    GIT_REPOSITORY https://github.com/Wang-Jianwei/xswl-signals.git
    GIT_TAG main  # 或特定版本标签 / or specific version tag
)
FetchContent_MakeAvailable(xswl-signals)

target_link_libraries(your_target PRIVATE xswl::signals)
```

### 方法 2 / Method 2: add_subdirectory
```cmake
add_subdirectory(path/to/xswl-signals)
target_link_libraries(your_target PRIVATE xswl::signals)
```

### 方法 3 / Method 3: find_package（安装后 / after installation）
```cmake
find_package(xswl-signals REQUIRED)
target_link_libraries(your_target PRIVATE xswl::signals)
```

**安装库 / Install the library:**
```bash
./build.sh build --install
# 或手动 / or manually:
cmake --build build --target install
```

## 📚 文档 / Documentation

- [中文 API 文档](doc/API.md) - 详细的 API 参考和使用示例
- [English API Documentation](doc/API_EN.md) - Complete API reference and usage examples

## 💡 核心功能示例 / Core Features Examples

### 参数适配 / Parameter Adaptation

槽函数可以接受比信号更少的参数：

Slots can accept fewer parameters than the signal:

```cpp
xswl::signal_t<int, std::string, double> sig;

// 完整参数 / Full parameters
sig.connect([](int a, const std::string& b, double c) { /* ... */ });

// 只需要前两个 / Only first two
sig.connect([](int a, const std::string& b) { /* ... */ });

// 只需要第一个 / Only first one
sig.connect([](int a) { /* ... */ });

// 不需要参数 / No parameters
sig.connect([]() { /* ... */ });

sig(42, "hello", 3.14);  // 所有槽都会被调用 / All slots will be called
```

### 生命周期管理 / Lifetime Management

使用 `shared_ptr` 自动管理对象生命周期：

Automatic lifetime tracking with `shared_ptr`:

```cpp
struct Handler {
    void on_event(int value) {
        std::cout << "处理 / Handling: " << value << "\n";
    }
};

xswl::signal_t<int> sig;

{
    auto handler = std::make_shared<Handler>();
    sig.connect(handler, &Handler::on_event);
    
    sig(1);  // 输出 / Prints: 处理 / Handling: 1
}  // handler 被销毁 / handler destroyed

sig(2);  // 不输出（对象已销毁）/ Does not print (object destroyed)
```

### 标签连接 / Tagged Connections

批量管理相关连接：

Batch management of related connections:

```cpp
xswl::signal_t<std::string> sig;

sig.connect("logger", [](const std::string& msg) {
    std::cout << "Log: " << msg << "\n";
});

sig.connect("logger", [](const std::string& msg) {
    std::cout << "Debug: " << msg << "\n";
});

sig.connect("ui", [](const std::string& msg) {
    std::cout << "UI: " << msg << "\n";
});

sig("test");  // 所有槽都执行 / All slots execute

sig.disconnect("logger");  // 断开所有 logger 标签 / Disconnect all "logger" tags

sig("test2");  // 只有 UI 槽执行 / Only UI slot executes
```

### 连接管理 / Connection Management

```cpp
// RAII 风格的作用域连接 / RAII-style scoped connection
{
    xswl::scoped_connection_t scoped = sig.connect([]() { /* ... */ });
    sig();  // 会调用 / Will call
}  // 自动断开 / Automatically disconnects

sig();  // 不会调用 / Will not call

// 连接组 / Connection group
xswl::connection_group_t group;
group += sig1.connect([]() { /* ... */ });
group += sig2.connect([]() { /* ... */ });
group.disconnect_all();  // 批量断开 / Batch disconnect
```

## 📖 示例程序 / Examples

构建后运行示例：/ After `./build.sh build`, run examples:

```bash
./build/examples/signals_basic       # 基础用法 / Basic usage
./build/examples/signals_lifecycle   # 生命周期管理 / Lifetime management
```

示例源码：/ Example sources:
- [examples/basic.cpp](examples/basic.cpp) - 基础功能演示 / Basic features demonstration
- [examples/lifecycle.cpp](examples/lifecycle.cpp) - 生命周期和成员函数 / Lifetime and member functions

## 🧪 测试 / Tests

运行测试：/ Run tests:

```bash
./build.sh test
```

测试包括：/ Tests include:
- SignalsBaseTest - 基础功能测试 / Basic functionality tests
- SignalsStrictTest - 严格模式测试 / Strict mode tests

## 📁 项目结构 / Project Layout

```
xswl-signals/
├── include/
│   └── xswl/
│       └── signals.hpp          # 单头文件库 / Single-header library
├── doc/
│   ├── API.md                   # 中文 API 文档 / Chinese API documentation
│   └── API_EN.md                # 英文 API 文档 / English API documentation
├── tests/                       # 测试代码 / Test code
│   ├── test_signals_base.cpp
│   └── test_signals_strict.cpp
├── examples/                    # 示例代码 / Example code
│   ├── basic.cpp
│   └── lifecycle.cpp
├── cmake/                       # CMake 配置 / CMake config files
├── CMakeLists.txt
├── build.sh                     # 构建脚本 / Build script
└── README.md
```

## 🎯 使用场景 / Use Cases

- **事件系统** / Event Systems - GUI 事件处理、游戏事件系统 / GUI event handling, game event systems
- **观察者模式** / Observer Pattern - 数据变化通知 / Data change notifications
- **插件系统** / Plugin Systems - 模块间通信 / Inter-module communication
- **回调管理** / Callback Management - 异步操作完成通知 / Async operation completion notifications

## ⚠️ 注意事项 / Notes

- 信号参数最多 6 个（可扩展）/ Maximum 6 signal parameters (extensible)
- 槽函数参数不能超过信号参数数量 / Slot parameters cannot exceed signal parameters
- 避免在槽函数中修改连接 / Avoid modifying connections in slot functions
- 使用 `shared_ptr` 连接成员函数以确保线程安全 / Use `shared_ptr` for member functions to ensure safety

## 🤝 贡献 / Contributing

💻 Vibe Coding...

欢迎提交 Issue 和 Pull Request！

Issues and Pull Requests are welcome!

## 📄 许可证 / License

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🔗 相关链接 / Links

- [GitHub Repository](https://github.com/Wang-Jianwei/xswl-signals)
- [API 文档 / API Documentation](doc/API.md)

---

**Made with ❤️ using modern C++11**