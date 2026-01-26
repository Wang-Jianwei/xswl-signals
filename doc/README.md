# xswl::signals 文档 / Documentation

欢迎查阅 xswl::signals 库的详细文档！

Welcome to the xswl::signals library documentation!

## 📚 文档列表 / Documentation List

### API 参考 / API Reference

- **[API.md](API.md)** - 中文 API 详细文档
  - 完整的类和方法说明
  - 详细的使用示例
  - 最佳实践指南
  - 常见问题解答

- **[API_EN.md](API_EN.md)** - English API Documentation
  - Complete class and method reference
  - Detailed usage examples
  - Best practices guide
  - FAQ

## 🚀 快速链接 / Quick Links

### 核心类 / Core Classes

1. **[signal_t](API.md#signal_t)** - 信号类 / Signal class
   - 创建和发射信号 / Creating and emitting signals
   - 连接槽函数 / Connecting slots
   - 优先级管理 / Priority management

2. **[connection_t](API.md#connection_t)** - 连接句柄 / Connection handle
   - 连接状态管理 / Connection state management
   - 阻塞/解除阻塞 / Block/unblock
   - 手动断开 / Manual disconnection

3. **[scoped_connection_t](API.md#scoped_connection_t)** - RAII 连接 / RAII connection
   - 自动生命周期管理 / Automatic lifetime management

4. **[connection_group_t](API.md#connection_group_t)** - 连接组 / Connection group
   - 批量连接管理 / Batch connection management

### 特性说明 / Features

- **[参数适配 / Parameter Adaptation](API.md#参数适配)** - 槽函数可接受更少参数
- **[优先级调度 / Priority Dispatch](API.md#优先级调度)** - 控制槽函数执行顺序
- **[单次槽函数 / Single-Shot Slots](API.md#单次槽函数)** - 只执行一次的槽
- **[生命周期管理 / Lifetime Management](API.md#生命周期管理)** - 自动对象跟踪
- **[标签连接 / Tagged Connections](API.md#标签连接)** - 批量断开相关连接
- **[线程安全性 / Thread Safety](API.md#线程安全性)** - 多线程环境下的使用

## 📖 学习路径 / Learning Path

### 初学者 / Beginners

1. 阅读 [README.md](../README.md) 了解项目概览
2. 查看 [快速开始](../README.md#-快速开始--quick-start)
3. 运行 [examples/basic.cpp](../examples/basic.cpp)
4. 阅读 [API 基础部分](API.md#核心类)

### 进阶用户 / Advanced Users

1. 学习 [参数适配](API.md#参数适配) 高级特性
2. 掌握 [生命周期管理](API.md#生命周期管理)
3. 了解 [线程安全性](API.md#线程安全性)
4. 查看 [最佳实践](API.md#最佳实践)

## 💡 示例代码 / Example Code

所有示例代码都可以在以下位置找到：

All example code can be found at:

- [examples/basic.cpp](../examples/basic.cpp) - 基础用法演示
- [examples/lifecycle.cpp](../examples/lifecycle.cpp) - 生命周期管理演示

## 🔍 查找内容 / Finding Content

### 按功能查找 / By Feature

- **信号创建** → [signal_t 构造函数](API.md#构造函数)
- **连接槽函数** → [connect 方法](API.md#连接方法)
- **单次连接** → [connect_once](API.md#2-单次连接)
- **成员函数绑定** → [连接成员函数](API.md#3-连接成员函数shared_ptr)
- **优先级控制** → [优先级调度](API.md#优先级调度)
- **生命周期** → [生命周期管理](API.md#生命周期管理)
- **标签管理** → [标签连接](API.md#标签连接)

### 按问题查找 / By Problem

- **如何确保只执行一次？** → [单次槽函数](API.md#单次槽函数)
- **如何管理对象生命周期？** → [生命周期管理](API.md#生命周期管理)
- **如何批量断开连接？** → [标签连接](API.md#标签连接) 或 [connection_group_t](API.md#connection_group_t)
- **槽函数参数可以不同吗？** → [参数适配](API.md#参数适配)
- **是否线程安全？** → [线程安全性](API.md#线程安全性)

## 🛠️ 开发工具 / Development Tools

### 调试技巧 / Debugging Tips

```cpp
// 检查连接数量
std::cout << "Active slots: " << sig.slot_count() << "\n";

// 检查连接状态
if (conn.is_connected()) {
    std::cout << "Connection is valid\n";
}

// 检查是否被阻塞
if (conn.is_blocked()) {
    std::cout << "Connection is blocked\n";
}
```

### 常见错误 / Common Mistakes

1. **忘记保持连接句柄** - 无法手动管理连接
   ```cpp
   // ⚠️ 注意：连接仍然有效，但无法手动断开或管理
   sig.connect([]() { /* ... */ });  // 槽函数会一直存在
   
   // ✅ 推荐：保存连接句柄以便后续管理
   auto conn = sig.connect([]() { /* ... */ });
   conn.disconnect();  // 可以手动断开
   ```

2. **使用裸指针但对象已销毁** - 未定义行为
   ```cpp
   // ❌ 危险
   {
       Handler h;
       sig.connect(&h, &Handler::on_event);
   }  // h 被销毁
   sig(1);  // 未定义行为！
   
   // ✅ 正确：使用 shared_ptr
   auto h = std::make_shared<Handler>();
   sig.connect(h, &Handler::on_event);
   ```

3. **在槽函数中修改连接** - 可能导致竞态条件
   ```cpp
   // ⚠️ 谨慎使用
   sig.connect([&]() {
       sig.disconnect_all();  // 在信号发射期间修改连接
   });
   ```

## 📞 获取帮助 / Getting Help

- 查看 [FAQ](API.md#常见问题)
- 阅读 [使用示例](API.md#使用示例)
- 提交 [GitHub Issue](https://github.com/Wang-Jianwei/xswl-signals/issues)

## 📝 文档更新 / Documentation Updates

文档版本：2026-01-26

Documentation Version: 2026-01-26

---

[返回主页 / Back to Main](../README.md)
