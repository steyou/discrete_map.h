#ifndef MAP_POLICY_H
#define MAP_POLICY_H

#include <cstddef>

class MapPolicy {
public:
    //virtual constexpr is a C++20 feature
    virtual size_t get_index(size_t raw_hash_val, size_t capacity) const noexcept = 0;

    virtual size_t next_capacity(size_t capacity) const noexcept = 0;
    virtual size_t next_capacity(size_t capacity, size_t proposed) const noexcept = 0;

    virtual constexpr size_t min_capacity() const noexcept = 0;
    virtual constexpr size_t max_capacity() const noexcept = 0;
    virtual constexpr float threshold() const noexcept = 0;
};

#endif
