#pragma once
/* Copyright (c) 2022-2023 Alex Sierkov (alex dot sierkov at gmail dot com)
 * Copyright (c) 2024-2025 R2 Rationality OÜ (info at r2rationality dot com) */

#include <turbo/common/error.hpp>
#include <variant>

namespace turbo::variant {
    template<typename TO, typename FROM>
    TO &get_nice(FROM &v)
    {
        return std::visit([&](auto &vo) -> TO & {
            using T = decltype(vo);
            if constexpr (std::is_same_v<std::decay_t<T>, std::decay_t<TO>>) {
                return vo;
            } else {
                throw error(fmt::format("expected type {} but got {}", typeid(TO).name(), typeid(T).name()));
            }
        }, v);
    }

    template<typename TO, typename FROM>
    const TO &get_nice(const FROM &v)
    {
        return std::visit([&](const auto &vo) -> const TO & {
            using T = decltype(vo);
            if constexpr (std::is_same_v<std::decay_t<T>, std::decay_t<TO>>) {
                return vo;
            } else {
                throw error(fmt::format("expected type {} but got {}", typeid(TO).name(), typeid(T).name()));
            }
        }, v);
    }
}