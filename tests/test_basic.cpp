#include "test_common.hpp"

// 基础测试：简单信号发射与槽调用计数
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

// 基准：带参数的信号与槽参数传递正确性
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

// 基准：多参数信号与槽的参数传递和类型匹配
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

// 测试：多个槽连接于同一信号，全部应该被调用
TEST_CASE(multiple_slots)
{
    xswl::signal_t<int> sig;
    std::vector<int> results;

    sig.connect([&results](int v) { results.push_back(v * 1); });
    sig.connect([&results](int v) { results.push_back(v * 2); });
    sig.connect([&results](int v) { results.push_back(v * 3); });

    sig(10);

    ASSERT_EQ(results.size(), 3u);
}

// 测试：空信号状态检查与基本行为（发射空信号不崩溃）
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
