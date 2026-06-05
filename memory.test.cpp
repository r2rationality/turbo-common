/* Copyright (c) 2022-2023 Alex Sierkov (alex dot sierkov at gmail dot com)
 * Copyright (c) 2024-2025 R2 Rationality OÜ (info at r2rationality dot com) */

#include <turbo/common/test.hpp>
#include <vector>
#include "memory.hpp"

using namespace turbo;

namespace {
    void touch_pages(uint8_vector &data)
    {
        static constexpr size_t page_size = 4096;
        for (size_t off = 0; off < data.size(); off += page_size) {
            volatile auto *p = data.data() + off;
            *p = static_cast<uint8_t>(off);
        }
        if (!data.empty()) {
            volatile auto *p = data.data() + data.size() - 1;
            *p = static_cast<uint8_t>(data.size());
        }
    }
}

suite turbo_common_memory_suite = [] {
    "turbo::common::memory"_test = [] {
        const auto before = memory::my_usage_mb();

        static constexpr size_t warm_chunk_mb = 32;
        static constexpr size_t min_warm_delta_mb = 16;
        static constexpr size_t max_warm_chunks = 16;
        std::vector<uint8_vector> warm_blocks {};
        warm_blocks.reserve(max_warm_chunks);
        size_t warmed = before;
        for (size_t i = 0; i < max_warm_chunks && warmed < before + min_warm_delta_mb; ++i) {
            warm_blocks.emplace_back(warm_chunk_mb << 20);
            touch_pages(warm_blocks.back());
            warmed = memory::my_usage_mb();
        }
        expect(warmed >= before + min_warm_delta_mb) << warmed << before;

        static constexpr size_t measured_mb = 128;
        static constexpr size_t tolerance_mb = 16;
        uint8_vector measured(measured_mb << 20);
        touch_pages(measured);
        const auto after_alloc = memory::my_usage_mb();
        const auto delta = after_alloc > warmed ? after_alloc - warmed : 0;
        expect(delta + tolerance_mb >= measured_mb) << after_alloc << warmed << delta;
        // Some standard libraries do not immediately return memory to the OS, so do not check release.
    };
};
