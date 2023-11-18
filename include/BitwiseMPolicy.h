#ifndef BITWISE_POLICY_H
#define BITWISE_POLICY_H

#include <cstddef>
#include "MapPolicy.h"

struct BitwiseMPolicy : public MapPolicy<float> {
    size_t get_index(size_t raw_hash_val, size_t capacity) const override noexcept {
        return raw_hash_val & (capacity - 1);
    } 

    constexpr size_t min_capacity() const override noexcept {
        return 8u;
    }

    size_t next_capacity(size_t capacity) const override noexcept {
	return capacity << 1;
    }

    constexpr float threshold() const override noexcept {
        return 0.8f;
    }
}

#endif
