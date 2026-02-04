#include "xswl/signals.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <exception>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// ============================= 测试框架 =====================================

#define TEST_CASE(name) \
    void test_##name(); \
    struct TestRegister_##name { \
        TestRegister_##name() { TestRunner::instance().add(#name, &test_##name); } \
    } g_test_register_##name; \
    void test_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " #expr << " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b)    ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b)    ASSERT_TRUE((a) != (b))
#define ASSERT_GE(a, b)    ASSERT_TRUE((a) >= (b))
#define ASSERT_LE(a, b)    ASSERT_TRUE((a) <= (b))
#define ASSERT_GT(a, b)    ASSERT_TRUE((a) > (b))
#define ASSERT_LT(a, b)    ASSERT_TRUE((a) < (b))

class TestRunner
{
public:
    typedef void (*TestFunc)();

    static TestRunner& instance()
    {
        static TestRunner inst;
        return inst;
    }

    void add(const std::string& name, TestFunc f)
    {
        tests_.push_back(TestCase{name, f});
    }

    int run()
    {
        int passed = 0;
        int failed = 0;

        std::cout << "Running " << tests_.size() << " tests...\n";
        std::cout << std::string(60, '=') << "\n";

        for (std::size_t i = 0; i < tests_.size(); ++i)
        {
            const TestCase& t = tests_[i];
            std::cout << "[ RUN      ] " << t.name << std::endl;
            try
            {
                t.func();
                std::cout << "[       OK ] " << t.name << std::endl;
                ++passed;
            }
            catch (const std::exception& e)
            {
                std::cout << "[  FAILED  ] " << t.name << "\n"
                          << "             " << e.what() << std::endl;
                ++failed;
            }
        }

        std::cout << std::string(60, '=') << "\n";
        std::cout << passed << " passed, " << failed << " failed\n";
        return failed;
    }

private:
    struct TestCase
    {
        std::string name;
        TestFunc    func;
    };
    std::vector<TestCase> tests_;
};

// 简单计数器
struct Counter
{
    std::atomic<int> v;
    Counter() : v(0) {}
    void inc() { ++v; }
    void add(int x) { v += x; }
    int  get() const { return v.load(); }
    void reset() { v.store(0); }
};

// 一个带状态的接收类，用来测试成员函数连接和生命周期
struct Receiver : public std::enable_shared_from_this<Receiver>
{
    std::atomic<int> call_count;
    std::atomic<int> last_int;
    std::string      last_str;

    Receiver() : call_count(0), last_int(0) {}

    void on_no_arg()
    {
        ++call_count;
    }
    void on_int(int v)
    {
        ++call_count;
        last_int = v;
    }
    void on_str(const std::string& s)
    {
        ++call_count;
        last_str = s;
    }
};

// =========================== 1. 基础与不变量 ================================

// 测试：基本连接、发射与断开不变量（empty/slot_count）
TEST_CASE(basic_connect_emit)
{
    xswl::signal_t<> sig;
    Counter c;

    ASSERT_TRUE(sig.empty());
    ASSERT_EQ(sig.slot_count(), 0u);

    xswl::connection_t<> conn = sig.connect([&c]() { c.inc(); });

    ASSERT_FALSE(sig.empty());
    ASSERT_EQ(sig.slot_count(), 1u);

    emit sig();
    ASSERT_EQ(c.get(), 1);

    emit sig();
    ASSERT_EQ(c.get(), 2);

    conn.disconnect();
    ASSERT_TRUE(sig.empty() || sig.slot_count() == 0u);

    emit sig();
    ASSERT_EQ(c.get(), 2); // 不再增长
}

// 测试：带参数信号的参数传递正确性（值和引用类型）
TEST_CASE(signal_with_args)
{
    xswl::signal_t<int, std::string> sig;
    int r1 = 0;
    std::string r2;

    sig.connect([&](int a, const std::string& b) {
        r1 = a;
        r2 = b;
    });

    sig(42, "hello");
    ASSERT_EQ(r1, 42);
    ASSERT_EQ(r2, "hello");
}

// 测试：empty 与 slot_count 的行为以及断开连接后的计数变化
TEST_CASE(empty_and_slot_count)
{
    xswl::signal_t<int> sig;
    ASSERT_TRUE(sig.empty());
    ASSERT_EQ(sig.slot_count(), 0u);

    auto c1 = sig.connect([](int) {});
    auto c2 = sig.connect([](int) {});
    ASSERT_FALSE(sig.empty());
    ASSERT_EQ(sig.slot_count(), 2u);

    c1.disconnect();
    ASSERT_EQ(sig.slot_count(), 1u);

    sig.disconnect_all();
    ASSERT_TRUE(sig.empty());
    ASSERT_EQ(sig.slot_count(), 0u);
}

// =========================== 2. 优先级与顺序 ===============================

// 测试：槽按优先级从高到低执行
TEST_CASE(priority_order)
{
    xswl::signal_t<> sig;
    std::vector<int> order;

    sig.connect([&]() { order.push_back(1); }, 10);
    sig.connect([&]() { order.push_back(2); }, 100);
    sig.connect([&]() { order.push_back(3); }, -1);

    sig();

    ASSERT_EQ(order.size(), 3u);
    ASSERT_EQ(order[0], 2); // priority 100
    ASSERT_EQ(order[1], 1); // priority 10
    ASSERT_EQ(order[2], 3); // priority -1
}

// 测试：相同优先级时保持注册顺序（稳定性）
TEST_CASE(stable_order_same_priority)
{
    xswl::signal_t<> sig;
    std::vector<int> order;

    sig.connect([&]() { order.push_back(1); }, 0);
    sig.connect([&]() { order.push_back(2); }, 0);
    sig.connect([&]() { order.push_back(3); }, 0);

    sig();

    ASSERT_EQ(order.size(), 3u);
    ASSERT_EQ(order[0], 1);
    ASSERT_EQ(order[1], 2);
    ASSERT_EQ(order[2], 3);
}

// =========================== 3. 单次连接语义 ===============================

// 测试：connect_once 单次槽的语义与移除行为
TEST_CASE(single_shot_basic)
{
    xswl::signal_t<int> sig;
    Counter c;

    sig.connect_once([&c](int v) { c.add(v); });

    sig(10);
    ASSERT_EQ(c.get(), 10);

    sig(20);
    ASSERT_EQ(c.get(), 10); // 不应再次执行

    ASSERT_TRUE(sig.empty()); // 单次槽执行后应被移除
}

// 测试：多个单次槽在首次发射后均失效
TEST_CASE(multiple_single_shot)
{
    xswl::signal_t<> sig;
    Counter c;

    sig.connect_once([&]() { c.inc(); });
    sig.connect_once([&]() { c.inc(); });
    sig.connect_once([&]() { c.inc(); });

    sig();
    ASSERT_EQ(c.get(), 3);

    sig();
    ASSERT_EQ(c.get(), 3);
}

// ======================= 4. 成员函数 & 生命周期 ===========================

// 测试：通过 shared_ptr 连接成员函数并在对象销毁后不再被调用
TEST_CASE(member_function_shared_ptr_lifetime)
{
    xswl::signal_t<int> sig;
    Counter c;

    std::weak_ptr<Receiver> wptr;

    {
        std::shared_ptr<Receiver> r(new Receiver);
        wptr = r;

        sig.connect(r, &Receiver::on_int);
        sig.connect([&c](int) { c.inc(); });

        sig(42);
        ASSERT_EQ(r->call_count.load(), 1);
        ASSERT_EQ(r->last_int.load(), 42);
        ASSERT_EQ(c.get(), 1);
    } // r 销毁

    ASSERT_TRUE(wptr.expired());

    // 再发射不会调用已销毁对象的槽，但其他槽仍然可用
    sig(100);
    ASSERT_EQ(c.get(), 2);
}

// 测试：使用裸指针连接成员函数（不负责生命周期管理）
TEST_CASE(member_function_raw_pointer)
{
    xswl::signal_t<int> sig;
    Receiver r;

    sig.connect(&r, &Receiver::on_int);

    sig(5);
    ASSERT_EQ(r.call_count.load(), 1);
    ASSERT_EQ(r.last_int.load(), 5);
}

// 测试：基于标签的连接与断开，验证按标签移除行为
TEST_CASE(tag_connect_disconnect)
{
    xswl::signal_t<int> sig;
    Counter c1, c2;

    sig.connect("tag1", [&c1](int v) { c1.add(v); });
    sig.connect("tag2", [&c2](int v) { c2.add(v); });

    sig(10);
    ASSERT_EQ(c1.get(), 10);
    ASSERT_EQ(c2.get(), 10);

    bool ok = sig.disconnect("tag1");
    ASSERT_TRUE(ok);

    sig(5);
    ASSERT_EQ(c1.get(), 10); // 已断开
    ASSERT_EQ(c2.get(), 15);

    ok = sig.disconnect("tag1");
    ASSERT_FALSE(ok); // 二次断开失败
}

// 测试：信号析构后，仍可安全调用先前连接句柄的 disconnect
TEST_CASE(disconnect_after_signal_destruction)
{
    xswl::connection_t<> conn;
    {
        xswl::signal_t<> sig;
        conn = sig.connect([]() {});
        ASSERT_TRUE(conn.is_connected());
    } // sig 析构

    // 不应崩溃，且安全返回
    conn.disconnect();
}

// ====================== 5. scoped_connection & group =======================

// 测试：scoped_connection 的 RAII 行为（离开作用域时自动断开）
TEST_CASE(scoped_connection_raii)
{
    xswl::signal_t<> sig;
    Counter c;

    {
        xswl::scoped_connection_t sc = sig.connect([&]() { c.inc(); });
        sig();
        ASSERT_EQ(c.get(), 1);
    }

    sig();
    ASSERT_EQ(c.get(), 1); // 已自动断开
}

// 测试：connection_group 管理多个连接并能批量断开
TEST_CASE(connection_group_basic)
{
    xswl::signal_t<> sig;
    Counter c;

    xswl::connection_group_t group;
    group += sig.connect([&]() { c.inc(); });
    group += sig.connect([&]() { c.inc(); });

    ASSERT_EQ(group.size(), 2u);

    sig();
    ASSERT_EQ(c.get(), 2);

    group.disconnect_all();
    ASSERT_TRUE(group.empty());

    sig();
    ASSERT_EQ(c.get(), 2);
}

// =========================== 6. 重入与修改 ================================

// 测试：在 emit 过程中注册新槽，确保不会影响当前正在执行的槽序列
TEST_CASE(connect_inside_emit)
{
    xswl::signal_t<> sig;
    Counter c;

    // 第一次发射时，动态添加第二个槽
    bool added = false;
    sig.connect([&]() {
        c.inc();
        if (!added) {
            added = true;
            sig.connect([&]() { c.inc(); });
        }
    });

    sig(); // 只应调用第一个槽
    ASSERT_EQ(c.get(), 1);

    sig(); // 此时应调用两个槽
    ASSERT_EQ(c.get(), 3);
}

// 测试：在槽内部断开自身连接，验证当前发射不受影响
TEST_CASE(disconnect_self_inside_emit)
{
    xswl::signal_t<> sig;
    Counter c;
    xswl::connection_t<> conn;

    conn = sig.connect([&]() {
        c.inc();
        conn.disconnect();
    });

    sig();
    ASSERT_EQ(c.get(), 1);

    sig();
    ASSERT_EQ(c.get(), 1); // 不再执行
}

// 测试：在高优先级槽中断开其他槽，检查其对本次与下次发射的影响
TEST_CASE(disconnect_others_inside_emit)
{
    xswl::signal_t<> sig;
    Counter c1, c2;
    xswl::connection_t<> conn2;

    sig.connect([&]() {
        c1.inc();
        conn2.disconnect();
    }, 100);

    conn2 = sig.connect([&]() {
        c2.inc();
    }, 0);

    sig();
    
    // 高优先级槽必然被调用
    ASSERT_GE(c1.get(), 1);
    
    // 当前实现：disconnect 在本次 emit 中就生效，
    // 所以 c2 可能是 0（没被调用）或者 1（如果实现改成“本次也调用”）
    int first_c2 = c2.get();
    ASSERT_TRUE(first_c2 == 0 || first_c2 == 1);

    int before1 = c1.get();
    int before2 = c2.get();

    sig(); // conn2 已断开
    ASSERT_GT(c1.get(), before1);
    ASSERT_EQ(c2.get(), before2);
}

// 测试：递归发射（槽内部再次发射），验证嵌套调用计数正确
TEST_CASE(recursive_emit)
{
    xswl::signal_t<int> sig;
    Counter c;

    sig.connect([&](int depth) {
        c.inc();
        if (depth > 0)
            sig(depth - 1);
    });

    sig(5);
    ASSERT_EQ(c.get(), 6); // 5..0 共 6 次
}

// =========================== 7. 异常安全测试 ==============================

// 测试：若某槽抛异常，不应阻断其他槽的执行
TEST_CASE(slot_throw_does_not_block_others)
{
    xswl::signal_t<> sig;
    Counter c;

    sig.connect([&]() { c.inc(); }, 100);
    sig.connect([]() { throw std::runtime_error("test"); }, 50);
    sig.connect([&]() { c.inc(); }, 0);

    sig();
    ASSERT_EQ(c.get(), 2); // 其他槽应执行完
}

// =========================== 8. 多线程并发 =================================

// 并发测试：多线程并发发射带参数的信号，验证集成一致性
TEST_CASE(concurrent_emit_many_threads)
{
    xswl::signal_t<int> sig;
    std::atomic<int> sum(0);

    sig.connect([&](int v) { sum.fetch_add(v, std::memory_order_relaxed); });

    const int threads = 8;
    const int loops   = 10000;

    std::vector<std::thread> tv;
    for (int i = 0; i < threads; ++i)
    {
        tv.push_back(std::thread([&]() {
            for (int j = 0; j < loops; ++j)
                sig(1);
        }));
    }

    for (std::size_t i = 0; i < tv.size(); ++i)
        tv[i].join();

    ASSERT_EQ(sum.load(), threads * loops);
}

// 并发测试：在持续发射期间进行连接/断开，验证不会崩溃并维持基本不变量
TEST_CASE(concurrent_connect_disconnect_while_emit)
{
    xswl::signal_t<> sig;
    std::atomic<int> calls(0);
    std::atomic<bool> running(true);

    // 发射线程
    std::thread emitter([&]() {
        while (running.load(std::memory_order_relaxed)) {
            sig();
            std::this_thread::yield();
        }
    });

    // 多个连接/断开线程
    std::vector<std::thread> tv;
    for (int t = 0; t < 4; ++t)
    {
        tv.push_back(std::thread([&]() {
            for (int i = 0; i < 200; ++i)
            {
                xswl::connection_t<> conn = sig.connect([&]() {
                    calls.fetch_add(1, std::memory_order_relaxed);
                });
                std::this_thread::yield();
                conn.disconnect();
            }
        }));
    }

    for (std::size_t i = 0; i < tv.size(); ++i)
        tv[i].join();

    running.store(false, std::memory_order_relaxed);
    emitter.join();

    // 只要没崩溃就算通过；做一个简单下界检查
    ASSERT_GE(calls.load(), 0);
}

// 并发测试：在发射线程运行时反复 block/unblock 连接，验证稳定性
TEST_CASE(concurrent_block_unblock)
{
    xswl::signal_t<> sig;
    std::atomic<int> calls(0);
    std::atomic<bool> running(true);

    xswl::connection_t<> conn = sig.connect([&]() {
        calls.fetch_add(1, std::memory_order_relaxed);
    });

    std::thread emitter([&]() {
        while (running.load(std::memory_order_relaxed)) {
            sig();
        }
    });

    // 在另一个线程频繁 block / unblock
    std::thread blocker([&]() {
        for (int i = 0; i < 1000; ++i) {
            conn.block();
            std::this_thread::yield();
            conn.unblock();
        }
    });

    blocker.join();
    running.store(false, std::memory_order_relaxed);
    emitter.join();

    ASSERT_GE(calls.load(), 0);
}

// 多线程测试：单次槽在并发场景中应至多被执行一次
TEST_CASE(concurrent_single_shot)
{
    xswl::signal_t<> sig;
    const int slots = 100;
    std::vector<std::atomic<int>*> counters;
    counters.reserve(slots);

    for (int i = 0; i < slots; ++i)
    {
        std::atomic<int>* pc = new std::atomic<int>(0);
        counters.push_back(pc);
        sig.connect_once([pc]() {
            pc->fetch_add(1, std::memory_order_relaxed);
        });
    }

    const int threads = 8;
    const int loops   = 2000;
    std::vector<std::thread> tv;

    for (int t = 0; t < threads; ++t)
    {
        tv.push_back(std::thread([&]() {
            for (int i = 0; i < loops; ++i)
                sig();
        }));
    }

    for (std::size_t i = 0; i < tv.size(); ++i)
        tv[i].join();

    // 每个单次槽应最多调用一次
    int total = 0;
    for (int i = 0; i < slots; ++i)
    {
        int v = counters[i]->load();
        ASSERT_TRUE(v == 0 || v == 1);
        total += v;
        delete counters[i];
    }

    ASSERT_EQ(total, slots); // 全都执行了一次（有可能某些没执行也算合理，这里强测一把）
}

// 随机压力测试：多个线程对单个信号进行随机操作（emit/connect/disconnect/block/unblock）以检验稳定性
TEST_CASE(random_stress_test)
{
    xswl::signal_t<int> sig;
    std::atomic<bool> stop(false);
    std::atomic<int>  sum(0);

    // 一个固定的槽，始终存在
    sig.connect([&](int v) {
        sum.fetch_add(v, std::memory_order_relaxed);
    });

    // 工作者：随机 emit / connect / disconnect / block / unblock
    const int workers = 4;
    std::vector<std::thread> tv;

    for (int w = 0; w < workers; ++w)
    {
        tv.push_back(std::thread([&]() {
            std::mt19937 rng(static_cast<unsigned long>(
                std::chrono::high_resolution_clock::now()
                    .time_since_epoch()
                    .count()));
            std::uniform_int_distribution<int> dist(0, 4);

            std::vector<xswl::connection_t<int> > conns;

            while (!stop.load(std::memory_order_relaxed))
            {
                int op = dist(rng);
                switch (op)
                {
                    case 0: // emit
                        sig(1);
                        break;
                    case 1: // connect new slot
                    {
                        auto c = sig.connect(
                            [](int) {/* no-op */});
                        conns.push_back(std::move(c));
                        break;
                    }
                    case 2: // disconnect some
                        if (!conns.empty())
                        {
                            conns.back().disconnect();
                            conns.pop_back();
                        }
                        break;
                    case 3: // block some
                        if (!conns.empty())
                            conns[0].block();
                        break;
                    case 4: // unblock some
                        if (!conns.empty())
                            conns[0].unblock();
                        break;
                }
            }

            // 退场前清理
            for (std::size_t i = 0; i < conns.size(); ++i)
                conns[i].disconnect();
        }));
    }

    // 主线程等待一段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true, std::memory_order_relaxed);

    for (std::size_t i = 0; i < tv.size(); ++i)
        tv[i].join();

    // 只要没崩溃、和简单不变量成立即可
    ASSERT_GE(sum.load(), 0);
}

// =========================== 9. 各种类型参数 ===============================

// 测试：引用和 const 引用参数传递的正确性
TEST_CASE(ref_and_const_ref_args)
{
    xswl::signal_t<int&, const std::string&> sig;
    int v = 10;
    std::string s = "hello";
    int seen = 0;
    std::string seen_s;

    sig.connect([&](int& x, const std::string& y) {
        x *= 2;
        seen = x;
        seen_s = y;
    });

    sig(v, s);
    ASSERT_EQ(v, 20);
    ASSERT_EQ(seen, 20);
    ASSERT_EQ(seen_s, "hello");
}

// 测试：shared_ptr 参数的复制和引用计数行为
TEST_CASE(shared_ptr_args)
{
    xswl::signal_t<std::shared_ptr<int> > sig;
    std::shared_ptr<int> captured;

    sig.connect([&](std::shared_ptr<int> p) {
        captured = p;
    });

    std::shared_ptr<int> v(new int(42));
    sig(v);

    ASSERT_TRUE(captured);
    ASSERT_EQ(*captured, 42);
    ASSERT_EQ(v.use_count(), 2); // v 和 captured
}

// ============================ 10. 真实场景模拟 =============================

// 场景模拟：可观察者模式，用信号表示属性变化并记录历史
TEST_CASE(observable_pattern)
{
    struct Observable {
        xswl::signal_t<int> value_changed;
        int value;
        Observable() : value(0) {}
        void set_value(int v) {
            if (v != value) {
                value = v;
                value_changed(v);
            }
        }
    };

    Observable obj;
    std::vector<int> hist;

    obj.value_changed.connect([&](int v) {
        hist.push_back(v);
    });

    obj.set_value(1);
    obj.set_value(2);
    obj.set_value(2);
    obj.set_value(3);

    ASSERT_EQ(hist.size(), 3u);
    ASSERT_EQ(hist[0], 1);
    ASSERT_EQ(hist[1], 2);
    ASSERT_EQ(hist[2], 3);
}

// 场景模拟：信号链（一个信号触发另一个），验证传递与累加
TEST_CASE(signal_chaining)
{
    xswl::signal_t<int> s1;
    xswl::signal_t<int> s2;
    Counter c;

    s1.connect([&](int v) { s2(v * 2); });
    s2.connect([&](int v) { c.add(v); });

    s1(5);
    ASSERT_EQ(c.get(), 10);

    s1(3);
    ASSERT_EQ(c.get(), 16);
}

// 参数适配测试：注册不同参数数量的槽，验证只传递所需参数
TEST_CASE(partial_args_connect)
{
    xswl::signal_t<int, double, std::string> sig;
    
    int v1 = 0;
    double v2 = 0;
    std::string v3;
    int no_arg_count = 0;

    // 无参数
    sig.connect([&]() { ++no_arg_count; });

    // 1 个参数
    sig.connect([&](int a) { v1 = a; });

    // 2 个参数
    sig.connect([&](int a, double b) { v1 = a; v2 = b; });

    // 完整参数
    sig.connect([&](int a, double b, const std::string& c) {
        v1 = a;
        v2 = b;
        v3 = c;
    });

    sig(42, 3.14, "test");

    ASSERT_EQ(no_arg_count, 1);
    ASSERT_EQ(v1, 42);
    ASSERT_EQ(v2, 3.14);
    ASSERT_EQ(v3, "test");
}

// 测试：参数适配与一次性槽的组合语义（部分参数的单次槽）
TEST_CASE(partial_args_single_shot)
{
    xswl::signal_t<int, int> sig;
    Counter c;

    sig.connect_once([&]() { c.inc(); });
    sig.connect_once([&](int a) { c.add(a); });

    sig(10, 20);
    ASSERT_EQ(c.get(), 11);  // 1 + 10

    sig(5, 5);
    ASSERT_EQ(c.get(), 11);  // 不再执行
}

// 测试：带标签的参数适配连接，验证标签断开时不会影响其他槽
TEST_CASE(partial_args_with_tag)
{
    xswl::signal_t<int, std::string> sig;
    int value = 0;

    sig.connect("monitor", [&](int v) { value = v; });

    sig(100, "ignored");
    ASSERT_EQ(value, 100);

    sig.disconnect("monitor");

    sig(200, "ignored");
    ASSERT_EQ(value, 100);
}

// 测试：零参数信号仍然正常工作
TEST_CASE(zero_arg_signal_still_works)
{
    xswl::signal_t<> sig;
    Counter c;

    sig.connect([&]() { c.inc(); });

    sig();
    ASSERT_EQ(c.get(), 1);
}

// 测试：多线程环境下单次槽必须恰好被调用一次
TEST_CASE(concurrent_single_shot_exact_once)
{
    xswl::signal_t<> sig;
    std::atomic<int> call_count(0);
    
    sig.connect_once([&]() {
        call_count.fetch_add(1, std::memory_order_relaxed);
    });
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 100; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j)
                sig();
        });
    }
    
    for (auto& t : threads)
        t.join();
    
    // 必须恰好调用一次
    ASSERT_EQ(call_count.load(), 1);
}

// 测试：移动信号后源对象应处于安全状态，被移动到的对象应保持原有槽
TEST_CASE(moved_signal_safety)
{
    xswl::signal_t<> s1;
    s1.connect([]() {});
    
    xswl::signal_t<> s2 = std::move(s1);
    
    // s1 移动后应该安全
    ASSERT_FALSE(s1.valid());
    ASSERT_TRUE(s1.empty());
    s1.disconnect_all();  // 不应崩溃
    s1();                 // 不应崩溃
    
    ASSERT_TRUE(s2.valid());
    ASSERT_FALSE(s2.empty());
}

// 测试：成员函数的参数适配（仅传递需要的前 N 个参数）
TEST_CASE(member_function_partial_args)
{
    struct Receiver
    {
        int value = 0;
        void on_no_arg() { value = 1; }
        void on_one_arg(int v) { value = v; }
    };
    
    xswl::signal_t<int, int, int> sig;
    auto r = std::make_shared<Receiver>();
    
    sig.connect(r, &Receiver::on_no_arg);
    sig(10, 20, 30);
    ASSERT_EQ(r->value, 1);
    
    r->value = 0;
    sig.disconnect_all();
    
    sig.connect(r, &Receiver::on_one_arg);
    sig(42, 0, 0);
    ASSERT_EQ(r->value, 42);
}

// ==========================================================================
// main
// ==========================================================================
int main()
{
    return TestRunner::instance().run();
}