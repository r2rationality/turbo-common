/* Copyright (c) 2024-2025 R2 Rationality OÜ (info at r2rationality dot com) */

#include "test.hpp"
#include "serializable.hpp"

namespace {
    using namespace turbo;
    using namespace turbo::codec;

    struct point_t {
        uint32_t x = 0;
        uint32_t y = 0;

        void serialize(auto &archive)
        {
            archive.process("x", x);
            archive.process("y", y);
        }
    };

    struct plain_t {
        int val = 0;
    };

    struct line_t {
        point_t a{};
        point_t b{};

        void serialize(auto &archive)
        {
            archive.process("a", a);
            archive.process("b", b);
        }
    };
}

suite turbo_common_serializable_suite = [] {
    "turbo::common::serializable"_test = [] {
        "concepts"_test = [] {
            expect(serializable_c<point_t>);
            expect(!serializable_c<plain_t>);
            expect(!serializable_c<uint32_t>);
            expect(not_serializable_c<plain_t>);
        };
        "formatter::scalar"_test = [] {
            std::string out{};
            formatter frmtr{ std::back_inserter(out) };
            frmtr.format(uint32_t{42});
            expect_equal(std::string{"42"}, out);
        };
        "formatter::serializable"_test = [] {
            point_t p{3, 7};
            const auto out = fmt::format("{}", p);
            expect(out.find("x: 3") != std::string::npos) << out;
            expect(out.find("y: 7") != std::string::npos) << out;
        };
        "formatter::nested"_test = [] {
            line_t l{{ 1, 2 }, { 3, 4 }};
            const auto out = fmt::format("{}", l);
            expect(out.find("a:") != std::string::npos) << out;
            expect(out.find("b:") != std::string::npos) << out;
            expect(out.find("x: 1") != std::string::npos) << out;
            expect(out.find("x: 3") != std::string::npos) << out;
        };
    };
};
