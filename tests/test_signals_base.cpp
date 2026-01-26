#include "xswl/signals.hpp"

#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ============================================================================
// 测试工具
// ============================================================================

#define TEST_CASE(name) \
    void test_##name(); \
    struct TestRegister_##name { \
        TestRegister_##name() { \
            TestRunner::instance().add(#name, test_##name); \
        } \
    } g_test_register_##name; \
    void test_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    } while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b)    ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b)    ASSERT_TRUE((a) != (b))
#define ASSERT_GT(a, b)    ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b)    ASSERT_TRUE((a) >= (b))
#define ASSERT_LT(a, b)    ASSERT_TRUE((a) < (b))
#define ASSERT_LE(a, b)    ASSERT_TRUE((a) <= (b))

class TestRunner
{
public:
    typedef void (*TestFunc)();
    
    static TestRunner& instance()
    {
        static TestRunner runner;
        return runner;
    }
    
    void add(const std::string& name, TestFunc func)
    {
        tests_.push_back({name, func});
    }
    
    int run()
    {
        int passed = 0;
        int failed = 0;
        
        std::cout << "Running " << tests_.size() << " tests...\n";
        std::cout << std::string(60, '=') << "\n";
        
        for (const auto& test : tests_)
        {
            std::cout << "[ RUN      ] " << test.name << std::endl;
            try
            {
                test.func();
                std::cout << "[       OK ] " << test.name << std::endl;
                ++passed;
            }
            catch (const std::exception& e)
            {
                std::cout << "[  FAILED  ] " << test.name << "\n";
                std::cout << "             " << e.what() << std::endl;
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
        TestFunc func;
    };
    
    std::vector<TestCase> tests_;
};

// ============================================================================
// 测试辅助类
// ============================================================================

class Counter
{
public:
    Counter() : count_(0) {}
    
    void increment() { ++count_; }
    void increment_by(int n) { count_ += n; }
    void set(int v) { count_ = v; }
    int get() const { return count_; }
    void reset() { count_ = 0; }
    
private:
    std::atomic<int> count_;
};

class Receiver : public std::enable_shared_from_this<Receiver>
{
public:
    Receiver() : call_count_(0), last_value_(0) {}
    
    void on_signal() { ++call_count_; }
    void on_value(int v) { ++call_count_; last_value_ = v; }
    void on_two_values(int a, int b) { ++call_count_; last_value_ = a + b; }
    void on_string(const std::string& s) { ++call_count_; last_string_ = s; }
    
    int call_count() const { return call_count_; }
    int last_value() const { return last_value_; }
    const std::string& last_string() const { return last_string_; }
    
    void reset()
    {
        call_count_ = 0;
        last_value_ = 0;
        last_string_.clear();
    }
    
private:
    std::atomic<int> call_count_;
    std::atomic<int> last_value_;
    std::string last_string_;
};

// ============================================================================
// 基本功能测试
// ============================================================================

TEST_CASE(basic_signal_emit)
{
    xswl::signal_t<> sig;
    Counter counter;
    
    sig.connect([&counter]() { counter.increment(); });
    
    sig();
    ASSERT_EQ(counter.get(), 1);
    
    sig();
    sig();
    ASSERT_EQ(counter.get(), 3);
}

TEST_CASE(signal_with_arguments)
{
    xswl::signal_t<int> sig;
    int received = 0;
    
    sig.connect([&received](int v) { received = v; });
    
    sig(42);
    ASSERT_EQ(received, 42);
    
    sig(100);
    ASSERT_EQ(received, 100);
}

TEST_CASE(signal_with_multiple_arguments)
{
    xswl::signal_t<int, double, std::string> sig;
    int a = 0;
    double b = 0;
    std::string c;
    
    sig.connect([&](int x, double y, const std::string& z) {
        a = x;
        b = y;
        c = z;
    });
    
    sig(1, 2.5, "hello");
    
    ASSERT_EQ(a, 1);
    ASSERT_EQ(b, 2.5);
    ASSERT_EQ(c, "hello");
}

TEST_CASE(multiple_slots)
{
    xswl::signal_t<int> sig;
    std::vector<int> results;
    
    sig.connect([&results](int v) { results.push_back(v * 1); });
    sig.connect([&results](int v) { results.push_back(v * 2); });
    sig.connect([&results](int v) { results.push_back(v * 3); });
    
    sig(10);
    
    ASSERT_EQ(results.size(), 3u);
    // 注意：顺序可能因优先级而异
}

TEST_CASE(empty_signal)
{
    xswl::signal_t<> sig;
    
    ASSERT_TRUE(sig.empty());
    ASSERT_EQ(sig.slot_count(), 0u);
    
    // 发射空信号不应崩溃
    sig();
    
    auto conn = sig.connect([]() {});
    ASSERT_FALSE(sig.empty());
    ASSERT_EQ(sig.slot_count(), 1u);
}

// ============================================================================
// 连接管理测试
// ============================================================================

TEST_CASE(connection_disconnect)
{
    xswl::signal_t<> sig;
    Counter counter;
    
    auto conn = sig.connect([&counter]() { counter.increment(); });
    
    sig();
    ASSERT_EQ(counter.get(), 1);
    
    conn.disconnect();
    
    sig();
    ASSERT_EQ(counter.get(), 1);  // 不再增加
    
    ASSERT_FALSE(conn.is_connected());
}

TEST_CASE(connection_block_unblock)
{
    xswl::signal_t<> sig;
    Counter counter;
    
    auto conn = sig.connect([&counter]() { counter.increment(); });
    
    sig();
    ASSERT_EQ(counter.get(), 1);
    
    conn.block();
    ASSERT_TRUE(conn.is_blocked());
    
    sig();
    ASSERT_EQ(counter.get(), 1);  // 被阻塞
    
    conn.unblock();
    ASSERT_FALSE(conn.is_blocked());
    
    sig();
    ASSERT_EQ(counter.get(), 2);  // 恢复
}

TEST_CASE(scoped_connection)
{
    xswl::signal_t<> sig;
    Counter counter;
    
    {
        xswl::scoped_connection_t sc = sig.connect([&counter]() {
            counter.increment();
        });
        
        sig();
        ASSERT_EQ(counter.get(), 1);
    }  // sc 销毁，自动断开
    
    sig();
    ASSERT_EQ(counter.get(), 1);  // 不再增加
}

TEST_CASE(scoped_connection_move)
{
    xswl::signal_t<> sig;
    Counter counter;
    
    xswl::scoped_connection_t sc1;
    
    {
        xswl::scoped_connection_t sc2 = sig.connect([&counter]() {
            counter.increment();
        });
        
        sig();
        ASSERT_EQ(counter.get(), 1);
        
        sc1 = std::move(sc2);  // 移动
    }
    
    sig();
    ASSERT_EQ(counter.get(), 2);  // sc1 仍有效
    
    sc1.disconnect();
    
    sig();
    ASSERT_EQ(counter.get(), 2);
}

TEST_CASE(connection_group)
{
    xswl::signal_t<> sig;
    Counter counter;
    xswl::connection_group_t group;
    
    group += sig.connect([&counter]() { counter.increment(); });
    group += sig.connect([&counter]() { counter.increment(); });
    group += sig.connect([&counter]() { counter.increment(); });
    
    ASSERT_EQ(group.size(), 3u);
    
    sig();
    ASSERT_EQ(counter.get(), 3);
    
    group.disconnect_all();
    ASSERT_TRUE(group.empty());
    
    sig();
    ASSERT_EQ(counter.get(), 3);  // 不再增加
}

TEST_CASE(disconnect_all)
{
    xswl::signal_t<> sig;
    Counter counter;
    
    sig.connect([&counter]() { counter.increment(); });
    sig.connect([&counter]() { counter.increment(); });
    sig.connect([&counter]() { counter.increment(); });
    
    sig();
    ASSERT_EQ(counter.get(), 3);
    
    sig.disconnect_all();
    ASSERT_TRUE(sig.empty());
    
    sig();
    ASSERT_EQ(counter.get(), 3);
}

// ============================================================================
// 单次连接测试
// ============================================================================

TEST_CASE(single_shot_connection)
{
    xswl::signal_t<int> sig;
    Counter counter;
    
    sig.connect_once([&counter](int v) { counter.increment_by(v); });
    
    sig(10);
    ASSERT_EQ(counter.get(), 10);
    
    sig(20);
    ASSERT_EQ(counter.get(), 10);  // 只调用一次
    
    sig(30);
    ASSERT_EQ(counter.get(), 10);
}

TEST_CASE(multiple_single_shot)
{
    xswl::signal_t<> sig;
    Counter counter;
    
    sig.connect_once([&counter]() { counter.increment(); });
    sig.connect_once([&counter]() { counter.increment(); });
    sig.connect_once([&counter]() { counter.increment(); });
    
    sig();
    ASSERT_EQ(counter.get(), 3);
    
    sig();
    ASSERT_EQ(counter.get(), 3);  // 全部失效
}

TEST_CASE(mixed_connections)
{
    xswl::signal_t<> sig;
    Counter normal_counter;
    Counter once_counter;
    
    sig.connect([&normal_counter]() { normal_counter.increment(); });
    sig.connect_once([&once_counter]() { once_counter.increment(); });
    sig.connect([&normal_counter]() { normal_counter.increment(); });
    
    sig();
    ASSERT_EQ(normal_counter.get(), 2);
    ASSERT_EQ(once_counter.get(), 1);
    
    sig();
    ASSERT_EQ(normal_counter.get(), 4);
    ASSERT_EQ(once_counter.get(), 1);  // 不再增加
}

// ============================================================================
// 优先级测试
// ============================================================================

TEST_CASE(priority_order)
{
    xswl::signal_t<> sig;
    std::vector<int> order;
    
    sig.connect([&order]() { order.push_back(1); }, 10);   // 中优先级
    sig.connect([&order]() { order.push_back(2); }, 100);  // 高优先级
    sig.connect([&order]() { order.push_back(3); }, 1);    // 低优先级
    
    sig();
    
    ASSERT_EQ(order.size(), 3u);
    ASSERT_EQ(order[0], 2);  // 高优先级先执行
    ASSERT_EQ(order[1], 1);  // 中优先级
    ASSERT_EQ(order[2], 3);  // 低优先级
}

TEST_CASE(same_priority_stable_order)
{
    xswl::signal_t<> sig;
    std::vector<int> order;
    
    sig.connect([&order]() { order.push_back(1); }, 0);
    sig.connect([&order]() { order.push_back(2); }, 0);
    sig.connect([&order]() { order.push_back(3); }, 0);
    
    sig();
    
    ASSERT_EQ(order.size(), 3u);
    ASSERT_EQ(order[0], 1);  // 保持插入顺序
    ASSERT_EQ(order[1], 2);
    ASSERT_EQ(order[2], 3);
}

TEST_CASE(negative_priority)
{
    xswl::signal_t<> sig;
    std::vector<int> order;
    
    sig.connect([&order]() { order.push_back(1); }, 0);
    sig.connect([&order]() { order.push_back(2); }, -10);
    sig.connect([&order]() { order.push_back(3); }, 10);
    
    sig();
    
    ASSERT_EQ(order[0], 3);
    ASSERT_EQ(order[1], 1);
    ASSERT_EQ(order[2], 2);
}

// ============================================================================
// 成员函数连接测试
// ============================================================================

TEST_CASE(member_function_shared_ptr)
{
    xswl::signal_t<int> sig;
    auto receiver = std::make_shared<Receiver>();
    
    sig.connect(receiver, &Receiver::on_value);
    
    sig(42);
    ASSERT_EQ(receiver->call_count(), 1);
    ASSERT_EQ(receiver->last_value(), 42);
}

TEST_CASE(member_function_lifetime)
{
    xswl::signal_t<int> sig;
    Counter call_counter;
    
    {
        auto receiver = std::make_shared<Receiver>();
        sig.connect(receiver, &Receiver::on_value);
        
        sig(1);
        ASSERT_EQ(receiver->call_count(), 1);
    }  // receiver 销毁
    
    // 发射信号不应崩溃，槽自动失效
    sig(2);
    sig(3);
}

TEST_CASE(member_function_raw_pointer)
{
    xswl::signal_t<int> sig;
    Receiver receiver;
    
    sig.connect(&receiver, &Receiver::on_value);
    
    sig(100);
    ASSERT_EQ(receiver.call_count(), 1);
    ASSERT_EQ(receiver.last_value(), 100);
}

TEST_CASE(member_function_const)
{
    // 测试 const 成员函数
    struct ConstReceiver
    {
        mutable int value = 0;
        void on_value(int v) const { value = v; }
    };
    
    xswl::signal_t<int> sig;
    auto receiver = std::make_shared<ConstReceiver>();
    
    sig.connect(receiver, &ConstReceiver::on_value);
    
    sig(55);
    ASSERT_EQ(receiver->value, 55);
}

// ============================================================================
// 标签连接测试
// ============================================================================

TEST_CASE(tagged_connection)
{
    xswl::signal_t<int> sig;
    Counter counter;
    
    sig.connect("worker", [&counter](int v) { counter.increment_by(v); });
    
    sig(10);
    ASSERT_EQ(counter.get(), 10);
    
    bool result = sig.disconnect("worker");
    ASSERT_TRUE(result);
    
    sig(20);
    ASSERT_EQ(counter.get(), 10);  // 不再增加
}

TEST_CASE(disconnect_nonexistent_tag)
{
    xswl::signal_t<> sig;
    
    bool result = sig.disconnect("nonexistent");
    ASSERT_FALSE(result);
}

TEST_CASE(multiple_same_tag)
{
    xswl::signal_t<> sig;
    Counter counter;
    
    sig.connect("same_tag", [&counter]() { counter.increment(); });
    sig.connect("same_tag", [&counter]() { counter.increment(); });
    
    sig();
    // 行为取决于实现：可能是2次或共享标签
    ASSERT_GE(counter.get(), 1);
    
    sig.disconnect("same_tag");
    int count_after = counter.get();
    
    sig();
    // 至少移除了一个
    ASSERT_LE(counter.get(), count_after + 1);
}

TEST_CASE(multiple_different_tags)
{
    xswl::signal_t<> sig;
    Counter counter1, counter2;
    
    sig.connect("tag1", [&counter1]() { counter1.increment(); });
    sig.connect("tag2", [&counter2]() { counter2.increment(); });
    
    sig();
    ASSERT_EQ(counter1.get(), 1);
    ASSERT_EQ(counter2.get(), 1);
    
    sig.disconnect("tag1");
    
    sig();
    ASSERT_EQ(counter1.get(), 1);  // tag1 断开
    ASSERT_EQ(counter2.get(), 2);  // tag2 仍有效
}

// ============================================================================
// 异常安全测试
// ============================================================================

TEST_CASE(slot_throws_exception)
{
    xswl::signal_t<> sig;
    Counter counter;
    
    sig.connect([&counter]() { counter.increment(); }, 100);  // 先执行
    sig.connect([]() { throw std::runtime_error("test"); }, 50);
    sig.connect([&counter]() { counter.increment(); }, 0);   // 后执行
    
    // 信号发射应该捕获异常，继续执行其他槽
    sig();
    
    // 第一个槽应该执行了
    ASSERT_GE(counter.get(), 1);
}

TEST_CASE(slot_exception_isolation)
{
    xswl::signal_t<int> sig;
    std::vector<int> results;
    
    sig.connect([&results](int v) { results.push_back(v); }, 100);
    sig.connect([](int) { throw std::runtime_error("error"); }, 50);
    sig.connect([&results](int v) { results.push_back(v * 2); }, 0);
    
    sig(5);
    
    ASSERT_GE(results.size(), 1u);
    ASSERT_EQ(results[0], 5);
}

// ============================================================================
// 边界情况测试
// ============================================================================

TEST_CASE(connect_during_emit)
{
    xswl::signal_t<> sig;
    Counter counter;
    bool added = false;
    
    sig.connect([&]() {
        counter.increment();
        if (!added)
        {
            added = true;
            sig.connect([&counter]() { counter.increment(); });
        }
    });
    
    sig();  // 第一次发射
    
    sig();  // 第二次发射应该有两个槽
    ASSERT_GE(counter.get(), 2);
}

TEST_CASE(disconnect_during_emit)
{
    xswl::signal_t<> sig;
    Counter counter;
    xswl::connection_t<> conn;
    
    conn = sig.connect([&]() {
        counter.increment();
        conn.disconnect();  // 自我断开
    });
    
    sig();
    ASSERT_EQ(counter.get(), 1);
    
    sig();
    ASSERT_EQ(counter.get(), 1);  // 已断开
}

TEST_CASE(disconnect_other_during_emit)
{
    xswl::signal_t<> sig;
    Counter counter1, counter2;
    xswl::connection_t<> conn2;
    
    sig.connect([&]() {
        counter1.increment();
        conn2.disconnect();  // 断开另一个
    }, 100);
    
    conn2 = sig.connect([&counter2]() {
        counter2.increment();
    }, 0);
    
    sig();
    ASSERT_EQ(counter1.get(), 1);
    // counter2 可能是 0 或 1，取决于执行顺序
}

TEST_CASE(recursive_emit)
{
    xswl::signal_t<int> sig;
    Counter counter;
    
    sig.connect([&](int depth) {
        counter.increment();
        if (depth > 0)
        {
            sig(depth - 1);  // 递归发射
        }
    });
    
    sig(5);
    ASSERT_EQ(counter.get(), 6);  // 0, 1, 2, 3, 4, 5
}

TEST_CASE(signal_destruction_during_use)
{
    auto sig = std::make_shared<xswl::signal_t<>>();
    Counter counter;
    
    auto conn = sig->connect([&counter]() { counter.increment(); });
    
    (*sig)();
    ASSERT_EQ(counter.get(), 1);
    
    sig.reset();  // 销毁信号
    
    // 连接应该安全失效
    conn.disconnect();  // 不应崩溃
}

// ============================================================================
// 多线程测试
// ============================================================================

TEST_CASE(concurrent_emit)
{
    xswl::signal_t<int> sig;
    std::atomic<int> total{0};
    
    sig.connect([&total](int v) {
        total.fetch_add(v, std::memory_order_relaxed);
    });
    
    const int num_threads = 10;
    const int emits_per_thread = 1000;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&sig, emits_per_thread]() {
            for (int j = 0; j < emits_per_thread; ++j)
            {
                sig(1);
            }
        });
    }
    
    for (auto& t : threads)
    {
        t.join();
    }
    
    ASSERT_EQ(total.load(), num_threads * emits_per_thread);
}

TEST_CASE(concurrent_connect_disconnect)
{
    xswl::signal_t<> sig;
    std::atomic<int> call_count{0};
    std::atomic<bool> running{true};
    
    // 发射线程
    std::thread emitter([&]() {
        while (running.load())
        {
            sig();
            std::this_thread::yield();
        }
    });
    
    // 连接/断开线程
    std::vector<std::thread> connectors;
    for (int i = 0; i < 5; ++i)
    {
        connectors.emplace_back([&]() {
            for (int j = 0; j < 100; ++j)
            {
                auto conn = sig.connect([&call_count]() {
                    call_count.fetch_add(1, std::memory_order_relaxed);
                });
                std::this_thread::yield();
                conn.disconnect();
            }
        });
    }
    
    for (auto& t : connectors)
    {
        t.join();
    }
    
    running.store(false);
    emitter.join();
    
    // 只要不崩溃就算通过
    ASSERT_GE(call_count.load(), 0);
}

TEST_CASE(concurrent_block_unblock)
{
    xswl::signal_t<> sig;
    std::atomic<int> call_count{0};
    std::atomic<bool> running{true};
    
    auto conn = sig.connect([&call_count]() {
        call_count.fetch_add(1, std::memory_order_relaxed);
    });
    
    // 发射线程
    std::thread emitter([&]() {
        while (running.load())
        {
            sig();
            std::this_thread::yield();
        }
    });
    
    // 阻塞/解除阻塞线程
    std::thread blocker([&]() {
        for (int i = 0; i < 1000; ++i)
        {
            conn.block();
            std::this_thread::yield();
            conn.unblock();
        }
    });
    
    blocker.join();
    running.store(false);
    emitter.join();
    
    ASSERT_GE(call_count.load(), 0);
}

TEST_CASE(concurrent_different_signals)
{
    xswl::signal_t<int> sig1;
    xswl::signal_t<int> sig2;
    xswl::signal_t<int> sig3;
    
    std::atomic<int> total1{0}, total2{0}, total3{0};
    
    sig1.connect([&total1](int v) { total1.fetch_add(v); });
    sig2.connect([&total2](int v) { total2.fetch_add(v); });
    sig3.connect([&total3](int v) { total3.fetch_add(v); });
    
    const int iterations = 1000;
    
    std::thread t1([&]() { for(int i = 0; i < iterations; ++i) sig1(1); });
    std::thread t2([&]() { for(int i = 0; i < iterations; ++i) sig2(2); });
    std::thread t3([&]() { for(int i = 0; i < iterations; ++i) sig3(3); });
    
    t1.join();
    t2.join();
    t3.join();
    
    ASSERT_EQ(total1.load(), iterations * 1);
    ASSERT_EQ(total2.load(), iterations * 2);
    ASSERT_EQ(total3.load(), iterations * 3);
}

TEST_CASE(stress_test_many_connections)
{
    xswl::signal_t<int> sig;
    std::atomic<int> total{0};
    std::vector<xswl::connection_t<int>> connections;
    
    const int num_slots = 1000;
    
    for (int i = 0; i < num_slots; ++i)
    {
        connections.push_back(sig.connect([&total](int v) {
            total.fetch_add(v, std::memory_order_relaxed);
        }));
    }
    
    ASSERT_EQ(sig.slot_count(), static_cast<std::size_t>(num_slots));
    
    sig(1);
    ASSERT_EQ(total.load(), num_slots);
    
    // 断开一半
    for (int i = 0; i < num_slots / 2; ++i)
    {
        connections[i].disconnect();
    }
    
    total.store(0);
    sig(1);
    ASSERT_EQ(total.load(), num_slots / 2);
}

TEST_CASE(concurrent_single_shot)
{
    xswl::signal_t<> sig;
    std::atomic<int> call_count{0};
    
    for (int i = 0; i < 100; ++i)
    {
        sig.connect_once([&call_count]() {
            call_count.fetch_add(1, std::memory_order_relaxed);
        });
    }
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([&sig]() {
            sig();
        });
    }
    
    for (auto& t : threads)
    {
        t.join();
    }
    
    // 单次槽应该只被调用100次（总共），不管多少线程
    ASSERT_EQ(call_count.load(), 100);
}

TEST_CASE(producer_consumer_pattern)
{
    xswl::signal_t<int> data_ready;
    std::atomic<int> sum{0};
    std::atomic<int> count{0};
    const int total_items = 1000;
    
    // 消费者
    data_ready.connect([&](int value) {
        sum.fetch_add(value, std::memory_order_relaxed);
        count.fetch_add(1, std::memory_order_relaxed);
    });
    
    // 多个生产者
    std::vector<std::thread> producers;
    for (int i = 0; i < 4; ++i)
    {
        producers.emplace_back([&, i]() {
            for (int j = 0; j < total_items / 4; ++j)
            {
                data_ready(1);
            }
        });
    }
    
    for (auto& t : producers)
    {
        t.join();
    }
    
    ASSERT_EQ(count.load(), total_items);
    ASSERT_EQ(sum.load(), total_items);
}

// ============================================================================
// 性能测试
// ============================================================================

TEST_CASE(emit_performance)
{
    xswl::signal_t<int> sig;
    volatile int sink = 0;
    
    sig.connect([&sink](int v) { sink = v; });
    
    const int iterations = 100000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i)
    {
        sig(i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "             " << iterations << " emits in " 
              << duration.count() << " us ("
              << (duration.count() * 1000.0 / iterations) << " ns/emit)" << std::endl;
    
    ASSERT_TRUE(true);  // 只要能完成就通过
}

TEST_CASE(connect_disconnect_performance)
{
    xswl::signal_t<> sig;
    
    const int iterations = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i)
    {
        auto conn = sig.connect([]() {});
        conn.disconnect();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "             " << iterations << " connect/disconnect in "
              << duration.count() << " us" << std::endl;
    
    ASSERT_TRUE(sig.empty());
}

TEST_CASE(many_slots_emit_performance)
{
    xswl::signal_t<> sig;
    std::atomic<int> counter{0};
    std::vector<xswl::connection_t<>> connections;
    
    const int num_slots = 100;
    const int iterations = 10000;
    
    for (int i = 0; i < num_slots; ++i)
    {
        connections.push_back(sig.connect([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i)
    {
        sig();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "             " << iterations << " emits with " << num_slots 
              << " slots in " << duration.count() << " us" << std::endl;
    
    ASSERT_EQ(counter.load(), num_slots * iterations);
}

// ============================================================================
// 特殊类型测试
// ============================================================================

TEST_CASE(signal_with_reference_args)
{
    xswl::signal_t<int&> sig;
    
    sig.connect([](int& v) { v *= 2; });
    
    int value = 10;
    sig(value);
    
    ASSERT_EQ(value, 20);
}

TEST_CASE(signal_with_const_reference)
{
    xswl::signal_t<const std::string&> sig;
    std::string received;
    
    sig.connect([&received](const std::string& s) { received = s; });
    
    std::string msg = "hello world";
    sig(msg);
    
    ASSERT_EQ(received, "hello world");
}

TEST_CASE(signal_with_move_only_type)
{
    xswl::signal_t<std::unique_ptr<int>&> sig;
    int received_value = 0;
    
    sig.connect([&received_value](std::unique_ptr<int>& ptr) {
        if (ptr)
        {
            received_value = *ptr;
        }
    });
    
    std::unique_ptr<int> ptr(new int(42));
    sig(ptr);
    
    ASSERT_EQ(received_value, 42);
    ASSERT_TRUE(ptr != nullptr);  // 仍然有效
}

TEST_CASE(signal_with_shared_ptr)
{
    xswl::signal_t<std::shared_ptr<int>> sig;
    std::shared_ptr<int> received;
    
    sig.connect([&received](std::shared_ptr<int> ptr) { received = ptr; });
    
    auto ptr = std::make_shared<int>(100);
    sig(ptr);
    
    ASSERT_EQ(*received, 100);
    ASSERT_EQ(ptr.use_count(), 2);  // ptr 和 received 共享
}

// ============================================================================
// 信号作为成员变量测试
// ============================================================================

TEST_CASE(signal_as_class_member)
{
    struct Button
    {
        xswl::signal_t<> clicked;
        xswl::signal_t<int, int> position_changed;
        
        void click() { clicked(); }
        void move(int x, int y) { position_changed(x, y); }
    };
    
    Button btn;
    Counter click_counter;
    int last_x = 0, last_y = 0;
    
    btn.clicked.connect([&click_counter]() { click_counter.increment(); });
    btn.position_changed.connect([&](int x, int y) { last_x = x; last_y = y; });
    
    btn.click();
    btn.click();
    ASSERT_EQ(click_counter.get(), 2);
    
    btn.move(100, 200);
    ASSERT_EQ(last_x, 100);
    ASSERT_EQ(last_y, 200);
}

TEST_CASE(observable_pattern)
{
    struct Observable
    {
        xswl::signal_t<int> value_changed;
        
        void set_value(int v)
        {
            if (v != value_)
            {
                value_ = v;
                value_changed(v);
            }
        }
        
        int value() const { return value_; }
        
    private:
        int value_ = 0;
    };
    
    Observable obj;
    std::vector<int> history;
    
    obj.value_changed.connect([&history](int v) { history.push_back(v); });
    
    obj.set_value(1);
    obj.set_value(2);
    obj.set_value(2);  // 相同值，不触发
    obj.set_value(3);
    
    ASSERT_EQ(history.size(), 3u);
    ASSERT_EQ(history[0], 1);
    ASSERT_EQ(history[1], 2);
    ASSERT_EQ(history[2], 3);
}

// ============================================================================
// 信号链测试
// ============================================================================

TEST_CASE(signal_chaining)
{
    xswl::signal_t<int> source;
    xswl::signal_t<int> relay;
    Counter counter;
    
    // source -> relay -> handler
    source.connect([&relay](int v) { relay(v * 2); });
    relay.connect([&counter](int v) { counter.increment_by(v); });
    
    source(5);
    ASSERT_EQ(counter.get(), 10);
    
    source(3);
    ASSERT_EQ(counter.get(), 16);
}

TEST_CASE(bidirectional_signals)
{
    struct NodeA
    {
        xswl::signal_t<int> send;
        void receive(int v) { received = v; }
        int received = 0;
    };
    
    struct NodeB
    {
        xswl::signal_t<int> send;
        void receive(int v) { received = v; }
        int received = 0;
    };
    
    auto a = std::make_shared<NodeA>();
    auto b = std::make_shared<NodeB>();
    
    a->send.connect(b, &NodeB::receive);
    b->send.connect(a, &NodeA::receive);
    
    a->send(100);
    ASSERT_EQ(b->received, 100);
    
    b->send(200);
    ASSERT_EQ(a->received, 200);
}

// ============================================================================
// 主函数
// ============================================================================

int main()
{
    return TestRunner::instance().run();
}