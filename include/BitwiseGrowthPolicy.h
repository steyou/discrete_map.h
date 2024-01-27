#ifndef BITWISE_POLICY_H
#define BITWISE_POLICY_H

#include <cstddef>
#include "GrowthPolicy.h"

class BitwiseGrowthPolicy : public GrowthPolicy<BitwiseGrowthPolicy> {
public:
    size_type get_index_impl(size_type capacity, size_type raw_hash_val) const noexcept {
        return raw_hash_val & (capacity - 1);
    } 

    size_type next_capacity_impl(size_type capacity) const noexcept {
        return capacity << 1;
    }

    constexpr size_type min_capacity_impl() const noexcept {
        return 8u;
    }

    constexpr size_type max_capacity_impl() const noexcept {
        unsigned int value = min_capacity_impl();
        unsigned int mask = 1u << (sizeof(unsigned int)*8 - 1);
    	while ((value & mask) == 0) {
            value <<= 1;
        }
        return value;
    }
};

#endif
