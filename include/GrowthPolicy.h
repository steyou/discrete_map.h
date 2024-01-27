#ifndef GROWTH_POLICY_H
#define GROWTH_POLICY_H

#include <cstddef>

template<class Derived>
class GrowthPolicy {
protected:
    using size_type = size_t;
public:
    //virtual constexpr is a C++20 feature

    /**
     * returns the next index from the output of a hash function and number of elements in table.
     *
     * Also known as the `indexer`.
     *
     * @arg raw_hash_val output of hash function
     * @arg capacity capacity of some internal vector containing key-value pairs.
     */
    size_type get_index(size_type capacity, size_type raw_hash_val) const noexcept {
        return static_cast<const Derived*>(this)->get_index_impl(capacity, raw_hash_val);
    }

    size_type next_capacity(size_type capacity) const noexcept {
        return static_cast<const Derived*>(this)->next_capacity_impl(capacity);
    }

    constexpr size_type min_capacity() const noexcept {
        return static_cast<const Derived*>(this)->min_capacity_impl();
    }

    constexpr size_type max_capacity() const noexcept {
        return static_cast<const Derived*>(this)->max_capacity_impl();
    }
};

#endif
