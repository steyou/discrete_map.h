#ifndef BITWISE_POLICY_H
#define BITWISE_POLICY_H

#include <cstddef>
#include "MapPolicy.h"

struct BitwiseMapPolicy : public MapPolicy {
    size_t get_index(size_t raw_hash_val, size_t capacity) const noexcept override {
        return raw_hash_val & (capacity - 1);
    } 

    size_t next_capacity(size_t capacity) const noexcept override {
        return capacity << 1;
    }

    /**
     * returns the next capacity if proposed capacity exceeds the current one.
     */
    size_t next_capacity(size_t capacity, size_t proposed) const noexcept override {
        if (proposed > capacity) { //this guard prevents unnecessary resizes.
            return next_capacity(proposed);
        }
        return capacity;
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

    constexpr float threshold() const noexcept override {
        return 0.8f;
    }
};

#endif
