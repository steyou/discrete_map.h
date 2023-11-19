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

    constexpr size_t min_capacity() const noexcept override {
        return 8u;
    }

    constexpr float threshold() const noexcept override {
        return 0.8f;
    }
};

#endif
