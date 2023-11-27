#ifndef GROWTH_POLICY_H
#define GROWTH_POLICY_H

#include <cstddef>

class GrowthPolicy {
public:
    //virtual constexpr is a C++20 feature
    template<class T>
    virtual T get_index(T raw_hash_val, T capacity) const noexcept = 0;

    template<class T>
    virtual T next_capacity(T capacity) const noexcept = 0;
    virtual constexpr size_t min_capacity() const noexcept = 0;
    virtual constexpr size_t max_capacity() const noexcept = 0;
};

#endif
