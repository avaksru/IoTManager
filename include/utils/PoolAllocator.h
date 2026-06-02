#pragma once
#include <cstddef>
#include <vector>
/**
 * Simple Fixed-Size Block Pool Allocator
 * Designed to reduce fragmentation for frequently created/destroyed objects of the same size.
 */
template <typename T, size_t BlockCount = 64>
class PoolAllocator {
public:
    PoolAllocator() {
        pool_.resize(BlockCount);
        for (size_t i = 0; i < BlockCount; ++i) {
            freeList_.push_back(&pool_[i]);
        }
    }

    ~PoolAllocator() = default;

    T* allocate() {
        if (freeList_.empty()) {
            return nullptr; // Pool exhausted
        }
        T* ptr = freeList_.back();
        freeList_.pop_back();
        return ptr;
    }

    void deallocate(T* ptr) {
        if (!ptr) return;
        freeList_.push_back(ptr);
    }

    // Placement new wrapper
    template <typename... Args>
    T* construct(Args&&... args) {
        T* ptr = allocate();
        if (!ptr) return nullptr;
        return new (ptr) T(std::forward<Args>(args)...);
    }

    // Destruction wrapper
    void destroy(T* ptr) {
        if (!ptr) return;
        ptr->~T();
        deallocate(ptr);
    }

private:
    std::vector<T> pool_;
    std::vector<T*> freeList_;
};
