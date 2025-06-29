#pragma once
/* This file is part of Daedalus Turbo project: https://github.com/sierkov/daedalus-turbo/
 * Copyright (c) 2022-2023 Alex Sierkov (alex dot sierkov at gmail dot com)
 * Copyright (c) 2024-2025 R2 Rationality OÜ (info at r2rationality dot com)
 * This code is distributed under the license specified in:
 * https://github.com/sierkov/daedalus-turbo/blob/main/LICENSE */

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>
#include <vector>
#include <turbo/common/error.hpp>

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

        T take()
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

    template<typename T>
    struct task_t {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        struct promise_type {
            task_t get_return_object() { return task_t{handle_type::from_promise(*this)}; }
            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void unhandled_exception() { _exception = std::current_exception(); }
            void return_value(T v) { _value = std::move(v); }
        private:
            friend task_t;
            std::optional<T> _value;
            std::exception_ptr _exception;
        };

        task_t(task_t&& o) noexcept:
            _coro{std::exchange(o._coro, {})}
        {
        }

        task_t& operator=(task_t&& o) noexcept
        {
            if (this != &o) {
                if (_coro)
                    _coro.destroy();
                _coro = std::exchange(o._coro, {});
            }
            return *this;
        }

        ~task_t()
        {
            if (_coro)
                _coro.destroy();
        }

        T result()
        {
            if (!_coro) [[unlikely]]
                throw error("an attempt to get result from an empty coro::task_t!");
            if (!_coro.done())
                _coro(); // Start and complete coroutine if needed
            if (_coro.promise()._exception)
                std::rethrow_exception(_coro.promise()._exception);
            return std::move(*_coro.promise()._value);
        }
    private:
        explicit task_t(handle_type h):
            _coro{h}
        {
        }

        handle_type _coro;
    };

    template<>
    struct task_t<void> {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        struct promise_type {
            task_t get_return_object() { return task_t{handle_type::from_promise(*this)}; }
            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void unhandled_exception() { _exception = std::current_exception(); }
            void return_void() noexcept {}
        private:
            friend task_t;
            std::exception_ptr _exception;
        };

        task_t(task_t&& o) noexcept:
            _coro{std::exchange(o._coro, {})}
        {
        }

        task_t& operator=(task_t&& o) noexcept
        {
            if (this != &o) {
                if (_coro)
                    _coro.destroy();
                _coro = std::exchange(o._coro, {});
            }
            return *this;
        }

        ~task_t()
        {
            if (_coro)
                _coro.destroy();
        }

        void wait()
        {
            //if (!_coro) [[unlikely]]
            //    throw error("an attempt to get result from an empty coro::task_t!");
            if (!_coro.done())
                _coro(); // Start and complete coroutine if needed
            if (_coro.promise()._exception)
                std::rethrow_exception(_coro.promise()._exception);
        }
    private:
        explicit task_t(handle_type h):
            _coro{h}
        {
        }

        handle_type _coro;
    };
}