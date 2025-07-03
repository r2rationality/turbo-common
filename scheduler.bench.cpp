/* Copyright (c) 2022-2023 Alex Sierkov (alex dot sierkov at gmail dot com)
 * Copyright (c) 2024-2025 R2 Rationality OÜ (info at r2rationality dot com) */

#include "scheduler.hpp"
#include <filesystem>
#include <turbo/common/benchmark.hpp>
#include <turbo/common/coro.hpp>
#include <turbo/common/zstd.hpp>
#ifdef _MSC_VER
#   include <SDKDDKVer.h>
#endif
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

namespace {
    using namespace turbo;

    template<std::random_access_iterator IT>
    void sum_batch(std::atomic<double> &total_time, const IT begin, const IT end) {
        const auto start_time = std::chrono::system_clock::now();
        double sum = 0.0;
        for (auto it = begin; it != end; ++it) {;
            const auto &val = *it;
            sum += std::sqrt(val * val);
        }
        total_time.fetch_add(
            std::chrono::duration<double> { std::chrono::system_clock::now() - start_time }.count() * 1000,
            std::memory_order_relaxed
        );
    }

    template<std::random_access_iterator IT>
    boost::asio::awaitable<void> sum_batch_boost_coro(std::atomic<double> &total_time, const IT begin, const IT end)
    {
        sum_batch(total_time, begin, end);
        co_return;
    }

    template<std::random_access_iterator IT>
    coro::task_t<void> sum_batch_coro(std::atomic<double> &total_time, const IT begin, const IT end)
    {
        sum_batch(total_time, begin, end);
        co_return;
    }
}

suite turbo_scheduler_bench_suite = [] {
    "turbo::scheduler"_test = [] {
        static const std::string DATA_DIR { "./data/immutable"s };
        auto &sched = scheduler::get();
        std::vector<double> tasks {};
        for (size_t i = 0; i < 1'000'000; ++i)
            tasks.emplace_back(static_cast<double>(i));
        ankerl::nanobench::Bench b {};
        b.title("turbo::scheduler")
            .output(&std::cerr)
            .performanceCounters(true)
            .relative(true);
        {
            size_t data_multiple = 20;
            std::vector<uint8_vector> chunks;
            uint8_vector buf;
            size_t total_size = 0;
            for (const auto &entry: std::filesystem::directory_iterator(DATA_DIR)) {
                if (entry.path().extension() != ".chunk") continue;
                file::read(entry.path().string(), buf);
                total_size += buf.size();
                chunks.push_back(buf);
            }
            b.batch(total_size);
            b.unit("byte");
            b.run("scheduler/default progress update", [&] {
                for (size_t i = 0; i < data_multiple; ++i) {
                    for (const auto &chunk: chunks) {
                        sched.submit(
                            "compress", 0,
                            [&chunk]() {
                                uint8_vector tmp;
                                zstd::compress(tmp, chunk, 3);
                            }
                        );
                    }
                }
                sched.process();
            });
        }
        b.unit("task");
        b.batch(tasks.size());
        for (size_t batch_size: { 10, 100, 1'000, 10'000 }) {
            b.run(fmt::format("nano tasks: scheduler - batch {}", batch_size), [&]() {
                std::atomic<double> total_time { 0.0 };
                for (size_t start = 0; start < tasks.size(); start += batch_size) {
                    auto end = std::min(start + batch_size, tasks.size());
                    sched.submit("math", 0, [&tasks, &total_time, start, end]() {
                        const auto start_time = std::chrono::system_clock::now();
                        double sum = 0.0;
                        for (size_t i = start; i < end; ++i) {
                            const auto &val = tasks[i];
                            sum += std::sqrt(val * val);
                        }
                        total_time.fetch_add(
                            std::chrono::duration<double> { std::chrono::system_clock::now() - start_time }.count() * 1000,
                            std::memory_order_relaxed
                        );
                    });
                }
                sched.process();
            });
        }
        for (size_t batch_size: { 10, 100, 1'000, 10'000 }) {
            b.run(fmt::format("nano tasks: scheduler coro - batch {}", batch_size), [&]() {
                std::atomic<double> total_time { 0.0 };
                for (size_t start = 0; start < tasks.size(); start += batch_size) {
                    auto end = std::min(start + batch_size, tasks.size());
                    auto coro = std::make_shared<coro::task_t<void>>(sum_batch_coro(total_time, tasks.begin() + start, tasks.begin() + end));
                    sched.submit("math", 0, [&tasks, &total_time, coro=std::move(coro)]() mutable {
                        coro->wait();
                    });
                }
                sched.process();
            });
        }
        {
            boost::asio::thread_pool tp {};
            for (size_t batch_size: { 10, 100, 1000, 10'000 }) {
                b.run(fmt::format("nano task: io_context: batch: {}", batch_size), [&]() {
                    boost::asio::thread_pool tp {};
                    std::atomic<double> total_time { 0.0 };
                    for (size_t start = 0; start < tasks.size(); start += batch_size) {
                        const auto end = std::min(start + batch_size, tasks.size());
                        boost::asio::post(tp, [&tasks, &total_time, start, end] {
                            const auto start_time = std::chrono::system_clock::now();
                            double sum = 0.0;
                            for (size_t i = start; i < end; ++i) {
                                const auto &val = tasks[i];
                                sum += std::sqrt(val * val);
                            }
                            total_time.fetch_add(
                                std::chrono::duration<double> { std::chrono::system_clock::now() - start_time }.count() * 1000,
                                std::memory_order_relaxed
                            );
                        });
                    }
                    tp.join();
                });
            }
        }
        {
            boost::asio::thread_pool tp {};
            for (size_t batch_size: { 10, 100, 1'000, 10'000 }) {
                b.run(fmt::format("nano task: io_context coro: batch: {}", batch_size), [&]() {
                    boost::asio::thread_pool tp {};
                    std::atomic<double> total_time { 0.0 };
                    for (size_t start = 0; start < tasks.size(); start += batch_size) {
                        const auto end = std::min(start + batch_size, tasks.size());
                        boost::asio::co_spawn(
                            tp,
                            sum_batch_boost_coro(total_time, tasks.begin() + start, tasks.begin() + end),
                            boost::asio::detached
                        );
                    }
                    tp.join();
                });
            }
        }
    };
};