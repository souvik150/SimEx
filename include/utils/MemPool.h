#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <utility>

template <typename T, std::size_t ChunkSize = 1024>
class MemPool {
public:
    MemPool() = default;

    MemPool(const MemPool&) = delete;
    MemPool& operator=(const MemPool&) = delete;
    MemPool(MemPool&&) = delete;
    MemPool& operator=(MemPool&&) = delete;

    ~MemPool() = default;

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (free_list_.empty())
            addChunk();

        T* slot = free_list_.back();
        free_list_.pop_back();
        return new (slot) T(std::forward<Args>(args)...);
    }

    void deallocate(T* object) {
        if (!object)
            return;
        object->~T();
        free_list_.push_back(object);
    }

private:
    void addChunk() {
        auto chunk = std::make_unique<std::uint8_t[]>(ChunkSize * sizeof(T));
        T* ptr = reinterpret_cast<T*>(chunk.get());
        for (std::size_t i = 0; i < ChunkSize; ++i)
            free_list_.push_back(ptr + i);
        chunks_.push_back(std::move(chunk));
    }

    std::vector<std::unique_ptr<std::uint8_t[]>> chunks_;
    std::vector<T*> free_list_;
};
