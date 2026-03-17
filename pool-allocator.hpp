#pragma once
/* Copyright (c) 2022-2023 Alex Sierkov (alex dot sierkov at gmail dot com)
 * Copyright (c) 2024-2026 R2 Rationality OÜ (info at r2rationality dot com) */

#include <forward_list>
#include <memory>
#include <vector>

namespace turbo {
    template<typename T, size_t BATCH_SZ = 0x1000, bool SKIP_DTOR = std::is_trivially_destructible_v<T>>
    struct pool_allocator_t {
        static_assert(SKIP_DTOR, "pool_allocator_t does not call T's destructor on pool destruction; "
            "set SKIP_DTOR=true to acknowledge this if T only owns resources within the same pool");

        pool_allocator_t() = default;
        pool_allocator_t(const pool_allocator_t &) = delete;
        pool_allocator_t(pool_allocator_t &&) = delete;
        pool_allocator_t &operator=(const pool_allocator_t &) = delete;
        pool_allocator_t &operator=(pool_allocator_t &&) = delete;

        struct deleter_t {
            pool_allocator_t *_alloc = nullptr;

            void operator()(T* ptr) const
            {
                if (_alloc)
                    _alloc->deallocate(ptr);
            }
        };

        using ptr_t = std::unique_ptr<T, deleter_t>;

        T* allocate()
        {
            if (!_free.empty()) {
                auto ptr = _free.front();
                _free.pop_front();
                return ptr;
            }
            if (_arenas.empty() || _arena_offset == BATCH_SZ)
                _add_arena();
            std::byte* arena = static_cast<std::byte*>(_arenas.back());
            return reinterpret_cast<T*>(arena + (_arena_offset++) * sizeof(T));
        }

        void deallocate(T* ptr)
        {
            if (ptr)
                _free.push_front(ptr);
        }

        ~pool_allocator_t()
        {
            for (auto a: _arenas)
                operator delete(a, std::align_val_t{alignof(T)});
        }

        template<typename... Args>
        ptr_t make_ptr(Args&&... args)
        {
            T* raw = allocate();
            try {
                new (raw) T { std::forward<Args>(args)... };
            } catch (...) {
                deallocate(raw);
                throw;
            }
            return { raw, _deleter };
        }

        size_t free_count() const {
            return std::distance(_free.begin(), _free.end());
        }
    private:
        std::vector<void *> _arenas {};
        std::forward_list<T *> _free {};
        size_t _arena_offset = 0;
        deleter_t _deleter { this };

        void _add_arena()
        {
            _arenas.push_back(operator new(BATCH_SZ * sizeof(T), std::align_val_t{alignof(T)}));
            _arena_offset = 0;
        }
    };
}
