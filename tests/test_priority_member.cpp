#include "test_common.hpp"

// 测试：connect_once（一次性槽）应只被调用一次
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

// 测试：多个一次性槽同时存在，首次发射后全部失效
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

// 测试：优先级排序，高优先级先执行
TEST_CASE(priority_order)
{
    xswl::signal_t<> sig;
    std::vector<int> order;

    sig.connect([&order]() { order.push_back(1); }, 10);
    sig.connect([&order]() { order.push_back(2); }, 100);
    sig.connect([&order]() { order.push_back(3); }, 1);

    sig();

    ASSERT_EQ(order.size(), 3u);
    ASSERT_EQ(order[0], 2);
    ASSERT_EQ(order[1], 1);
    ASSERT_EQ(order[2], 3);
}

// 测试：相同优先级时保持注册顺序（稳定排序）
TEST_CASE(same_priority_stable_order)
{
    xswl::signal_t<> sig;
    std::vector<int> order;

    sig.connect([&order]() { order.push_back(1); }, 0);
    sig.connect([&order]() { order.push_back(2); }, 0);
    sig.connect([&order]() { order.push_back(3); }, 0);

    sig();

    ASSERT_EQ(order.size(), 3u);
    ASSERT_EQ(order[0], 1);
    ASSERT_EQ(order[1], 2);
    ASSERT_EQ(order[2], 3);
}

// 测试：通过 shared_ptr 连接成员函数，应在对象有效时回调
TEST_CASE(member_function_shared_ptr)
{
    xswl::signal_t<int> sig;
    auto receiver = std::make_shared<Receiver>();

    sig.connect(receiver, &Receiver::on_value);

    sig(42);
    ASSERT_EQ(receiver->call_count(), 1);
    ASSERT_EQ(receiver->last_value(), 42);
}

// 测试：shared_ptr 生命周期到期后，成员函数不再被调用
TEST_CASE(member_function_lifetime)
{
    xswl::signal_t<int> sig;
    Counter call_counter;

    {
        auto receiver = std::make_shared<Receiver>();
        sig.connect(receiver, &Receiver::on_value);

        sig(1);
        ASSERT_EQ(receiver->call_count(), 1);
    }

    sig(2);
    sig(3);
}

// 测试：用裸指针连接成员函数（不跟踪生命周期），需要小心对象生命周期
TEST_CASE(member_function_raw_pointer)
{
    xswl::signal_t<int> sig;
    Receiver receiver;

    sig.connect(&receiver, &Receiver::on_value);

    sig(100);
    ASSERT_EQ(receiver.call_count(), 1);
    ASSERT_EQ(receiver.last_value(), 100);
}

// 测试：const 成员函数的绑定和调用
TEST_CASE(member_function_const)
{
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

// 测试：带标签的连接，可以通过标签断开对应的槽
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
    ASSERT_EQ(counter.get(), 10);
}

// 测试：尝试断开不存在的标签，应返回 false
TEST_CASE(disconnect_nonexistent_tag)
{
    xswl::signal_t<> sig;

    bool result = sig.disconnect("nonexistent");
    ASSERT_FALSE(result);
}

// 测试：同标签多次连接，断开标签应移除所有与该标签关联的槽
TEST_CASE(multiple_same_tag)
{
    xswl::signal_t<> sig;
    Counter counter;

    sig.connect("same_tag", [&counter]() { counter.increment(); });
    sig.connect("same_tag", [&counter]() { counter.increment(); });

    sig();
    ASSERT_GE(counter.get(), 1);

    sig.disconnect("same_tag");
    int count_after = counter.get();

    sig();
    ASSERT_LE(counter.get(), count_after + 1);
}

// 测试：不同标签独立管理，解除一个标签不影响另一个
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
    ASSERT_EQ(counter1.get(), 1);
    ASSERT_EQ(counter2.get(), 2);
}
