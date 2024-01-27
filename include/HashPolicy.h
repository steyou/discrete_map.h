#ifndef HASH_POLICY_H
#define HASH_POLICY_H

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

template<template <class> class Derived,
         class SizeTraits>
class HashPolicy {
    private:
        using derived = Derived<SizeTraits>;
        using size_type = typename SizeTraits::size_type;
        using indices_type = typename SizeTraits::indices_type;

        using derived_iterator = typename derived::iterator;
        using derived_const_iterator = typename derived::const_iterator;

        derived _derived;

        std::vector<indices_type> _indices;

    public:

        HashPolicy(size_type initial_capacity)
            : _indices(initial_capacity, std::nullopt)
        {}

        ~HashPolicy() = default;

        size_type size() const noexcept {
            return _indices.size();
        }

        void clear() noexcept {
            //TODO document to be careful about calling this function. you can lead to "dangling pairs".
            _indices.clear();
        }

        [[nodiscard]] float load_factor(size_type num_elements) const noexcept {
            return static_cast<float>(num_elements) / static_cast<float>(_indices.size());
        }

        constexpr float threshold() const noexcept {
            return _derived.threshold();
        }

        template<class Callable>
        void rehash(size_type next_size, Callable indexer) {

            // rehashing downwards not supported
            if (next_size <= _indices.size()) {
                return;
            }

            //define a new array to store the existing indices
            std::vector<indices_type> the_bigger_probe(next_size, std::nullopt);

            // probe through the existing indices and relocate them according to the hash function.
            for (derived_const_iterator it = _derived.cbegin(_indices); it != _derived.cend(_indices) + (-1); ++it) {

                const indices_type& deref_index = *it;

                if (deref_index.has_value()) {

                    // this loop represents collision resolution if we try to hehash some element into a non-empty slot.
                    for (int offset = 0; true; ++offset) {

                        size_type new_index = indexer(deref_index.value()) + offset;

                        if (!the_bigger_probe[new_index].has_value()) {
                            the_bigger_probe[new_index] = deref_index.value();
                            break;
                        }
                    }
                }
            }

            _indices = the_bigger_probe;
        }

        template<class Callable>
        const indices_type& probe(const size_type hash_result, Callable&& stop_condition, bool stop_empty=true) const {
            //loop until some condition happens in the callback.
            for (derived_const_iterator it = _derived.cbegin(_indices) + hash_result; it != _derived.cend(_indices); ++it) {

                const indices_type& index = *it;

                if (index.has_value()) {
                    if (stop_condition(index.value())) {
                        return index;
                    }
                    //if false, the callback indicated to continue probing. We don't && the two if statements because we logically want a 'do nothing' branch when A(!B).
                }
                else if (stop_empty) {
                    //empty slot. under simple open addressing we stop here.
                    return index;
                }
            }
            throw std::runtime_error("probe loop completed and no action taken");
        }

        //mutable version
        template<class Callable>
        indices_type& probe(const size_type hash_result, Callable&& stop_condition, bool stop_empty=true) {
            //loop until some condition happens in the callback.
            for (derived_iterator it = _derived.begin(_indices) + hash_result; it != _derived.end(_indices); ++it) {

                indices_type& index = *it;

                if (index.has_value()) {
                    if (stop_condition(index.value())) {
                        return index;
                    }
                    //if false, the callback indicated to continue probing. We don't && the two if statements because we logically want a 'do nothing' branch when A(!B).
                }
                else if (stop_empty) {
                    //empty slot. under simple open addressing we stop here.
                    return index;
                }
            }
            throw std::runtime_error("probe loop completed and no action taken");
        }

};

#endif
