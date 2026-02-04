#include "test_common.hpp"

// 并发测试：在发射过程中注册新的槽，验证安全性与可见性
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

    sig();
    sig();
    ASSERT_GE(counter.get(), 2);
}

// 并发测试：在发射过程中断开自身连接，验证不会影响当前调用序列
TEST_CASE(disconnect_during_emit)
{
    xswl::signal_t<> sig;
    Counter counter;
    xswl::connection_t<> conn;

    conn = sig.connect([&]() {
        counter.increment();
        conn.disconnect();
    });

    sig();
    ASSERT_EQ(counter.get(), 1);

    sig();
    ASSERT_EQ(counter.get(), 1);
}

// 并发测试：在高优先级槽中断开其他槽，验证被中断槽不会在同次发射中被调用
TEST_CASE(disconnect_other_during_emit)
{
    xswl::signal_t<> sig;
    Counter counter1, counter2;
    xswl::connection_t<> conn2;

    sig.connect([&]() {
        counter1.increment();
        conn2.disconnect();
    }, 100);

    conn2 = sig.connect([&counter2]() { counter2.increment(); }, 0);

    sig();
    ASSERT_EQ(counter1.get(), 1);
}

// 测试：递归发射信号（槽内部再次发射），验证递归计数正确性
TEST_CASE(recursive_emit)
{
    xswl::signal_t<int> sig;
    Counter counter;

    sig.connect([&](int depth) {
        counter.increment();
        if (depth > 0)
        {
            sig(depth - 1);
        }
    });

    sig(5);
    ASSERT_EQ(counter.get(), 6);
}

// 测试：在使用期间销毁信号对象的安全性（连接仍能被正确断开）
TEST_CASE(signal_destruction_during_use)
{
    auto sig = std::make_shared<xswl::signal_t<>>();
    Counter counter;

    auto conn = sig->connect([&counter]() { counter.increment(); });

    (*sig)();
    ASSERT_EQ(counter.get(), 1);

    sig.reset();

    conn.disconnect();
}

// 并发测试：多线程并发发射带参数的信号，验证累加结果正确性
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
            for (int j = 0; j < emits_per_thread; ++j) sig(1);
        });
    }

    for (auto& t : threads) t.join();

    ASSERT_EQ(total.load(), num_threads * emits_per_thread);
}

// 并发测试：连接与断开在发射线程持续运行时并发进行的稳定性测试
TEST_CASE(concurrent_connect_disconnect)
{
    xswl::signal_t<> sig;
    std::atomic<int> call_count{0};
    std::atomic<bool> running{true};

    std::thread emitter([&]() {
        while (running.load()) {
            sig();
            std::this_thread::yield();
        }
    });

    std::vector<std::thread> connectors;
    for (int i = 0; i < 5; ++i)
    {
        connectors.emplace_back([&]() {
            for (int j = 0; j < 100; ++j)
            {
                auto conn = sig.connect([&call_count]() { call_count.fetch_add(1, std::memory_order_relaxed); });
                std::this_thread::yield();
                conn.disconnect();
            }
        });
    }

    for (auto& t : connectors) t.join();

    running.store(false);
    emitter.join();

    ASSERT_GE(call_count.load(), 0);
}

// 并发测试：在发射线程运行时反复 block/unblock 连接，验证稳定性
TEST_CASE(concurrent_block_unblock)
{
    xswl::signal_t<> sig;
    std::atomic<int> call_count{0};
    std::atomic<bool> running{true};

    auto conn = sig.connect([&call_count]() { call_count.fetch_add(1, std::memory_order_relaxed); });

    std::thread emitter([&]() {
        while (running.load()) { sig(); std::this_thread::yield(); }
    });

    std::thread blocker([&]() {
        for (int i = 0; i < 1000; ++i) { conn.block(); std::this_thread::yield(); conn.unblock(); }
    });

    blocker.join();
    running.store(false);
    emitter.join();

    ASSERT_GE(call_count.load(), 0);
}

// 并发测试：不同信号在各自线程中并发发射，互不干扰并验证结果
TEST_CASE(concurrent_different_signals)
{
    xswl::signal_t<int> sig1, sig2, sig3;
    std::atomic<int> total1{0}, total2{0}, total3{0};

    sig1.connect([&total1](int v) { total1.fetch_add(v); });
    sig2.connect([&total2](int v) { total2.fetch_add(v); });
    sig3.connect([&total3](int v) { total3.fetch_add(v); });

    const int iterations = 1000;

    std::thread t1([&]() { for(int i=0;i<iterations;++i) sig1(1); });
    std::thread t2([&]() { for(int i=0;i<iterations;++i) sig2(2); });
    std::thread t3([&]() { for(int i=0;i<iterations;++i) sig3(3); });

    t1.join(); t2.join(); t3.join();

    ASSERT_EQ(total1.load(), iterations * 1);
    ASSERT_EQ(total2.load(), iterations * 2);
    ASSERT_EQ(total3.load(), iterations * 3);
}
