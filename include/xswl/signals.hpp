#ifndef XSWL_SIGNALS_H
#define XSWL_SIGNALS_H

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef emit
    #define emit
#endif

namespace xswl {

template <typename... Args>
class signal_t;

template <typename... Args>
class connection_t;

class scoped_connection_t;
class connection_group_t;

namespace detail {

// ============================================================================
// C++11 index_sequence 实现
// ============================================================================
template <std::size_t... Is>
struct index_sequence
{
};

template <std::size_t N, std::size_t... Is>
struct make_index_sequence_impl : make_index_sequence_impl<N - 1, N - 1, Is...>
{
};

template <std::size_t... Is>
struct make_index_sequence_impl<0, Is...>
{
    typedef index_sequence<Is...> type;
};

template <std::size_t N>
struct make_index_sequence
{
    typedef typename make_index_sequence_impl<N>::type type;
};

// ============================================================================
// 可调用性检测
// ============================================================================

// 检测 F 是否可以用 Args... 调用
template <typename F, typename... Args>
struct is_invocable
{
private:
    template <typename Fn>
    static auto test(int)
        -> decltype(std::declval<Fn>()(std::declval<Args>()...), std::true_type{});

    template <typename>
    static std::false_type test(...);

public:
    static const bool value = decltype(test<F>(0))::value;
};

// 检测 F 是否可以用 tuple 中前 N 个元素调用
template <typename F, typename Tuple, typename Indices>
struct is_invocable_with_n_impl;

template <typename F, typename Tuple, std::size_t... Is>
struct is_invocable_with_n_impl<F, Tuple, index_sequence<Is...>>
{
private:
    template <typename Fn>
    static auto test(int)
        -> decltype(std::declval<Fn>()(
                        std::declval<typename std::tuple_element<Is, Tuple>::type>()...),
                    std::true_type{});

    template <typename>
    static std::false_type test(...);

public:
    static const bool value = decltype(test<F>(0))::value;
};

template <typename F, std::size_t N, typename... Args>
struct is_invocable_with_n
{
    typedef std::tuple<Args...> tuple_type;
    typedef typename make_index_sequence<N>::type indices;
    static const bool value = is_invocable_with_n_impl<F, tuple_type, indices>::value;
};

// ============================================================================
// 参数数量检测（找到最大可调用参数数量）
// ============================================================================
template <typename F, std::size_t N, std::size_t Max, typename... Args>
struct find_callable_arity_impl
{
    static const std::size_t value =
        is_invocable_with_n<F, N, Args...>::value
            ? N
            : find_callable_arity_impl<F, N - 1, Max, Args...>::value;
};

template <typename F, std::size_t Max, typename... Args>
struct find_callable_arity_impl<F, 0, Max, Args...>
{
    static const std::size_t value = is_invocable<F>::value ? 0 : Max + 1; // Max+1 表示无法调用
};

template <typename F, typename... Args>
struct callable_arity
{
    static const std::size_t max_args = sizeof...(Args);
    static const std::size_t value =
        find_callable_arity_impl<F, max_args, max_args, Args...>::value;
    static const bool is_valid = (value <= max_args);
};

// ============================================================================
// 参数适配器：将信号参数适配为槽函数需要的参数数量
// ============================================================================
template <typename Fn, std::size_t N>
struct arg_adapter;

// 无参数版本
template <typename Fn>
struct arg_adapter<Fn, 0>
{
    Fn fn;
    explicit arg_adapter(Fn f)
        : fn(std::move(f)) {}

    template <typename... A>
    void operator()(A &&...)
    {
        fn();
    }

    template <typename... A>
    void operator()(A &&...) const
    {
        fn();
    }
};

// 1 个参数版本
template <typename Fn>
struct arg_adapter<Fn, 1>
{
    Fn fn;
    explicit arg_adapter(Fn f)
        : fn(std::move(f)) {}

    template <typename A1, typename... Rest>
    void operator()(A1 &&a1, Rest &&...)
    {
        fn(std::forward<A1>(a1));
    }

    template <typename A1, typename... Rest>
    void operator()(A1 &&a1, Rest &&...) const
    {
        fn(std::forward<A1>(a1));
    }
};

// 2 个参数版本
template <typename Fn>
struct arg_adapter<Fn, 2>
{
    Fn fn;
    explicit arg_adapter(Fn f)
        : fn(std::move(f)) {}

    template <typename A1, typename A2, typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, Rest &&...)
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2));
    }

    template <typename A1, typename A2, typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, Rest &&...) const
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2));
    }
};

// 3 个参数版本
template <typename Fn>
struct arg_adapter<Fn, 3>
{
    Fn fn;
    explicit arg_adapter(Fn f)
        : fn(std::move(f)) {}

    template <typename A1, typename A2, typename A3, typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, A3 &&a3, Rest &&...)
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3));
    }

    template <typename A1, typename A2, typename A3, typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, A3 &&a3, Rest &&...) const
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3));
    }
};

// 4 个参数版本
template <typename Fn>
struct arg_adapter<Fn, 4>
{
    Fn fn;
    explicit arg_adapter(Fn f)
        : fn(std::move(f)) {}

    template <typename A1, typename A2, typename A3, typename A4, typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4, Rest &&...)
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
           std::forward<A4>(a4));
    }

    template <typename A1, typename A2, typename A3, typename A4, typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4, Rest &&...) const
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
           std::forward<A4>(a4));
    }
};

// 5 个参数版本
template <typename Fn>
struct arg_adapter<Fn, 5>
{
    Fn fn;
    explicit arg_adapter(Fn f)
        : fn(std::move(f)) {}

    template <typename A1, typename A2, typename A3, typename A4, typename A5,
              typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4, A5 &&a5, Rest &&...)
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
           std::forward<A4>(a4), std::forward<A5>(a5));
    }

    template <typename A1, typename A2, typename A3, typename A4, typename A5,
              typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4, A5 &&a5, Rest &&...) const
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
           std::forward<A4>(a4), std::forward<A5>(a5));
    }
};

// 6 个参数版本
template <typename Fn>
struct arg_adapter<Fn, 6>
{
    Fn fn;
    explicit arg_adapter(Fn f)
        : fn(std::move(f)) {}

    template <typename A1, typename A2, typename A3, typename A4, typename A5,
              typename A6, typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4, A5 &&a5, A6 &&a6, Rest &&...)
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
           std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6));
    }

    template <typename A1, typename A2, typename A3, typename A4, typename A5,
              typename A6, typename... Rest>
    void operator()(A1 &&a1, A2 &&a2, A3 &&a3, A4 &&a4, A5 &&a5, A6 &&a6,
                    Rest &&...) const
    {
        fn(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3),
           std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6));
    }
};

// 适配器工厂函数
template <std::size_t N, typename Fn>
arg_adapter<typename std::decay<Fn>::type, N> make_arg_adapter(Fn &&fn)
{
    return arg_adapter<typename std::decay<Fn>::type, N>(std::forward<Fn>(fn));
}

// ============================================================================
// 成员函数参数数量萃取
// ============================================================================
template <typename T>
struct member_function_arity;

template <typename C, typename R>
struct member_function_arity<R (C::*)()>
{
    static const std::size_t value = 0;
};

template <typename C, typename R>
struct member_function_arity<R (C::*)() const>
{
    static const std::size_t value = 0;
};

template <typename C, typename R, typename A1>
struct member_function_arity<R (C::*)(A1)>
{
    static const std::size_t value = 1;
};

template <typename C, typename R, typename A1>
struct member_function_arity<R (C::*)(A1) const>
{
    static const std::size_t value = 1;
};

template <typename C, typename R, typename A1, typename A2>
struct member_function_arity<R (C::*)(A1, A2)>
{
    static const std::size_t value = 2;
};

template <typename C, typename R, typename A1, typename A2>
struct member_function_arity<R (C::*)(A1, A2) const>
{
    static const std::size_t value = 2;
};

template <typename C, typename R, typename A1, typename A2, typename A3>
struct member_function_arity<R (C::*)(A1, A2, A3)>
{
    static const std::size_t value = 3;
};

template <typename C, typename R, typename A1, typename A2, typename A3>
struct member_function_arity<R (C::*)(A1, A2, A3) const>
{
    static const std::size_t value = 3;
};

template <typename C, typename R, typename A1, typename A2, typename A3, typename A4>
struct member_function_arity<R (C::*)(A1, A2, A3, A4)>
{
    static const std::size_t value = 4;
};

template <typename C, typename R, typename A1, typename A2, typename A3, typename A4>
struct member_function_arity<R (C::*)(A1, A2, A3, A4) const>
{
    static const std::size_t value = 4;
};

template <typename C, typename R, typename A1, typename A2, typename A3, typename A4,
          typename A5>
struct member_function_arity<R (C::*)(A1, A2, A3, A4, A5)>
{
    static const std::size_t value = 5;
};

template <typename C, typename R, typename A1, typename A2, typename A3, typename A4,
          typename A5>
struct member_function_arity<R (C::*)(A1, A2, A3, A4, A5) const>
{
    static const std::size_t value = 5;
};

template <typename C, typename R, typename A1, typename A2, typename A3, typename A4,
          typename A5, typename A6>
struct member_function_arity<R (C::*)(A1, A2, A3, A4, A5, A6)>
{
    static const std::size_t value = 6;
};

template <typename C, typename R, typename A1, typename A2, typename A3, typename A4,
          typename A5, typename A6>
struct member_function_arity<R (C::*)(A1, A2, A3, A4, A5, A6) const>
{
    static const std::size_t value = 6;
};

// ============================================================================
// 槽函数封装
// ============================================================================
template <typename... Args>
struct slot
{
    using function_type = std::function<void(Args...)>;

    function_type func;
    int priority;
    std::atomic<bool> blocked;         // 是否被 block
    std::atomic<bool> pending_removal; // 是否等待删除
    std::atomic<bool> executed;        // 用于单次槽的 CAS 控制
    bool single_shot;                  // 是否一次性
    std::weak_ptr<void> tracked;       // 跟踪的 owner/tag（生命周期控制）

    slot(function_type f, int p, bool ss, std::weak_ptr<void> t)
        : func(std::move(f))
        , priority(p)
        , blocked(false)
        , pending_removal(false)
        , executed(false)
        , single_shot(ss)
        , tracked(std::move(t))
    {
    }

    // tracked 是否曾经被设置（区别 default weak_ptr）
    bool has_tracked_object() const
    {
        return tracked.owner_before(std::weak_ptr<void>{}) || std::weak_ptr<void>{}.owner_before(tracked);
    }

    // 基础检查（不包含单次槽的执行状态）
    bool is_callable() const
    {
        if(blocked.load(std::memory_order_acquire))
            return false;

        if(pending_removal.load(std::memory_order_acquire))
            return false;

        if(has_tracked_object() && tracked.expired())
            return false;

        return true;
    }

    // 尝试获取执行权（用于单次槽的线程安全）
    bool try_acquire_execution()
    {
        if(!single_shot)
            return true;

        bool expected = false;
        return executed.compare_exchange_strong(expected, true,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire);
    }
};

// 成员函数指针检测
template <typename T>
struct is_member_function_pointer : std::false_type
{
};

template <typename C, typename R, typename... A>
struct is_member_function_pointer<R (C::*)(A...)> : std::true_type
{
};

template <typename C, typename R, typename... A>
struct is_member_function_pointer<R (C::*)(A...) const> : std::true_type
{
};

struct connection_tag
{
    explicit connection_tag(std::string n)
        : name(std::move(n)) {}
    std::string name;
};

// ============================================================================
// 信号内部实现（共享状态）
// ============================================================================
template <typename... Args>
class signal_impl
{
public:
    using slot_type = slot<Args...>;
    using slot_ptr  = std::shared_ptr<slot_type>;

    std::mutex mutex_;
    std::vector<slot_ptr> slots_;
    std::vector<std::shared_ptr<connection_tag>> tags_;
    bool dirty_ = false; // 是否需要清理 or 重排

    void disconnect_slot(const slot_ptr &s)
    {
        if(!s)
            return;
        std::lock_guard<std::mutex> lk(mutex_);
        s->pending_removal.store(true, std::memory_order_release);
        dirty_ = true;
    }

    void cleanup_slots_locked()
    {
        auto it = std::remove_if(
            slots_.begin(), slots_.end(),
            [](const slot_ptr &s) {
                return !s || s->pending_removal.load(std::memory_order_acquire);
            });
        slots_.erase(it, slots_.end());
    }
};

} // namespace detail

// ============================================================================
// 连接句柄
// ============================================================================
template <typename... Args>
class connection_t
{
public:
    using impl_type = detail::signal_impl<Args...>;
    using slot_type = detail::slot<Args...>;

    connection_t() = default;

    connection_t(const std::shared_ptr<impl_type> &impl,
                 const std::shared_ptr<slot_type> &s)
        : impl_(impl)
        , slot_(s)
    {
    }

    // 拷贝/移动：复制 weak_ptr 即可
    connection_t(const connection_t &)            = default;
    connection_t &operator=(const connection_t &) = default;
    connection_t(connection_t &&)                 = default;
    connection_t &operator=(connection_t &&)      = default;

    // 是否仍然连接有效
    bool is_connected() const
    {
        auto s = slot_.lock();
        return s && !s->pending_removal.load(std::memory_order_acquire);
    }

    explicit operator bool() const { return is_connected(); }

    // 主动断开
    void disconnect()
    {
        auto impl = impl_.lock();
        auto s    = slot_.lock();
        if(impl && s)
        {
            impl->disconnect_slot(s);
        }
        // 如果 impl 已销毁，则什么也不做（安全）
    }

    // 阻塞 / 解除阻塞
    void block(bool b = true)
    {
        if(auto s = slot_.lock())
        {
            s->blocked.store(b, std::memory_order_release);
        }
    }

    void unblock() { block(false); }

    bool is_blocked() const
    {
        auto s = slot_.lock();
        return s && s->blocked.load(std::memory_order_acquire);
    }

    // 释放引用（不影响实际连接）
    void reset()
    {
        impl_.reset();
        slot_.reset();
    }

private:
    std::weak_ptr<impl_type> impl_;
    std::weak_ptr<slot_type> slot_;
};

// ============================================================================
// 信号类
// ============================================================================
template <typename... Args>
class signal_t
{
public:
    using impl_type     = detail::signal_impl<Args...>;
    using slot_type     = detail::slot<Args...>;
    using function_type = typename slot_type::function_type;
    using slot_ptr      = std::shared_ptr<slot_type>;

    signal_t()
        : impl_(std::make_shared<impl_type>())
    {
    }

    ~signal_t()
    {
        disconnect_all();
    }

    signal_t(signal_t &&other) noexcept
        : impl_(std::move(other.impl_))
    {
    }

    signal_t &operator=(signal_t &&other) noexcept
    {
        if(this != &other)
        {
            disconnect_all();
            impl_ = std::move(other.impl_);
        }
        return *this;
    }

    signal_t(const signal_t &)            = delete;
    signal_t &operator=(const signal_t &) = delete;

    // -------------------------------------------------------------------------
    // connect：任意可调用对象（非成员函数指针）|支持参数适配（槽可以接受比信号更少的参数）
    // -------------------------------------------------------------------------
    template <typename Fn>
    typename std::enable_if<!detail::is_member_function_pointer<typename std::decay<Fn>::type>::value
                                && detail::callable_arity<Fn, Args...>::is_valid,
                            connection_t<Args...>>::type
    connect(Fn &&func, int priority = 0)
    {
        return connect_with_arity(std::forward<Fn>(func), priority, false, std::weak_ptr<void>(),
                                  std::integral_constant<std::size_t, detail::callable_arity<Fn, Args...>::value>());
    }

    // 单次连接
    template <typename Fn>
    typename std::enable_if<detail::callable_arity<Fn, Args...>::is_valid,
                            connection_t<Args...>>::type
    connect_once(Fn &&func, int priority = 0)
    {
        return connect_with_arity(std::forward<Fn>(func), priority, true, std::weak_ptr<void>(),
                                  std::integral_constant<std::size_t,
                                                         detail::callable_arity<Fn, Args...>::value>());
    }

    // ---------------------------------------------------------------------
    // connect：成员函数 + shared_ptr（支持参数适配 + 自动跟踪对象生命周期）
    // ---------------------------------------------------------------------
    template <typename Obj, typename MemFn>
    typename std::enable_if<detail::is_member_function_pointer<typename std::decay<MemFn>::type>::value,
                            connection_t<Args...>>::type
    connect(const std::shared_ptr<Obj> &obj, MemFn memfn, int priority = 0)
    {
        if(!obj)
            return connection_t<Args...>();

        return connect_member_with_arity(
            obj, memfn, priority, false,
            std::integral_constant<
                std::size_t,
                detail::member_function_arity<MemFn>::value>());
    }

    // ---------------------------------------------------------------------
    // connect：成员函数 + 裸指针（支持参数适配 + 不跟踪生命周期）
    // ---------------------------------------------------------------------
    template <typename Obj, typename MemFn>
    typename std::enable_if<detail::is_member_function_pointer<typename std::decay<MemFn>::type>::value,
                            connection_t<Args...>>::type
    connect(Obj *obj, MemFn memfn, int priority = 0)
    {
        if(!obj)
            return connection_t<Args...>();

        return connect_raw_member_with_arity(
            obj, memfn, priority, false,
            std::integral_constant<
                std::size_t,
                detail::member_function_arity<MemFn>::value>());
    }

    // -------------------------------------------------------------------------
    // 带标签的连接（支持参数适配）
    // -------------------------------------------------------------------------
    template <typename Fn>
    typename std::enable_if<detail::callable_arity<Fn, Args...>::is_valid,
                            connection_t<Args...>>::type
    connect(const std::string &tag, Fn &&func, int priority = 0)
    {
        if(!impl_)
            return connection_t<Args...>();

        std::shared_ptr<detail::connection_tag> tag_ptr = get_or_create_tag(tag);
        return connect_with_arity(
            std::forward<Fn>(func), priority, false, std::weak_ptr<void>(tag_ptr),
            std::integral_constant<std::size_t,
                                   detail::callable_arity<Fn, Args...>::value>());
    }

    // -------------------------------------------------------------------------
    // 通过标签断开
    // -------------------------------------------------------------------------
    bool disconnect(const std::string &tag)
    {
        if(!impl_)
            return false;

        std::lock_guard<std::mutex> lk(impl_->mutex_);

        auto it = std::find_if(
            impl_->tags_.begin(), impl_->tags_.end(),
            [&tag](const std::shared_ptr<detail::connection_tag> &t) {
                return t && t->name == tag;
            });

        if(it == impl_->tags_.end())
            return false;

        auto tag_ptr = *it;
        impl_->tags_.erase(it);

        for(auto &s : impl_->slots_)
        {
            if(s && s->tracked.lock() == tag_ptr)
            {
                s->pending_removal.store(true, std::memory_order_release);
            }
        }
        impl_->dirty_ = true;
        return true;
    }

    // -------------------------------------------------------------------------
    // 发射信号
    // -------------------------------------------------------------------------
    void operator()(Args... args) const
    {
        if(!impl_)
            return;

        std::vector<slot_ptr> local_slots;
        {
            std::lock_guard<std::mutex> lk(impl_->mutex_);
            if(impl_->slots_.empty())
                return;

            if(impl_->dirty_)
            {
                impl_->cleanup_slots_locked();
                std::stable_sort(impl_->slots_.begin(), impl_->slots_.end(),
                                 [](const slot_ptr &a, const slot_ptr &b) {
                                     return a->priority > b->priority;
                                 });
                impl_->dirty_ = false;
            }
            local_slots = impl_->slots_; // 拷贝一份，避免长时间持锁
        }

        bool need_cleanup = false;

        for(const auto &sp : local_slots)
        {
            if(!sp)
                continue;

            // 检查 tracked 对象是否过期
            if(sp->has_tracked_object() && sp->tracked.expired())
            {
                sp->pending_removal.store(true, std::memory_order_release);
                need_cleanup = true;
                continue;
            }

            // 基础可调用性检查
            if(!sp->is_callable())
                continue;

            // 单次槽：使用 CAS 确保只有一个线程执行
            if(!sp->try_acquire_execution())
            {
                continue;
            }

            // 标记单次槽为待删除
            if(sp->single_shot)
            {
                sp->pending_removal.store(true, std::memory_order_release);
                need_cleanup = true;
            }

            try
            {
                sp->func(args...);
            }
            catch(...)
            {
                // 异常吞噬，防止影响其他槽
            }
        }

        if(need_cleanup)
        {
            std::lock_guard<std::mutex> lk(impl_->mutex_);
            impl_->dirty_ = true;
        }
    }

    void emit_signal(Args... args) const
    {
        (*this)(args...);
    }

    // -------------------------------------------------------------------------
    // 管理接口
    // -------------------------------------------------------------------------
    void disconnect_all()
    {
        if(!impl_)
            return;

        std::lock_guard<std::mutex> lk(impl_->mutex_);
        for(auto &s : impl_->slots_)
        {
            if(s)
                s->pending_removal.store(true, std::memory_order_release);
        }
        impl_->slots_.clear();
        impl_->tags_.clear();
        impl_->dirty_ = false;
    }

    // 槽数量（过滤掉已标记删除的）
    std::size_t slot_count() const
    {
        if(!impl_)
            return 0;

        std::lock_guard<std::mutex> lk(impl_->mutex_);
        std::size_t count = 0;
        for(auto &s : impl_->slots_)
        {
            if(s && !s->pending_removal.load(std::memory_order_acquire))
            {
                ++count;
            }
        }
        return count;
    }

    bool empty() const
    {
        return slot_count() == 0;
    }

    bool valid() const
    {
        return impl_ != nullptr;
    }

private:
    std::shared_ptr<impl_type> impl_;

    friend class connection_t<Args...>;

    // -------------------------------------------------------------------------
    // 参数适配分发（完整参数，无需适配）
    // -------------------------------------------------------------------------
    template <typename Fn>
    connection_t<Args...> connect_with_arity(Fn &&func,
                                             int priority,
                                             bool single_shot,
                                             std::weak_ptr<void> tracked,
                                             std::integral_constant<std::size_t, sizeof...(Args)>)
    {
        return connect_impl(function_type(std::forward<Fn>(func)), priority,
                            single_shot, std::move(tracked));
    }

    // 参数适配分发（需要适配）
    template <typename Fn, std::size_t N>
    connection_t<Args...> connect_with_arity(Fn &&func,
                                             int priority,
                                             bool single_shot,
                                             std::weak_ptr<void> tracked,
                                             std::integral_constant<std::size_t, N>)
    {
        auto adapter = detail::make_arg_adapter<N>(std::forward<Fn>(func));
        return connect_impl(function_type(adapter), priority, single_shot,
                            std::move(tracked));
    }

    // -------------------------------------------------------------------------
    // 成员函数连接（shared_ptr）
    // -------------------------------------------------------------------------
    template <typename Obj, typename MemFn>
    connection_t<Args...> connect_member_with_arity(
        const std::shared_ptr<Obj> &obj,
        MemFn memfn,
        int priority,
        bool single_shot,
        std::integral_constant<std::size_t, sizeof...(Args)>)
    {
        std::weak_ptr<Obj> weak_obj = obj;
        auto wrapper                = [weak_obj, memfn](Args... args) {
            std::shared_ptr<Obj> sp = weak_obj.lock();
            if(sp)
            {
                (sp.get()->*memfn)(args...);
            }
        };
        return connect_impl(function_type(wrapper), priority, single_shot,
                            std::weak_ptr<void>(obj));
    }

    template <typename Obj, typename MemFn, std::size_t N>
    connection_t<Args...> connect_member_with_arity(
        const std::shared_ptr<Obj> &obj,
        MemFn memfn,
        int priority,
        bool single_shot,
        std::integral_constant<std::size_t, N>)
    {
        std::weak_ptr<Obj> weak_obj = obj;
        auto wrapper                = [weak_obj, memfn](Args... args) {
            std::shared_ptr<Obj> sp = weak_obj.lock();
            if(sp)
            {
                call_member_with_n_args<N>(sp.get(), memfn, args...);
            }
        };
        return connect_impl(function_type(wrapper), priority, single_shot,
                            std::weak_ptr<void>(obj));
    }

    // -------------------------------------------------------------------------
    // 成员函数连接（裸指针）
    // -------------------------------------------------------------------------
    template <typename Obj, typename MemFn>
    connection_t<Args...> connect_raw_member_with_arity(
        Obj *obj,
        MemFn memfn,
        int priority,
        bool single_shot,
        std::integral_constant<std::size_t, sizeof...(Args)>)
    {
        auto wrapper = [obj, memfn](Args... args) { (obj->*memfn)(args...); };
        return connect_impl(function_type(wrapper), priority, single_shot,
                            std::weak_ptr<void>());
    }

    template <typename Obj, typename MemFn, std::size_t N>
    connection_t<Args...> connect_raw_member_with_arity(
        Obj *obj,
        MemFn memfn,
        int priority,
        bool single_shot,
        std::integral_constant<std::size_t, N>)
    {
        auto wrapper = [obj, memfn](Args... args) {
            call_member_with_n_args<N>(obj, memfn, args...);
        };
        return connect_impl(function_type(wrapper), priority, single_shot,
                            std::weak_ptr<void>());
    }

    // -------------------------------------------------------------------------
    // 辅助函数：调用成员函数（只传递前 N 个参数）
    // -------------------------------------------------------------------------
    template <std::size_t N, typename Obj, typename MemFn, typename... CallArgs>
    static typename std::enable_if<N == 0>::type
    call_member_with_n_args(Obj *obj, MemFn memfn, CallArgs &&...)
    {
        (obj->*memfn)();
    }

    template <std::size_t N, typename Obj, typename MemFn, typename A1,
              typename... Rest>
    static typename std::enable_if<N == 1>::type
    call_member_with_n_args(Obj *obj, MemFn memfn, A1 &&a1, Rest &&...)
    {
        (obj->*memfn)(std::forward<A1>(a1));
    }

    template <std::size_t N, typename Obj, typename MemFn, typename A1, typename A2,
              typename... Rest>
    static typename std::enable_if<N == 2>::type
    call_member_with_n_args(Obj *obj, MemFn memfn, A1 &&a1, A2 &&a2, Rest &&...)
    {
        (obj->*memfn)(std::forward<A1>(a1), std::forward<A2>(a2));
    }

    template <std::size_t N, typename Obj, typename MemFn, typename A1, typename A2,
              typename A3, typename... Rest>
    static typename std::enable_if<N == 3>::type
    call_member_with_n_args(Obj *obj, MemFn memfn, A1 &&a1, A2 &&a2, A3 &&a3,
                            Rest &&...)
    {
        (obj->*memfn)(std::forward<A1>(a1), std::forward<A2>(a2),
                      std::forward<A3>(a3));
    }

    template <std::size_t N, typename Obj, typename MemFn, typename A1, typename A2,
              typename A3, typename A4, typename... Rest>
    static typename std::enable_if<N == 4>::type
    call_member_with_n_args(Obj *obj, MemFn memfn, A1 &&a1, A2 &&a2, A3 &&a3,
                            A4 &&a4, Rest &&...)
    {
        (obj->*memfn)(std::forward<A1>(a1), std::forward<A2>(a2),
                      std::forward<A3>(a3), std::forward<A4>(a4));
    }

    template <std::size_t N, typename Obj, typename MemFn, typename A1, typename A2,
              typename A3, typename A4, typename A5, typename... Rest>
    static typename std::enable_if<N == 5>::type
    call_member_with_n_args(Obj *obj, MemFn memfn, A1 &&a1, A2 &&a2, A3 &&a3,
                            A4 &&a4, A5 &&a5, Rest &&...)
    {
        (obj->*memfn)(std::forward<A1>(a1), std::forward<A2>(a2),
                      std::forward<A3>(a3), std::forward<A4>(a4),
                      std::forward<A5>(a5));
    }

    template <std::size_t N, typename Obj, typename MemFn, typename A1, typename A2,
              typename A3, typename A4, typename A5, typename A6, typename... Rest>
    static typename std::enable_if<N == 6>::type
    call_member_with_n_args(Obj *obj, MemFn memfn, A1 &&a1, A2 &&a2, A3 &&a3,
                            A4 &&a4, A5 &&a5, A6 &&a6, Rest &&...)
    {
        (obj->*memfn)(std::forward<A1>(a1), std::forward<A2>(a2),
                      std::forward<A3>(a3), std::forward<A4>(a4),
                      std::forward<A5>(a5), std::forward<A6>(a6));
    }

    // -------------------------------------------------------------------------
    // 实际连接实现
    // -------------------------------------------------------------------------
    connection_t<Args...> connect_impl(function_type f,
                                       int p,
                                       bool ss,
                                       std::weak_ptr<void> tracked)
    {
        if(!impl_)
            return connection_t<Args...>();

        auto s = std::make_shared<slot_type>(std::move(f), p, ss, std::move(tracked));
        {
            std::lock_guard<std::mutex> lk(impl_->mutex_);
            impl_->slots_.push_back(s);
            impl_->dirty_ = true;
        }
        return connection_t<Args...>(impl_, s);
    }

    std::shared_ptr<detail::connection_tag> get_or_create_tag(const std::string &name)
    {
        std::lock_guard<std::mutex> lk(impl_->mutex_);
        for(auto &t : impl_->tags_)
        {
            if(t && t->name == name)
                return t;
        }
        auto t = std::make_shared<detail::connection_tag>(name);
        impl_->tags_.push_back(t);
        return t;
    }
};

// ============================================================================
// scoped_connection_t：RAII 管理单个 connection
// ============================================================================
class scoped_connection_t
{
public:
    scoped_connection_t() {}

    template <typename... Args>
    scoped_connection_t(connection_t<Args...> c)
        : disconnector_([c]() mutable { c.disconnect(); })
    {
    }

    ~scoped_connection_t()
    {
        disconnect();
    }

    scoped_connection_t(scoped_connection_t &&other) noexcept
        : disconnector_(std::move(other.disconnector_))
    {
        other.disconnector_ = nullptr;
    }

    scoped_connection_t &operator=(scoped_connection_t &&other) noexcept
    {
        if(this != &other)
        {
            disconnect();
            disconnector_       = std::move(other.disconnector_);
            other.disconnector_ = nullptr;
        }
        return *this;
    }

    scoped_connection_t(const scoped_connection_t &)            = delete;
    scoped_connection_t &operator=(const scoped_connection_t &) = delete;

    template <typename... Args>
    scoped_connection_t &operator=(connection_t<Args...> c)
    {
        disconnect();
        disconnector_ = [c]() mutable { c.disconnect(); };
        return *this;
    }

    void disconnect()
    {
        if(disconnector_)
        {
            disconnector_();
            disconnector_ = nullptr;
        }
    }

    void release()
    {
        disconnector_ = nullptr;
    }

private:
    std::function<void()> disconnector_;
};

// ============================================================================
// connection_group_t：管理多个连接
// ============================================================================
class connection_group_t
{
public:
    connection_group_t() {}

    template <typename... Args>
    void add(connection_t<Args...> c)
    {
        connections_.emplace_back(std::move(c));
    }

    template <typename... Args>
    connection_group_t &operator+=(connection_t<Args...> c)
    {
        add(std::move(c));
        return *this;
    }

    void disconnect_all()
    {
        connections_.clear(); // scoped_connection 析构时会自动 disconnect
    }

    std::size_t size() const
    {
        return connections_.size();
    }

    bool empty() const
    {
        return connections_.empty();
    }

private:
    std::vector<scoped_connection_t> connections_;
};

} // namespace xswl

#endif // XSWL_SIGNALS_H
