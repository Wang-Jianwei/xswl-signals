# xswl::signals API 文档

## 目录

- [概述](#概述)
- [核心类](#核心类)
  - [signal_t](#signal_t)
  - [connection_t](#connection_t)
  - [scoped_connection_t](#scoped_connection_t)
  - [connection_group_t](#connection_group_t)
- [特性说明](#特性说明)
  - [参数适配](#参数适配)
  - [优先级调度](#优先级调度)
  - [单次槽函数](#单次槽函数)
  - [生命周期管理](#生命周期管理)
  - [标签连接](#标签连接)
  - [线程安全性](#线程安全性)
- [使用示例](#使用示例)

---

## 概述

`xswl::signals` 是一个现代化的 C++11 信号/槽库，提供了类型安全、灵活且高效的事件处理机制。支持各种高级特性，如优先级调度、参数适配、自动生命周期管理等。

**主要特性：**
- Header-only 设计，易于集成
- 完整的连接生命周期管理
- 优先级调度支持
- 单次槽函数（connect_once）
- 自动参数适配（槽函数可接受比信号更少的参数）
- 成员函数绑定与自动生命周期跟踪
- 标签连接与批量断开
- 基础线程安全（使用原子操作和互斥锁）

---

## 核心类

### signal_t

信号类模板，用于定义和发射信号。

```cpp
template <typename... Args>
class signal_t;
```

#### 模板参数

- `Args...` - 信号携带的参数类型列表

#### 构造函数

```cpp
signal_t();                              // 默认构造
signal_t(signal_t&& other) noexcept;     // 移动构造
```

信号对象不可拷贝。

#### 连接方法

##### 1. 连接普通可调用对象

```cpp
template <typename Fn>
connection_t<Args...> connect(Fn&& func, int priority = 0);
```

**参数：**
- `func` - 可调用对象（lambda、函数指针、仿函数等）
- `priority` - 优先级（默认为 0，数值越大优先级越高）

**返回：** `connection_t<Args...>` 连接句柄

**说明：**
- 支持参数适配：槽函数可以接受比信号更少的参数（0-6个）
- 例如：`signal_t<int, string>` 可以连接到接受 `(int)` 或 `()` 的槽函数

**示例：**
```cpp
xswl::signal_t<int, std::string> sig;

// 完整参数
auto c1 = sig.connect([](int i, const std::string& s) {
    std::cout << i << ": " << s << "\n";
});

// 参数适配：只接受第一个参数
auto c2 = sig.connect([](int i) {
    std::cout << "只需要 int: " << i << "\n";
}, 10);  // 更高优先级

// 参数适配：不接受任何参数
auto c3 = sig.connect([]() {
    std::cout << "无参数槽函数\n";
});
```

##### 2. 单次连接

```cpp
template <typename Fn>
connection_t<Args...> connect_once(Fn&& func, int priority = 0);
```

**参数：**
- `func` - 可调用对象
- `priority` - 优先级

**返回：** `connection_t<Args...>` 连接句柄

**说明：**
- 槽函数只会被调用一次，之后自动断开
- 在多线程环境下使用 CAS 保证只执行一次
- 同样支持参数适配

**示例：**
```cpp
sig.connect_once([](int value) {
    std::cout << "这只会打印一次: " << value << "\n";
});

sig(1);  // 输出
sig(2);  // 不再输出
```

##### 3. 连接成员函数（shared_ptr）

```cpp
template <typename Obj, typename MemFn>
connection_t<Args...> connect(
    const std::shared_ptr<Obj>& obj,
    MemFn memfn,
    int priority = 0
);
```

**参数：**
- `obj` - 对象的 `shared_ptr`
- `memfn` - 成员函数指针
- `priority` - 优先级

**返回：** `connection_t<Args...>` 连接句柄

**说明：**
- 自动跟踪对象生命周期
- 当 `shared_ptr` 引用的对象被销毁后，槽函数将不再被调用
- 支持参数适配

**示例：**
```cpp
struct Receiver {
    void on_message(int id, const std::string& msg) {
        std::cout << "Received: " << id << " - " << msg << "\n";
    }
    
    void on_id(int id) {  // 只需要第一个参数
        std::cout << "ID: " << id << "\n";
    }
};

auto receiver = std::make_shared<Receiver>();
sig.connect(receiver, &Receiver::on_message);
sig.connect(receiver, &Receiver::on_id);  // 参数适配

receiver.reset();  // 销毁对象后，槽函数将不再被调用
```

##### 4. 连接成员函数（裸指针）

```cpp
template <typename Obj, typename MemFn>
connection_t<Args...> connect(
    Obj* obj,
    MemFn memfn,
    int priority = 0
);
```

**参数：**
- `obj` - 对象指针
- `memfn` - 成员函数指针
- `priority` - 优先级

**返回：** `connection_t<Args...>` 连接句柄

**说明：**
- 不跟踪对象生命周期
- 调用者需确保对象在信号发射时有效
- 支持参数适配

**示例：**
```cpp
Receiver receiver;
auto c = sig.connect(&receiver, &Receiver::on_message);
// 必须确保 receiver 在 c 断开前始终有效
```

##### 5. 标签连接

```cpp
template <typename Fn>
connection_t<Args...> connect(
    const std::string& tag,
    Fn&& func,
    int priority = 0
);
```

**参数：**
- `tag` - 连接标签（字符串）
- `func` - 可调用对象
- `priority` - 优先级

**返回：** `connection_t<Args...>` 连接句柄

**说明：**
- 为连接分配一个标签
- 可通过标签批量断开相同标签的所有连接
- 支持参数适配

**示例：**
```cpp
sig.connect("logger", [](int i, const std::string& s) {
    std::cout << "Log: " << i << " - " << s << "\n";
});

sig.connect("logger", [](int i) {
    std::cout << "Log ID: " << i << "\n";
});

// 断开所有标签为 "logger" 的连接
sig.disconnect("logger");
```

#### 发射信号

```cpp
void operator()(Args... args) const;
void emit_signal(Args... args) const;
```

**参数：**
- `args...` - 要传递给槽函数的参数

**说明：**
- 按优先级顺序调用所有已连接的槽函数
- 相同优先级的槽按连接顺序（稳定排序）
- 异常会被捕获并忽略，不影响其他槽函数执行
- `emit_signal` 与 `operator()` 等价

**示例：**
```cpp
xswl::signal_t<int, std::string> sig;
sig.connect([](int i, const std::string& s) { /* ... */ });

// 两种方式等价
sig(42, "hello");
sig.emit_signal(42, "hello");

// 配合 emit 宏使用（可选）
emit sig(42, "hello");
```

#### 管理方法

##### disconnect_all

```cpp
void disconnect_all();
```

断开所有连接。

**示例：**
```cpp
sig.disconnect_all();
```

##### disconnect (标签)

```cpp
bool disconnect(const std::string& tag);
```

**参数：**
- `tag` - 要断开的标签名

**返回：** 如果找到并断开了标签，返回 `true`；否则返回 `false`

**示例：**
```cpp
if (sig.disconnect("logger")) {
    std::cout << "已断开 logger\n";
}
```

##### slot_count

```cpp
std::size_t slot_count() const;
```

**返回：** 当前活跃的槽数量（不包括已标记删除的）

**示例：**
```cpp
std::cout << "活跃槽数量: " << sig.slot_count() << "\n";
```

##### empty

```cpp
bool empty() const;
```

**返回：** 如果没有活跃的槽，返回 `true`

**示例：**
```cpp
if (sig.empty()) {
    std::cout << "没有任何连接\n";
}
```

##### valid

```cpp
bool valid() const;
```

**返回：** 信号对象是否有效（内部实现存在）

---

### connection_t

连接句柄，用于管理单个槽连接。

```cpp
template <typename... Args>
class connection_t;
```

#### 构造与赋值

```cpp
connection_t();                                    // 默认构造（空连接）
connection_t(const connection_t&);                 // 拷贝构造
connection_t(connection_t&&);                      // 移动构造
connection_t& operator=(const connection_t&);      // 拷贝赋值
connection_t& operator=(connection_t&&);           // 移动赋值
```

#### 方法

##### is_connected

```cpp
bool is_connected() const;
explicit operator bool() const;
```

**返回：** 连接是否仍然有效

**示例：**
```cpp
auto c = sig.connect([]() { /* ... */ });
if (c.is_connected()) {
    std::cout << "连接有效\n";
}
// 或者
if (c) {
    std::cout << "连接有效\n";
}
```

##### disconnect

```cpp
void disconnect();
```

主动断开连接。

**示例：**
```cpp
auto c = sig.connect([]() { /* ... */ });
c.disconnect();  // 断开连接
```

##### block / unblock

```cpp
void block(bool b = true);
void unblock();
```

**参数：**
- `b` - 是否阻塞（默认为 `true`）

**说明：**
- 阻塞后，信号发射时将跳过此槽函数
- 不会断开连接，可以随时解除阻塞

**示例：**
```cpp
auto c = sig.connect([]() { /* ... */ });

c.block();        // 阻塞
sig();            // 不会调用此槽

c.unblock();      // 解除阻塞
sig();            // 会调用此槽
```

##### is_blocked

```cpp
bool is_blocked() const;
```

**返回：** 连接是否被阻塞

##### reset

```cpp
void reset();
```

释放连接引用（不会断开实际连接）。

---

### scoped_connection_t

RAII 风格的连接管理，在作用域结束时自动断开。

```cpp
class scoped_connection_t;
```

#### 构造与赋值

```cpp
scoped_connection_t();                                         // 默认构造
template <typename... Args>
scoped_connection_t(connection_t<Args...> c);                  // 从连接构造

scoped_connection_t(scoped_connection_t&& other) noexcept;     // 移动构造
scoped_connection_t& operator=(scoped_connection_t&&);         // 移动赋值

template <typename... Args>
scoped_connection_t& operator=(connection_t<Args...> c);       // 从连接赋值
```

不可拷贝。

#### 方法

##### disconnect

```cpp
void disconnect();
```

手动断开连接。

##### release

```cpp
void release();
```

释放管理权（不断开连接）。

#### 示例

```cpp
{
    xswl::scoped_connection_t scoped_c = sig.connect([]() {
        std::cout << "槽函数\n";
    });
    
    sig();  // 会调用
}  // 离开作用域，自动断开

sig();  // 不会调用
```

---

### connection_group_t

管理多个连接的容器。

```cpp
class connection_group_t;
```

#### 方法

##### add

```cpp
template <typename... Args>
void add(connection_t<Args...> c);
```

添加连接到组中。

##### operator+=

```cpp
template <typename... Args>
connection_group_t& operator+=(connection_t<Args...> c);
```

添加连接到组中（操作符形式）。

##### disconnect_all

```cpp
void disconnect_all();
```

断开组中所有连接。

##### size / empty

```cpp
std::size_t size() const;
bool empty() const;
```

查询组中连接数量。

#### 示例

```cpp
xswl::connection_group_t group;

group += sig1.connect([]() { /* ... */ });
group += sig2.connect([]() { /* ... */ });
group.add(sig3.connect([]() { /* ... */ }));

std::cout << "组中有 " << group.size() << " 个连接\n";

group.disconnect_all();  // 批量断开
```

---

## 特性说明

### 参数适配

信号可以连接到接受更少参数的槽函数。槽函数可以接受 0 到信号参数数量之间的任意参数（最多支持 6 个参数）。

**示例：**
```cpp
xswl::signal_t<int, std::string, double> sig;

// 三个参数
sig.connect([](int a, const std::string& b, double c) {
    std::cout << a << ", " << b << ", " << c << "\n";
});

// 两个参数（忽略第三个）
sig.connect([](int a, const std::string& b) {
    std::cout << a << ", " << b << "\n";
});

// 一个参数（只接受第一个）
sig.connect([](int a) {
    std::cout << a << "\n";
});

// 无参数
sig.connect([]() {
    std::cout << "triggered\n";
});

sig(42, "hello", 3.14);  // 所有槽都会被调用
```

### 优先级调度

槽函数按优先级从高到低执行，优先级相同时按连接顺序（稳定排序）。

**示例：**
```cpp
xswl::signal_t<int> sig;

sig.connect([](int v) {
    std::cout << "优先级 0: " << v << "\n";
}, 0);

sig.connect([](int v) {
    std::cout << "优先级 100: " << v << "\n";
}, 100);

sig.connect([](int v) {
    std::cout << "优先级 50: " << v << "\n";
}, 50);

sig(1);
// 输出顺序：
// 优先级 100: 1
// 优先级 50: 1
// 优先级 0: 1
```

### 单次槽函数

使用 `connect_once` 连接的槽函数只会执行一次，然后自动断开。

**线程安全：**
在多线程环境下，使用 CAS（Compare-And-Swap）确保单次槽只被一个线程执行。

**示例：**
```cpp
xswl::signal_t<> sig;

sig.connect_once([]() {
    std::cout << "只执行一次\n";
});

sig();  // 输出
sig();  // 不输出
```

### 生命周期管理

#### shared_ptr 自动跟踪

当使用 `shared_ptr` 连接成员函数时，库会自动跟踪对象生命周期：

```cpp
struct Handler {
    void on_event(int value) {
        std::cout << "处理: " << value << "\n";
    }
};

xswl::signal_t<int> sig;

{
    auto handler = std::make_shared<Handler>();
    sig.connect(handler, &Handler::on_event);
    
    sig(1);  // 输出：处理: 1
}  // handler 被销毁

sig(2);  // 不输出（对象已销毁，槽自动失效）
```

#### 裸指针（需手动管理）

使用裸指针时，必须确保对象在连接有效期内存活：

```cpp
Handler handler;
auto c = sig.connect(&handler, &Handler::on_event);

sig(1);  // OK

c.disconnect();  // 使用完毕后断开
// 或者确保 handler 的生命周期长于 c
```

### 标签连接

为连接分配标签，便于批量管理：

```cpp
xswl::signal_t<std::string> sig;

// 多个连接使用相同标签
sig.connect("ui", [](const std::string& msg) {
    std::cout << "UI1: " << msg << "\n";
});

sig.connect("ui", [](const std::string& msg) {
    std::cout << "UI2: " << msg << "\n";
});

sig.connect("logger", [](const std::string& msg) {
    std::cout << "Log: " << msg << "\n";
});

sig("test");
// 输出：
// UI1: test
// UI2: test
// Log: test

sig.disconnect("ui");  // 断开所有 "ui" 标签的连接

sig("test2");
// 输出：
// Log: test2
```

### 线程安全性

**基本线程安全保证：**
- 使用互斥锁保护连接列表的修改
- 使用原子操作管理槽状态（blocked、pending_removal 等）
- 单次槽使用 CAS 确保只执行一次

**注意事项：**
- 信号发射时会复制槽列表快照，避免长时间持锁
- 异常会被捕获，不影响其他槽函数
- 适用于典型的单线程和轻度多线程场景
- 不建议在高并发环境下频繁修改连接

**多线程示例：**
```cpp
xswl::signal_t<int> sig;

// 线程 1：连接
std::thread t1([&]() {
    sig.connect([](int v) { /* ... */ });
});

// 线程 2：发射
std::thread t2([&]() {
    sig(42);
});

t1.join();
t2.join();
```

---

## 使用示例

### 基础用法

```cpp
#include "xswl/signals.hpp"
#include <iostream>

int main() {
    xswl::signal_t<int, std::string> sig;

    // 连接 lambda
    auto c1 = sig.connect([](int id, const std::string& msg) {
        std::cout << "槽1: " << id << " - " << msg << "\n";
    });

    // 单次连接
    sig.connect_once([](int id, const std::string& msg) {
        std::cout << "单次: " << id << " - " << msg << "\n";
    });

    // 带优先级
    sig.connect([](int id, const std::string& msg) {
        std::cout << "高优先级: " << id << " - " << msg << "\n";
    }, 100);

    emit sig(1, "hello");
    // 输出顺序：
    // 高优先级: 1 - hello
    // 槽1: 1 - hello
    // 单次: 1 - hello

    emit sig(2, "world");
    // 输出顺序：
    // 高优先级: 2 - world
    // 槽1: 2 - world
    // （单次槽已移除）

    c1.disconnect();
    return 0;
}
```

### 成员函数连接

```cpp
#include "xswl/signals.hpp"
#include <iostream>
#include <memory>

struct MessageHandler {
    std::string name;
    
    MessageHandler(std::string n) : name(std::move(n)) {}
    
    void on_message(int id, const std::string& msg) {
        std::cout << name << " 收到: " << id << " - " << msg << "\n";
    }
    
    void on_id_only(int id) {  // 参数适配
        std::cout << name << " ID: " << id << "\n";
    }
};

int main() {
    xswl::signal_t<int, std::string> sig;

    auto handler1 = std::make_shared<MessageHandler>("处理器1");
    auto handler2 = std::make_shared<MessageHandler>("处理器2");

    sig.connect(handler1, &MessageHandler::on_message);
    sig.connect(handler2, &MessageHandler::on_message);
    sig.connect(handler1, &MessageHandler::on_id_only);  // 参数适配

    emit sig(1, "测试消息");
    // 输出：
    // 处理器1 收到: 1 - 测试消息
    // 处理器2 收到: 1 - 测试消息
    // 处理器1 ID: 1

    handler1.reset();  // 销毁 handler1

    emit sig(2, "第二条消息");
    // 输出：
    // 处理器2 收到: 2 - 第二条消息
    // （handler1 的槽已失效）

    return 0;
}
```

### 连接管理

```cpp
#include "xswl/signals.hpp"
#include <iostream>

int main() {
    xswl::signal_t<int> sig;

    // scoped_connection：自动管理
    {
        xswl::scoped_connection_t scoped = sig.connect([](int v) {
            std::cout << "作用域内: " << v << "\n";
        });
        
        sig(1);  // 输出：作用域内: 1
    }  // scoped 析构，自动断开

    sig(2);  // 不输出

    // connection_group：批量管理
    xswl::connection_group_t group;
    
    group += sig.connect([](int v) { std::cout << "槽1: " << v << "\n"; });
    group += sig.connect([](int v) { std::cout << "槽2: " << v << "\n"; });
    group += sig.connect([](int v) { std::cout << "槽3: " << v << "\n"; });

    sig(3);
    // 输出：
    // 槽1: 3
    // 槽2: 3
    // 槽3: 3

    group.disconnect_all();  // 批量断开
    sig(4);  // 不输出

    return 0;
}
```

### 阻塞与解除阻塞

```cpp
#include "xswl/signals.hpp"
#include <iostream>

int main() {
    xswl::signal_t<int> sig;

    auto c1 = sig.connect([](int v) {
        std::cout << "槽1: " << v << "\n";
    });

    auto c2 = sig.connect([](int v) {
        std::cout << "槽2: " << v << "\n";
    });

    sig(1);
    // 输出：
    // 槽1: 1
    // 槽2: 1

    c1.block();  // 阻塞槽1

    sig(2);
    // 输出：
    // 槽2: 2

    c1.unblock();  // 解除阻塞

    sig(3);
    // 输出：
    // 槽1: 3
    // 槽2: 3

    return 0;
}
```

### 完整应用示例

```cpp
#include "xswl/signals.hpp"
#include <iostream>
#include <memory>
#include <string>

// 事件系统
class EventSystem {
public:
    xswl::signal_t<const std::string&, int> on_event;
};

// 日志记录器
class Logger {
public:
    void log(const std::string& event, int value) {
        std::cout << "[LOG] " << event << ": " << value << "\n";
    }
};

// UI 处理器
class UIHandler {
public:
    void update(const std::string& event, int value) {
        std::cout << "[UI] 更新: " << event << " = " << value << "\n";
    }
    
    void on_value(int value) {  // 参数适配
        std::cout << "[UI] 值变化: " << value << "\n";
    }
};

int main() {
    EventSystem events;
    
    auto logger = std::make_shared<Logger>();
    auto ui = std::make_shared<UIHandler>();

    // 连接组件
    events.on_event.connect(logger, &Logger::log, 100);  // 高优先级
    events.on_event.connect(ui, &UIHandler::update);
    events.on_event.connect(ui, &UIHandler::on_value);  // 参数适配

    // 统计（单次）
    events.on_event.connect_once([](const std::string& event, int value) {
        std::cout << "[STATS] 首次事件: " << event << " = " << value << "\n";
    }, 50);

    // 触发事件
    emit events.on_event("temperature", 25);
    std::cout << "---\n";
    emit events.on_event("pressure", 1013);

    return 0;
}

// 输出：
// [LOG] temperature: 25
// [STATS] 首次事件: temperature = 25
// [UI] 更新: temperature = 25
// [UI] 值变化: 25
// ---
// [LOG] pressure: 1013
// [UI] 更新: pressure = 1013
// [UI] 值变化: 1013
```

---

## 最佳实践

1. **使用 shared_ptr 管理对象生命周期**
   - 连接成员函数时优先使用 `shared_ptr`，自动管理生命周期

2. **合理使用优先级**
   - 日志、监控等应使用较高优先级
   - 业务逻辑使用默认优先级（0）
   - UI 更新使用较低优先级

3. **使用 scoped_connection 或 connection_group**
   - 临时连接使用 `scoped_connection_t`
   - 多个相关连接使用 `connection_group_t` 统一管理

4. **标签用于分类管理**
   - 给相关连接分配相同标签，便于批量断开

5. **异常安全**
   - 槽函数中的异常会被捕获，不影响其他槽
   - 但应避免槽函数抛出异常

6. **性能考虑**
   - 信号发射时会复制槽列表，避免在槽函数中修改连接
   - 频繁发射的信号应避免过多的槽连接

---

## 限制

- 信号参数最多 6 个（可通过修改源码扩展）
- 槽函数参数数量不能超过信号参数数量
- 不保证槽函数的执行顺序（除优先级外）
- 槽函数应避免长时间阻塞

---

## 常见问题

**Q: 如何确保槽函数只执行一次？**
A: 使用 `connect_once` 方法。

**Q: 如何在槽函数中安全地断开连接？**
A: 可以在槽函数中调用 `connection.disconnect()`，断开操作是线程安全的。

**Q: 成员函数连接时，对象被销毁会发生什么？**
A: 如果使用 `shared_ptr`，槽会自动失效；如果使用裸指针，必须手动断开连接。

**Q: 信号可以返回值吗？**
A: 当前版本不支持返回值。可以通过传递引用参数或回调函数实现类似功能。

**Q: 可以在多线程中使用吗？**
A: 可以，但注意槽函数本身需要是线程安全的。

**Q: 如何调试连接问题？**
A: 使用 `slot_count()` 检查活跃连接数，使用 `is_connected()` 检查连接状态。

---

## 版本历史

当前版本基于 C++11 标准，提供核心信号/槽功能。

---

## 许可证

本项目采用 MIT 许可证。详见项目根目录的 [LICENSE](../LICENSE) 文件。
