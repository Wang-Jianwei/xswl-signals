#include "xswl/signals.hpp"

#include <iostream>
#include <string>

// 基本用法示例：
// 1) 连接普通 lambda
// 2) 单次连接 connect_once
// 3) 按优先级调用
int main()
{
    xswl::signal_t<int, std::string> sig_message;

    // 普通连接
    auto conn_log = sig_message.connect([](int id, const std::string& msg) {
        std::cout << "log    | id=" << id << " msg=" << msg << "\n";
    });

    // 单次连接，只触发一次
    sig_message.connect_once([](int id, const std::string& msg) {
        std::cout << "once   | id=" << id << " msg=" << msg << " (runs once)\n";
    });

    // 带优先级的连接，数值越大越先执行
    sig_message.connect([](int id, const std::string& msg) {
        std::cout << "prio50 | id=" << id << " msg=" << msg << "\n";
    }, /*priority*/ 50);

    sig_message.connect([](int id, const std::string& msg) {
        std::cout << "prio10 | id=" << id << " msg=" << msg << "\n";
    }, /*priority*/ 10);

    std::cout << "-- first emit --\n";
    emit sig_message(1, "hello world");

    std::cout << "-- second emit (single-shot gone) --\n";
    emit sig_message(2, "hello again");

    // 断开连接示例
    conn_log.disconnect();
    std::cout << "-- third emit (log disconnected) --\n";
    emit sig_message(3, "after disconnect");

    return 0;
}
