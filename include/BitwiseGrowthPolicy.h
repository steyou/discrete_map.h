#ifndef BITWISE_POLICY_H
#define BITWISE_POLICY_H

#include <cstddef>
#include "GrowthPolicy.h"

struct BitwiseGrowthPolicy : public GrowthPolicy {
    template<class T>
    T get_index(T raw_hash_val, T capacity) const noexcept override {
        return raw_hash_val & (capacity - 1);
    } 

    template<class T>
    T next_capacity(T capacity) const noexcept override {
        return capacity << 1;
    }

    constexpr size_t min_capacity() const noexcept override {
        return 8u;
    }

    constexpr size_t max_capacity() const noexcept override {
        unsigned int value = min_capacity();
        unsigned int mask = 1u << (sizeof(unsigned int)*8 - 1);
    	while ((value & mask) == 0) {
            value <<= 1;
        }
        return value;
    }
};

#endif
