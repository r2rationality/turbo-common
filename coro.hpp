#pragma once
/* This file is part of Daedalus Turbo project: https://github.com/sierkov/daedalus-turbo/
 * Copyright (c) 2022-2023 Alex Sierkov (alex dot sierkov at gmail dot com)
 * Copyright (c) 2024-2025 R2 Rationality OÜ (info at r2rationality dot com)
 * This code is distributed under the license specified in:
 * https://github.com/sierkov/daedalus-turbo/blob/main/LICENSE */

#include <atomic>
#include <coroutine>
#include <exception>
#include <future>
#include <mutex>
#include <optional>
#include <utility>
#include <spdlog/logger.h>
#include "error.hpp"
#include "logger.hpp"
#include "scheduler.hpp"

namespace turbo::coro {
    template<typename T>
    struct generator_t {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        struct promise_type {
            generator_t get_return_object()
            {
                return generator_t { handle_type::from_promise(*this) };
            }

            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            std::suspend_always final_suspend() noexcept
            {
                return {};
            }

            std::suspend_always yield_value(T value)
            {
                _current_value = std::move(value);
                return {};
            }

            void return_void()
            {
            }

            void unhandled_exception()
            {
                std::terminate();
            }
        private:
            friend generator_t;
            std::optional<T> _current_value {};
        };

        generator_t(generator_t&& t) noexcept:
            _coro { std::exchange(t._coro, {}) }
        {
        }

        generator_t& operator=(generator_t&& t) noexcept
        {
            if (this != &t) [[likely]] {
                if (_coro) [[likely]]
                    _coro.destroy();
                _coro = std::exchange(t._coro, {});
            }
            return *this;
        }

        ~generator_t()
        {
            if (_coro) [[likely]]
                _coro.destroy();
        }

        bool resume()
        {
            if (!_coro || _coro.done())
                return false;
            _coro.resume();
            return !_coro.done();
        }

        T result()
        {
            if (!_coro) [[unlikely]]
                throw error("an attempt to call take on an empty coro::generator_t!");
            auto &val = _coro.promise()._current_value;
            if (!val) [[unlikely]]
                throw error("an attempt to take from an empty promise!");
            auto res = std::move(*val);
            val.reset();
            return res;
        }
    private:
        handle_type _coro;

        explicit generator_t(handle_type h):
            _coro { h }
        {
        }
    };

    template <typename T>
    struct result_storage_t {
        void set_value(T&& v) { _value = std::move(v); }
        T get() {
            if (!_value) [[unlikely]]
                throw error("get called on a coroutine result storage before it was set!");
            return std::move(*_value);
        }
    protected:
        std::optional<T> _value;
    };

    template <>
    struct result_storage_t<void> {
        void set_value() {}
        void get() {}
    };

    template <typename T>
    struct promise_base_t : result_storage_t<T> {
        void return_value(T v) { this->set_value(std::move(v)); }
    };

    template <>
    struct promise_base_t<void> : result_storage_t<void> {
        void return_void() { this->set_value(); }
    };

    template<typename T>
    struct task_t {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        struct promise_type: promise_base_t<T> {
            task_t get_return_object() { return task_t{_my_handle()}; }
            std::suspend_always initial_suspend() noexcept { return  {}; }
            std::suspend_always final_suspend() noexcept
            {
                auto mh = _my_handle();
                auto ch = _caller;
                logger::info("task_t<{}>::final_suspend begin handle: {} caller: {}", typeid(T).name(), mh.address(), ch.address());
                scheduler::get().submit("final-suspend", 100, [mh, ch] {
                    logger::info("task_t<{}>::final_suspend processing started handle: {} caller: {}", typeid(T).name(), mh.address(), ch.address());
                    if (ch && !ch.done())
                        ch.resume();
                    logger::info("task_t<{}>::final_suspend processed handle: {} caller: {}", typeid(T).name(), mh.address(), ch.address());
                });
                return {};
            }
            void unhandled_exception() { _exception = std::current_exception(); }
        private:
            handle_type _my_handle()
            {
                return handle_type::from_promise(*this);
            }

            friend task_t;
            std::exception_ptr _exception {};
            std::coroutine_handle<> _caller {};
        };

        task_t(task_t &&o) noexcept:
            _coro{std::exchange(o._coro, {})}
        {
            _log();
        }

        task_t& operator=(task_t &&o) noexcept
        {
            _log();
            if (this != &o) {
                if (_coro)
                    _coro.destroy();
                _coro = std::exchange(o._coro, {});
            }
            return *this;
        }

        ~task_t()
        {
            _log();
            if (_coro)
                _coro.destroy();
        }

        bool await_ready() const noexcept
        {
            _log();
            return done();
        }

        void await_suspend(std::coroutine_handle<> h)
        {
            _log("-begin");
            _coro.promise()._caller = h;
            resume();
            _log("-complete");
        }

        T await_resume() noexcept
        {
            _log();
            return result();
        }

        [[nodiscard]] bool done() const
        {
            _log();
            return _coro && _coro.done();
        }

        void resume()
        {
            _log();
            if (!_coro.done())
                _coro.resume();
        }

        T result()
        {
            _log();
            auto &p = _coro.promise();
            if (p._exception)
                std::rethrow_exception(p._exception);
            return p.get();
        }

        T wait()
        {
            _log();
            std::promise<T> promise;
            auto future = promise.get_future();
            auto wrapped_task = _notify_future(std::move(promise), *this);
            wrapped_task.resume();
            future.wait();
            return future.get();
        }
    private:
        void _log(std::optional<std::string> desc={}, const std::source_location &loc=std::source_location::current()) const
        {
            logger::info("{}{}: this: {} waiter: {} done: {}", loc.function_name(), desc.value_or(""), static_cast<const void *>(this), _coro.address(), _coro && _coro.done());
        }

        task_t<void> _notify_future(std::promise<T> promise, task_t<T> &task)
        {
            _log("-start");
            auto res = co_await task;
            _log("-complete");
            promise.set_value(std::move(res));
            _log("-set-value");
        }

        explicit task_t(handle_type h):
            _coro{h}
        {
            _log("-set-value");
        }

        handle_type _coro;
    };

    struct external_task_t {
        using opt_action_t = std::optional<std::function<void()>>;

        external_task_t(std::coroutine_handle<> &waiter, opt_action_t suspend_action={}):
            _waiter{waiter}, _suspend_action{std::move(suspend_action)}
        {
            _log();
        }

        ~external_task_t()
        {
            _log();
        }

        bool await_ready() const noexcept
        {
            _log();
            return false;
        }

        void await_suspend(std::coroutine_handle<> h)
        {
            _log("-start");
            _waiter = h;
            if (_suspend_action)
                _suspend_action->operator()();
            _log("-complete");
        }

        void await_resume() noexcept
        {
            _log();
            _waiter = {};
        }

    private:
        std::coroutine_handle<> &_waiter;
        opt_action_t _suspend_action;

        void _log(std::optional<std::string> desc={}, const std::source_location &loc=std::source_location::current()) const
        {
            logger::info("{}{}: {} waiter: {} done: {}", loc.function_name(), desc.value_or(""), static_cast<const void *>(this), _waiter.address(), _waiter && _waiter.done());
        }
    };
}
