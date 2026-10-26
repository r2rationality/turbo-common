/* Copyright (c) 2022-2023 Alex Sierkov (alex dot sierkov at gmail dot com)
 * Copyright (c) 2024-2025 R2 Rationality OÜ (info at r2rationality dot com) */

#include "benchmark.hpp"

namespace {
    using namespace turbo;
}

suite common_error_bench_suite = [] {
    "common::error"_test = [] {
        ankerl::nanobench::Bench b {};
        b.title("common::error")
            .output(&std::cerr)
            .unit("exception")
            .performanceCounters(true)
            .relative(true);
        {
            b.run("construct formatted",[&] {
                ankerl::nanobench::doNotOptimizeAway(error{"Hello {}!", "world"});
            });
            b.run("construct, throw, and catch",[&] {
                try {
                    throw error{"Hello {}!", "world"};
                } catch (const error &err) {
                    ankerl::nanobench::doNotOptimizeAway(err);
                }
            });
        }
    };
};
