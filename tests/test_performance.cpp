#include "test_common.hpp"
#include <thread>
#include <algorithm>

// 将微秒转换为每次操作的纳秒 (ns/op)
// 参数：microseconds - 总耗时（微秒），ops - 操作次数
// 返回：每次操作的纳秒数（double），ops 为 0 时返回 0.0
static double us_to_ns_per_op(long long microseconds, int ops)
{
    if (ops == 0) return 0.0;
    return (microseconds * 1000.0) / ops;
}

// 基准测试：空信号发射（没有任何槽）
// 用于测量调用空信号的基线开销
TEST_CASE(emit_empty_signal)
{
    xswl::signal_t<> sig;

    const int iterations = 200000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
        sig();
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "             empty: " << iterations << " emits in " << dur
              << " us (" << us_to_ns_per_op(dur, iterations) << " ns/emit)" << std::endl;

    ASSERT_TRUE(true);
}

// 基准测试：单槽不同变体
// 比较 lambda 捕获、无捕获以及成员函数（带 shared_ptr 跟踪）的调用开销差异
TEST_CASE(single_slot_emit_variants)
{
    volatile int sink = 0;

    // Lambda capturing sink by reference
    {
        xswl::signal_t<int> sig;
        sig.connect([&sink](int v) { sink = v; });
        const int iterations = 200000;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i)
            sig(i);
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "             single lambda ref: " << iterations << " emits in " << dur
                  << " us (" << us_to_ns_per_op(dur, iterations) << " ns/emit)" << std::endl;
    }

    // Static function wrapper
    {
        xswl::signal_t<int> sig;
        sig.connect([](int v) { (void)v; });
        const int iterations = 200000;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i)
            sig(i);
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "             single lambda no-capture: " << iterations << " emits in " << dur
                  << " us (" << us_to_ns_per_op(dur, iterations) << " ns/emit)" << std::endl;
    }

    // Member function via shared_ptr (tracked path)
    {
        struct Obj { void f(int) {} };
        auto p = std::make_shared<Obj>();
        xswl::signal_t<int> sig;
        sig.connect(p, &Obj::f);
        const int iterations = 200000;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i)
            sig(i);
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "             single member tracked: " << iterations << " emits in " << dur
                  << " us (" << us_to_ns_per_op(dur, iterations) << " ns/emit)" << std::endl;
    }

    ASSERT_TRUE(true);
}

// 基准测试：不同槽数量下的发射性能
// 测试随着槽数量增加，单次发射的开销如何变化（并验证计数正确性）
TEST_CASE(many_slots_scaled)
{
    const std::vector<int> slot_counts = {1, 10, 100, 500};
    for (int num_slots : slot_counts)
    {
        xswl::signal_t<> sig;
        std::atomic<int> counter{0};
        std::vector<xswl::connection_t<>> connections;

        for (int i = 0; i < num_slots; ++i)
            connections.push_back(sig.connect([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));

        int iterations = 200000;
        if (num_slots >= 100) iterations = 20000;
        if (num_slots >= 500) iterations = 4000;

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i)
            sig();
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        std::cout << "             " << iterations << " emits with " << num_slots
                  << " slots in " << dur << " us (" << us_to_ns_per_op(dur, iterations)
                  << " ns/emit)" << std::endl;

        ASSERT_EQ(counter.load(), num_slots * iterations);
    }
}

// 基准测试：并发发射（多线程）
// 用于验证多线程同时发射信号时的正确性与性能表现
TEST_CASE(concurrent_emits)
{
    xswl::signal_t<> sig;
    std::atomic<int> counter{0};
    const int num_slots = 10;
    const int threads = std::min(4u, std::thread::hardware_concurrency());
    const int per_thread = 2000;

    for (int i = 0; i < num_slots; ++i)
        sig.connect([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });

    std::vector<std::thread> ths;
    auto start = std::chrono::high_resolution_clock::now();
    for (int t = 0; t < threads; ++t)
    {
        ths.emplace_back([&sig, per_thread]() {
            for (int i = 0; i < per_thread; ++i)
                sig();
        });
    }
    for (auto &t : ths) t.join();
    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    const int expected = threads * per_thread * num_slots;
    std::cout << "             concurrent: " << threads << " threads x " << per_thread
              << " emits with " << num_slots << " slots in " << dur << " us" << std::endl;

    ASSERT_EQ(counter.load(), expected);
}

// 基准测试：批量连接/断开开销
// 测试频繁创建并断开连接的开销（例如临时监听场景）
TEST_CASE(connect_disconnect_batch_performance)
{
    xswl::signal_t<> sig;

    const int iterations = 20000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i)
    {
        auto conn = sig.connect([]() {});
        conn.disconnect();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "             " << iterations << " connect/disconnect in "
              << dur << " us (" << us_to_ns_per_op(dur, iterations) << " ns/op)" << std::endl;

    ASSERT_TRUE(sig.empty());
}
