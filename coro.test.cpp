/* This file is part of Daedalus Turbo project: https://github.com/sierkov/daedalus-turbo/
 * Copyright (c) 2022-2023 Alex Sierkov (alex dot sierkov at gmail dot com)
 * Copyright (c) 2024-2025 R2 Rationality OÜ (info at r2rationality dot com)
 * This code is distributed under the license specified in:
 * https://github.com/sierkov/daedalus-turbo/blob/main/LICENSE */

#include <turbo/common/coro.hpp>
#include <turbo/common/test.hpp>

namespace {
    using namespace turbo;
    using namespace turbo::coro;

    generator_t<int> counter(const int max)
    {
        for (int i = 1; i <= max; ++i)
            co_yield i;
    }

    task_t<int> compute()
    {
        co_return 7 * 6;
    }

    task_t<std::string> greet()
    {
        co_return "hello, coroutine!";
    }

    task_t<int> fail()
    {
        throw std::runtime_error("error in coroutine");
        co_return 0; // unreachable but need to indicate a coroutine to compilers
    }
}

suite turbo_common_coro_suite = [] {
    "turbo::common::coro"_test = [] {
        "generator_t multiple co_yield"_test = [] {
            std::vector<int> v {};
            auto gen = counter(2);
            while (gen.resume()) {
                v.emplace_back(gen.take());
            }
            expect_equal(std::vector<int> { 1, 2 }, v);
        };

        "generator_t yields values in order"_test = [] {
            auto c = counter(3);

            expect_equal(true, c.resume());
            expect_equal(1, c.take());

            expect_equal(true, c.resume());
            expect_equal(2, c.take());

            expect_equal(true, c.resume());
            expect_equal(3, c.take());

            expect_equal(false, c.resume()); // No more values
        };

        "generator_t throws on empty take"_test = [] {
            auto c = counter(1);

            expect_equal(true, c.resume());
            expect_equal(1, c.take());

            // Now coroutine is done, take() without value should throw
            expect(throws([&]{ c.take(); }));
        };

        "task_t returns correct result"_test = [] {
            auto c = compute();
            expect_equal(42, c.resume());
        };

        "task_t works with std::string"_test = [] {
            auto c = greet();
            expect_equal("hello, coroutine!", c.resume());
        };

        "task_t propagates exception"_test = [] {
            auto c = fail();
            expect(throws<std::runtime_error>([&]{ c.resume(); }));
        };

        "task_t is movable"_test = [] {
            auto c1 = compute();
            task_t<int> c2 = std::move(c1);
            expect_equal(42, c2.resume());
        };

        "external_task_t"_test = [] {
            size_t coro_steps = 0;
            size_t num_resumes = 0;
            std::coroutine_handle<> active_handle {};

            auto my_coro = [&] -> task_t<void> {
                ++coro_steps;
                co_await external_task_t{active_handle};
                ++coro_steps;
                co_await external_task_t{active_handle};
                ++coro_steps;
                co_await external_task_t{active_handle};
                ++coro_steps;
                co_await external_task_t{active_handle};
                ++coro_steps;
                co_return;
            };

            auto c1 = my_coro();
            ++num_resumes;
            c1.resume();

            for (size_t i = 0; i < 4; ++i) {
                expect(!!active_handle);
                if (active_handle) {
                    ++num_resumes;
                    active_handle.resume();
                }
            }

            expect(c1.done());
            expect_equal(5ULL, coro_steps);
            expect_equal(5ULL, num_resumes);
        };
    };
};