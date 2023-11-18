#ifndef GROWTH_POLICY_H
#define GROWTH_POLICY_H

#include <cstddef>

template <typename THRESHOLD_TYPE=float>
class MapPolicy {
public:
    //virtual constexpr is a C++20 feature
    virtual size_t get_index(size_t raw_hash_val, size_t capacity) const = 0;
    virtual size_t next_capacity(size_t capacity) const = 0;
    virtual constexpr size_t min_capacity() const = 0;
    virtual constexpr THRESHOLD_TYPE load_factor() const = 0;
}

#endif
