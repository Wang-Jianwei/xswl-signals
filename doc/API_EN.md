# xswl::signals API Documentation

## Table of Contents

- [Overview](#overview)
- [Core Classes](#core-classes)
  - [signal_t](#signal_t)
  - [connection_t](#connection_t)
  - [scoped_connection_t](#scoped_connection_t)
  - [connection_group_t](#connection_group_t)
- [Features](#features)
  - [Parameter Adaptation](#parameter-adaptation)
  - [Priority Dispatch](#priority-dispatch)
  - [Single-Shot Slots](#single-shot-slots)
  - [Lifetime Management](#lifetime-management)
  - [Tagged Connections](#tagged-connections)
  - [Thread Safety](#thread-safety)
- [Usage Examples](#usage-examples)

---

## Overview

`xswl::signals` is a modern C++11 signals/slots library providing type-safe, flexible, and efficient event handling mechanisms. It supports advanced features like priority dispatch, parameter adaptation, and automatic lifetime management.

**Key Features:**
- Header-only design for easy integration
- Complete connection lifecycle management
- Priority-based dispatch support
- Single-shot slots (connect_once)
- Automatic parameter adaptation (slots can accept fewer parameters than signals)
- Member function binding with automatic lifetime tracking
- Tagged connections for batch disconnection
- Basic thread safety (using atomics and mutexes)

---

## Core Classes

### signal_t

Signal class template for defining and emitting signals.

```cpp
template <typename... Args>
class signal_t;
```

#### Template Parameters

- `Args...` - Parameter types carried by the signal

#### Constructors

```cpp
signal_t();                              // Default constructor
signal_t(signal_t&& other) noexcept;     // Move constructor
```

Signal objects are not copyable.

#### Connection Methods

##### 1. Connect Callable Objects

```cpp
template <typename Fn>
connection_t<Args...> connect(Fn&& func, int priority = 0);
```

**Parameters:**
- `func` - Callable object (lambda, function pointer, functor, etc.)
- `priority` - Priority (default 0, higher values have higher priority)

**Returns:** `connection_t<Args...>` connection handle

**Notes:**
- Supports parameter adaptation: slots can accept fewer parameters than signals (0-6 parameters)
- Example: `signal_t<int, string>` can connect to slots accepting `(int)` or `()`

**Example:**
```cpp
xswl::signal_t<int, std::string> sig;

// Full parameters
auto c1 = sig.connect([](int i, const std::string& s) {
    std::cout << i << ": " << s << "\n";
});

// Parameter adaptation: only first parameter
auto c2 = sig.connect([](int i) {
    std::cout << "Only int: " << i << "\n";
}, 10);  // Higher priority

// Parameter adaptation: no parameters
auto c3 = sig.connect([]() {
    std::cout << "No parameters\n";
});
```

##### 2. Single-Shot Connection

```cpp
template <typename Fn>
connection_t<Args...> connect_once(Fn&& func, int priority = 0);
```

**Parameters:**
- `func` - Callable object
- `priority` - Priority

**Returns:** `connection_t<Args...>` connection handle

**Notes:**
- Slot is called only once, then automatically disconnected
- Uses CAS in multithreaded environments to ensure single execution
- Also supports parameter adaptation

**Example:**
```cpp
sig.connect_once([](int value) {
    std::cout << "This prints only once: " << value << "\n";
});

sig(1);  // Prints
sig(2);  // Does not print
```

##### 3. Connect Member Function (shared_ptr)

```cpp
template <typename Obj, typename MemFn>
connection_t<Args...> connect(
    const std::shared_ptr<Obj>& obj,
    MemFn memfn,
    int priority = 0
);
```

**Parameters:**
- `obj` - `shared_ptr` to the object
- `memfn` - Member function pointer
- `priority` - Priority

**Returns:** `connection_t<Args...>` connection handle

**Notes:**
- Automatically tracks object lifetime
- Slot will not be called after the `shared_ptr` object is destroyed
- Supports parameter adaptation

**Example:**
```cpp
struct Receiver {
    void on_message(int id, const std::string& msg) {
        std::cout << "Received: " << id << " - " << msg << "\n";
    }
    
    void on_id(int id) {  // Only needs first parameter
        std::cout << "ID: " << id << "\n";
    }
};

auto receiver = std::make_shared<Receiver>();
sig.connect(receiver, &Receiver::on_message);
sig.connect(receiver, &Receiver::on_id);  // Parameter adaptation

receiver.reset();  // After destruction, slot will no longer be called
```

##### 4. Connect Member Function (Raw Pointer)

```cpp
template <typename Obj, typename MemFn>
connection_t<Args...> connect(
    Obj* obj,
    MemFn memfn,
    int priority = 0
);
```

**Parameters:**
- `obj` - Object pointer
- `memfn` - Member function pointer
- `priority` - Priority

**Returns:** `connection_t<Args...>` connection handle

**Notes:**
- Does not track object lifetime
- Caller must ensure object is valid when signal is emitted
- Supports parameter adaptation

**Example:**
```cpp
Receiver receiver;
auto c = sig.connect(&receiver, &Receiver::on_message);
// Must ensure receiver is valid before c is disconnected
```

##### 5. Tagged Connection

```cpp
template <typename Fn>
connection_t<Args...> connect(
    const std::string& tag,
    Fn&& func,
    int priority = 0
);
```

**Parameters:**
- `tag` - Connection tag (string)
- `func` - Callable object
- `priority` - Priority

**Returns:** `connection_t<Args...>` connection handle

**Notes:**
- Assigns a tag to the connection
- All connections with the same tag can be disconnected in batch
- Supports parameter adaptation

**Example:**
```cpp
sig.connect("logger", [](int i, const std::string& s) {
    std::cout << "Log: " << i << " - " << s << "\n";
});

sig.connect("logger", [](int i) {
    std::cout << "Log ID: " << i << "\n";
});

// Disconnect all connections with tag "logger"
sig.disconnect("logger");
```

#### Emitting Signals

```cpp
void operator()(Args... args) const;
void emit_signal(Args... args) const;
```

**Parameters:**
- `args...` - Parameters to pass to slot functions

**Notes:**
- Calls all connected slots in priority order
- Slots with same priority are called in connection order (stable sort)
- Exceptions are caught and ignored, won't affect other slots
- `emit_signal` is equivalent to `operator()`

**Example:**
```cpp
xswl::signal_t<int, std::string> sig;
sig.connect([](int i, const std::string& s) { /* ... */ });

// Both are equivalent
sig(42, "hello");
sig.emit_signal(42, "hello");

// Use with emit macro (optional)
emit sig(42, "hello");
```

#### Management Methods

##### disconnect_all

```cpp
void disconnect_all();
```

Disconnect all connections.

##### disconnect (tag)

```cpp
bool disconnect(const std::string& tag);
```

**Parameters:**
- `tag` - Tag name to disconnect

**Returns:** `true` if tag was found and disconnected; otherwise `false`

##### slot_count

```cpp
std::size_t slot_count() const;
```

**Returns:** Number of active slots (excluding marked for deletion)

##### empty

```cpp
bool empty() const;
```

**Returns:** `true` if there are no active slots

##### valid

```cpp
bool valid() const;
```

**Returns:** Whether the signal object is valid (internal implementation exists)

---

### connection_t

Connection handle for managing individual slot connections.

```cpp
template <typename... Args>
class connection_t;
```

#### Construction and Assignment

```cpp
connection_t();                                    // Default constructor (empty connection)
connection_t(const connection_t&);                 // Copy constructor
connection_t(connection_t&&);                      // Move constructor
connection_t& operator=(const connection_t&);      // Copy assignment
connection_t& operator=(connection_t&&);           // Move assignment
```

#### Methods

##### is_connected

```cpp
bool is_connected() const;
explicit operator bool() const;
```

**Returns:** Whether the connection is still valid

##### disconnect

```cpp
void disconnect();
```

Manually disconnect the connection.

##### block / unblock

```cpp
void block(bool b = true);
void unblock();
```

**Parameters:**
- `b` - Whether to block (default `true`)

**Notes:**
- When blocked, signal emission will skip this slot
- Does not disconnect, can be unblocked at any time

##### is_blocked

```cpp
bool is_blocked() const;
```

**Returns:** Whether the connection is blocked

##### reset

```cpp
void reset();
```

Release connection reference (does not disconnect the actual connection).

---

### scoped_connection_t

RAII-style connection management, automatically disconnects when scope ends.

```cpp
class scoped_connection_t;
```

#### Construction and Assignment

```cpp
scoped_connection_t();                                         // Default constructor
template <typename... Args>
scoped_connection_t(connection_t<Args...> c);                  // Construct from connection

scoped_connection_t(scoped_connection_t&& other) noexcept;     // Move constructor
scoped_connection_t& operator=(scoped_connection_t&&);         // Move assignment

template <typename... Args>
scoped_connection_t& operator=(connection_t<Args...> c);       // Assign from connection
```

Not copyable.

#### Methods

##### disconnect

```cpp
void disconnect();
```

Manually disconnect.

##### release

```cpp
void release();
```

Release ownership (does not disconnect).

#### Example

```cpp
{
    xswl::scoped_connection_t scoped_c = sig.connect([]() {
        std::cout << "Slot function\n";
    });
    
    sig();  // Will call
}  // Leaves scope, automatically disconnects

sig();  // Will not call
```

---

### connection_group_t

Container for managing multiple connections.

```cpp
class connection_group_t;
```

#### Methods

##### add

```cpp
template <typename... Args>
void add(connection_t<Args...> c);
```

Add connection to group.

##### operator+=

```cpp
template <typename... Args>
connection_group_t& operator+=(connection_t<Args...> c);
```

Add connection to group (operator form).

##### disconnect_all

```cpp
void disconnect_all();
```

Disconnect all connections in the group.

##### size / empty

```cpp
std::size_t size() const;
bool empty() const;
```

Query number of connections in the group.

#### Example

```cpp
xswl::connection_group_t group;

group += sig1.connect([]() { /* ... */ });
group += sig2.connect([]() { /* ... */ });
group.add(sig3.connect([]() { /* ... */ }));

std::cout << "Group has " << group.size() << " connections\n";

group.disconnect_all();  // Batch disconnect
```

---

## Features

### Parameter Adaptation

Signals can connect to slots that accept fewer parameters. Slots can accept anywhere from 0 to the number of signal parameters (up to 6 parameters supported).

**Example:**
```cpp
xswl::signal_t<int, std::string, double> sig;

// Three parameters
sig.connect([](int a, const std::string& b, double c) {
    std::cout << a << ", " << b << ", " << c << "\n";
});

// Two parameters (ignores third)
sig.connect([](int a, const std::string& b) {
    std::cout << a << ", " << b << "\n";
});

// One parameter (only accepts first)
sig.connect([](int a) {
    std::cout << a << "\n";
});

// No parameters
sig.connect([]() {
    std::cout << "triggered\n";
});

sig(42, "hello", 3.14);  // All slots will be called
```

### Priority Dispatch

Slots execute in order from highest to lowest priority, with stable ordering for equal priorities.

**Example:**
```cpp
xswl::signal_t<int> sig;

sig.connect([](int v) {
    std::cout << "Priority 0: " << v << "\n";
}, 0);

sig.connect([](int v) {
    std::cout << "Priority 100: " << v << "\n";
}, 100);

sig.connect([](int v) {
    std::cout << "Priority 50: " << v << "\n";
}, 50);

sig(1);
// Output order:
// Priority 100: 1
// Priority 50: 1
// Priority 0: 1
```

### Single-Shot Slots

Slots connected with `connect_once` execute only once, then automatically disconnect.

**Thread Safety:**
In multithreaded environments, uses CAS (Compare-And-Swap) to ensure single-shot slots are executed by only one thread.

**Example:**
```cpp
xswl::signal_t<> sig;

sig.connect_once([]() {
    std::cout << "Executes only once\n";
});

sig();  // Prints
sig();  // Does not print
```

### Lifetime Management

#### Automatic tracking with shared_ptr

When connecting member functions with `shared_ptr`, the library automatically tracks object lifetime:

```cpp
struct Handler {
    void on_event(int value) {
        std::cout << "Handling: " << value << "\n";
    }
};

xswl::signal_t<int> sig;

{
    auto handler = std::make_shared<Handler>();
    sig.connect(handler, &Handler::on_event);
    
    sig(1);  // Prints: Handling: 1
}  // handler destroyed

sig(2);  // Does not print (object destroyed, slot automatically invalidated)
```

#### Raw pointers (manual management required)

When using raw pointers, you must ensure the object remains valid for the connection lifetime:

```cpp
Handler handler;
auto c = sig.connect(&handler, &Handler::on_event);

sig(1);  // OK

c.disconnect();  // Disconnect when done
// Or ensure handler lifetime exceeds c
```

### Tagged Connections

Assign tags to connections for batch management:

```cpp
xswl::signal_t<std::string> sig;

// Multiple connections with same tag
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
// Output:
// UI1: test
// UI2: test
// Log: test

sig.disconnect("ui");  // Disconnect all "ui" tagged connections

sig("test2");
// Output:
// Log: test2
```

### Thread Safety

**Basic Thread Safety Guarantees:**
- Mutex protects connection list modifications
- Atomic operations manage slot states (blocked, pending_removal, etc.)
- Single-shot slots use CAS to ensure single execution

**Notes:**
- Signal emission copies slot list snapshot to avoid long-held locks
- Exceptions are caught and won't affect other slots
- Suitable for typical single-threaded and light multithreaded scenarios
- Not recommended for high-concurrency environments with frequent connection modifications

**Multithreaded Example:**
```cpp
xswl::signal_t<int> sig;

// Thread 1: connect
std::thread t1([&]() {
    sig.connect([](int v) { /* ... */ });
});

// Thread 2: emit
std::thread t2([&]() {
    sig(42);
});

t1.join();
t2.join();
```

---

## Usage Examples

### Basic Usage

```cpp
#include "xswl/signals.hpp"
#include <iostream>

int main() {
    xswl::signal_t<int, std::string> sig;

    // Connect lambda
    auto c1 = sig.connect([](int id, const std::string& msg) {
        std::cout << "Slot1: " << id << " - " << msg << "\n";
    });

    // Single-shot connection
    sig.connect_once([](int id, const std::string& msg) {
        std::cout << "Once: " << id << " - " << msg << "\n";
    });

    // With priority
    sig.connect([](int id, const std::string& msg) {
        std::cout << "High priority: " << id << " - " << msg << "\n";
    }, 100);

    emit sig(1, "hello");
    // Output order:
    // High priority: 1 - hello
    // Slot1: 1 - hello
    // Once: 1 - hello

    emit sig(2, "world");
    // Output order:
    // High priority: 2 - world
    // Slot1: 2 - world
    // (Once slot removed)

    c1.disconnect();
    return 0;
}
```

### Member Function Connection

```cpp
#include "xswl/signals.hpp"
#include <iostream>
#include <memory>

struct MessageHandler {
    std::string name;
    
    MessageHandler(std::string n) : name(std::move(n)) {}
    
    void on_message(int id, const std::string& msg) {
        std::cout << name << " received: " << id << " - " << msg << "\n";
    }
    
    void on_id_only(int id) {  // Parameter adaptation
        std::cout << name << " ID: " << id << "\n";
    }
};

int main() {
    xswl::signal_t<int, std::string> sig;

    auto handler1 = std::make_shared<MessageHandler>("Handler1");
    auto handler2 = std::make_shared<MessageHandler>("Handler2");

    sig.connect(handler1, &MessageHandler::on_message);
    sig.connect(handler2, &MessageHandler::on_message);
    sig.connect(handler1, &MessageHandler::on_id_only);  // Parameter adaptation

    emit sig(1, "test message");
    // Output:
    // Handler1 received: 1 - test message
    // Handler2 received: 1 - test message
    // Handler1 ID: 1

    handler1.reset();  // Destroy handler1

    emit sig(2, "second message");
    // Output:
    // Handler2 received: 2 - second message
    // (handler1 slots invalidated)

    return 0;
}
```

### Connection Management

```cpp
#include "xswl/signals.hpp"
#include <iostream>

int main() {
    xswl::signal_t<int> sig;

    // scoped_connection: automatic management
    {
        xswl::scoped_connection_t scoped = sig.connect([](int v) {
            std::cout << "In scope: " << v << "\n";
        });
        
        sig(1);  // Prints: In scope: 1
    }  // scoped destructs, automatically disconnects

    sig(2);  // Does not print

    // connection_group: batch management
    xswl::connection_group_t group;
    
    group += sig.connect([](int v) { std::cout << "Slot1: " << v << "\n"; });
    group += sig.connect([](int v) { std::cout << "Slot2: " << v << "\n"; });
    group += sig.connect([](int v) { std::cout << "Slot3: " << v << "\n"; });

    sig(3);
    // Output:
    // Slot1: 3
    // Slot2: 3
    // Slot3: 3

    group.disconnect_all();  // Batch disconnect
    sig(4);  // Does not print

    return 0;
}
```

---

## Best Practices

1. **Use shared_ptr for lifetime management**
   - Prefer `shared_ptr` when connecting member functions for automatic lifetime management

2. **Use priorities appropriately**
   - Logging, monitoring should use higher priorities
   - Business logic uses default priority (0)
   - UI updates use lower priorities

3. **Use scoped_connection or connection_group**
   - Temporary connections use `scoped_connection_t`
   - Multiple related connections use `connection_group_t` for unified management

4. **Tags for categorization**
   - Assign same tag to related connections for batch disconnection

5. **Exception safety**
   - Exceptions in slots are caught and won't affect others
   - But avoid throwing exceptions from slots

6. **Performance considerations**
   - Signal emission copies slot list, avoid modifying connections in slots
   - Frequently emitted signals should avoid too many slot connections

---

## Limitations

- Maximum 6 signal parameters (can be extended by modifying source)
- Slot parameter count cannot exceed signal parameter count
- No guaranteed slot execution order (except by priority)
- Slots should avoid long blocking operations

---

## FAQ

**Q: How to ensure a slot executes only once?**
A: Use the `connect_once` method.

**Q: How to safely disconnect in a slot function?**
A: You can call `connection.disconnect()` in a slot function; disconnection is thread-safe.

**Q: What happens when an object is destroyed with member function connections?**
A: If using `shared_ptr`, slots automatically invalidate; if using raw pointers, you must manually disconnect.

**Q: Can signals return values?**
A: Current version doesn't support return values. Use reference parameters or callback functions for similar functionality.

**Q: Can it be used in multithreaded environments?**
A: Yes, but note that slot functions themselves need to be thread-safe.

**Q: How to debug connection issues?**
A: Use `slot_count()` to check active connection count, `is_connected()` to check connection status.

---

## Version History

Current version based on C++11 standard, provides core signals/slots functionality.

---

## License

This project is licensed under the MIT License. Please refer to the [LICENSE](../LICENSE) file in the project root directory.
