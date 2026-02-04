#include "test_common.hpp"

// 测试：连接断开后槽不再被调用
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

// 测试：阻塞 / 解除阻塞连接时的行为
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

// 测试：`scoped_connection_t` 的 RAII 语义（析构时自动断开）
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

// 测试：scoped_connection 的移动语义与持续有效性
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

// 测试：connection_group_t 管理多个连接并能一次性断开
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

// 测试：disconnect_all 清除所有槽并恢复为空状态
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
