#include "test_common.hpp"

// 边界测试：参数适配器支持大量（>6）参数的信号与槽
TEST_CASE(large_parameter_adapter)
{
    xswl::signal_t<int,int,int,int,int,int,int,int> sig;
    int received = 0;
    sig.connect([&received](int v) { received = v; });

    sig(7, 6, 5, 4, 3, 2, 1, 0);
    ASSERT_EQ(received, 7);
}

// 测试：信号对象移动语义，移动后原对象应为空，目标对象保留槽
TEST_CASE(signal_move_semantics)
{
    xswl::signal_t<> sig;
    Counter c;

    sig.connect([&c]() { c.increment(); });

    auto sig2 = std::move(sig);
    sig2();
    ASSERT_EQ(c.get(), 1);

    // 原对象应不再含有槽
    ASSERT_TRUE(sig.empty());
}

// 边界测试：成员函数参数边界（6 vs 7），验证 arity 萃取与适配行为
TEST_CASE(member_function_arity_boundary)
{
    struct M {
        int called = 0;
        void m6(int a1, int a2, int a3, int a4, int a5, int a6) { called = a1 + a2 + a3 + a4 + a5 + a6; }
        void m7(int a1, int a2, int a3, int a4, int a5, int a6, int a7) { called = a1; (void)a7; }
    };

    {
        auto obj = std::make_shared<M>();
        xswl::signal_t<int, int, int, int, int, int> sig6;
        sig6.connect(obj, &M::m6);
        sig6(1, 2, 3, 4, 5, 6);
        ASSERT_EQ(obj->called, 21);
    }

    static_assert(xswl::detail::member_function_arity<decltype(&M::m6)>::value == 6, "m6 should be arity 6");
}
