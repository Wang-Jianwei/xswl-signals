#include "xswl/signals.hpp"

#include <iostream>
#include <memory>
#include <string>

// 生命周期与成员函数示例：
// - 使用 shared_ptr 持有接收者，信号不会调用已销毁对象。
// - 展示 tag 连接与按标签断开。

struct Receiver : public std::enable_shared_from_this<Receiver>
{
    std::string name;
    explicit Receiver(std::string n) : name(std::move(n)) {}

    void on_message(int id, const std::string& msg)
    {
        std::cout << "recv(" << name << ") id=" << id << " msg=" << msg << "\n";
    }
};

int main()
{
    xswl::signal_t<int, std::string> sig_message;

    auto r1 = std::make_shared<Receiver>("alpha");
    auto r2 = std::make_shared<Receiver>("beta");

    // 连接成员函数（shared_ptr 自动管理生命周期）
    sig_message.connect(r1, &Receiver::on_message);
    sig_message.connect(r2, &Receiver::on_message);

    // 通过 tag 连接，方便选择性断开
    sig_message.connect("logger", [](int id, const std::string& msg) {
        std::cout << "log | id=" << id << " msg=" << msg << "\n";
    });

    std::cout << "-- emit with two receivers --\n";
    emit sig_message(1, "hi receivers");

    // 手动移除 logger
    sig_message.disconnect("logger");

    std::cout << "-- emit after removing logger --\n";
    emit sig_message(2, "no logger now");

    // r1 生命周期结束后，信号不会再调用它
    r1.reset();

    std::cout << "-- emit after r1 reset --\n";
    emit sig_message(3, "only beta alive");

    return 0;
}
