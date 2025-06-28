/* This file is part of Daedalus Turbo project: https://github.com/sierkov/daedalus-turbo/
 * Copyright (c) 2022-2023 Alex Sierkov (alex dot sierkov at gmail dot com)
 * Copyright (c) 2024-2025 R2 Rationality OÜ (info at r2rationality dot com)
 * This code is distributed under the license specified in:
 * https://github.com/sierkov/daedalus-turbo/blob/main/LICENSE */

#include "bytes.hpp"
#include "test.hpp"

namespace {
    using namespace turbo;
}

suite turbo_common_bytes_suite = [] {
    "turbo::common::bytes"_test = [] {
        "do not initialize"_test = [] {
            // fill the stack with non-zero values
            {
                const byte_array<4> tmp { 5, 4, 3, 2 };
            }
            byte_array<4> a;
            expect_equal(4, a.size());
            for (const auto &v: a)
                expect(v != 0);
            ankerl::nanobench::doNotOptimizeAway(a);
        };
        "initialize with zeros"_test = [] {
            byte_array<4> a {};
            expect(a.size() == 4);
            for (auto v: a)
                expect(v == 0) << v;
        };
        "initialize with values"_test = [] {
            const byte_array<4> a { 1, 2, 3, 4 };
            expect(a.size() == 4);
            expect_equal(1, a[0]);
            expect_equal(2, a[1]);
            expect_equal(3, a[2]);
            expect_equal(4, a[3]);
        };
        "construct from a span"_test = [] {
            const byte_array<4> b { 9, 8, 7, 6 };
            const byte_array<4> c { std::span(b) };
            expect_equal(b, c);
        };
        "construct from a string_view"_test = [] {
            using namespace std::literals;
            byte_array<4> a { "\x01\x02\x03\x04"sv };
            expect(a.size() == 4);
            expect_equal(1, a[0]);
            expect_equal(2, a[1]);
            expect_equal(3, a[2]);
            expect_equal(4, a[3]);
        };
        "construct from hex"_test = [] {
            using namespace std::literals;
            const auto a = byte_array<4>::from_hex("01020304");
            expect(a.size() == 4);
            expect_equal(1, a[0]);
            expect_equal(2, a[1]);
            expect_equal(3, a[2]);
            expect_equal(4, a[3]);
        };
        "assign_span"_test = [] {
            byte_array<4> a { 1, 2, 3, 4 };
            byte_array<4> b { 9, 8, 7, 6 };
            expect(a.size() == 4);
            expect(a[0] == 1);
            expect(a[1] == 2);
            expect(a[2] == 3);
            expect(a[3] == 4);
            a = std::span(b);
            expect(a[0] == 9);
            expect(a[1] == 8);
            expect(a[2] == 7);
            expect(a[3] == 6);
        };
        "assign_string_view"_test = [] {
            using namespace std::literals;
            byte_array<4> a {};
            expect(a.size() == 4);
            for (const auto v: a) expect(v == 0);
            a = "\x01\x02\x03\x04"sv;
            expect(a[0] == 1);
            expect(a[1] == 2);
            expect(a[2] == 3);
            expect(a[3] == 4);
        };
        "string formatting support"_test = [] {
            auto data = byte_array<4>::from_hex("f0e1d2c3");
            expect(fmt::format("{}", data) == "F0E1D2C3");
        };
        "secure_array"_test = [] {
            const auto empty = byte_array<4>::from_hex("00000000");
            const auto filled = byte_array<4>::from_hex("DEADBEAF");
            const uint8_t *data_ptr;
            {
                const secure_byte_array<4> sec { filled };
                data_ptr = sec.data();
                expect_equal(true, memcmp(sec.data(), filled.data(), filled.size()) == 0);
            }
            expect_equal(false, memcmp(data_ptr, filled.data(), filled.size()) == 0);
            expect_equal(true, memcmp(data_ptr, empty.data(), empty.size()) == 0);
        };
    };  
};