#ifndef DISCRETE_MAP_H
#define DISCRETE_MAP_H

//see https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/n4950.pdf
//§ 24.5.4.1

#include <system_error>
#include <expected>
#include <iterator>
#include <concepts>
#include <vector>
#include <cstddef>
#include <optional>
#include <memory>
#include <functional>

#include "BitwiseMapPolicy.h"
#include "MapPolicy.h"

template <typename Iterator>
concept BidirectionalIterator = requires (Iterator it) {
    { it++ } -> std::same_as<Iterator&>; // forward increment
    { it-- } -> std::same_as<Iterator&>; // backward increment
    { *it } ->  std::same_as<std::iterator_traits<Iterator>::reference>; // dereference
};


template<class Key,
         class T,
         class Hash = std::hash<Key>,
         class Pred = std::equal_to<Key>,
         class P = BitwiseMapPolicy
         class KeyAllocator = std::allocator<const Key>,
         class ValueAllocator = std::allocator<T>>
class discrete_map {
    private:
        std::unique_ptr<MapPolicy> _policy;

        //This vector acts as a container of indices, inspired by unordered_map buckets. It's used to store the shared index of a key-value pair, determined at insertion after the hash function derives the 'bucket'. Unlike the existing STL maps, this vector is what abstracts away probing, resizing, and collision management, keeping these operations separate from the key-value pairs. That allows the kv-pairs can be public const ref& exposed with little risk.
        std::vector<std::optional<size_t>> _index_probe;

        std::vector<const Key, KeyAllocator> _keys;
        std::vector<T, ValueAllocator> _values;

        //methods

        size_t _key2index(const Key& k, const size_t capacity) const noexcept {
            size_t hash_raw = hash_function()(k);
            return _policy->get_index(hash_raw, capacity);
        } 

        bool _circular_traversal(const size_t start_index, std::function<bool(const size_t)> callback) const {
            for (size_t i = start_index; i < _index_probe.size(); i++) {
                if (callback(i)) return true;
            }
            //if start_index != 0 then we must circle to the beginning if end reached
            for (size_t i = 0; i < start_index; i++) {
                if (callback(i)) return true;
            }
            return false; //the behavior of whatever we're calling did not happen. whether that's good or bad is not this function's responsibility.
        }

        template<class F>
        F _circular_traversal(const size_t start_index, std::function<bool(const size_t, F&)> callback) const {
            F result;
            for (size_t i = start_index; i < _index_probe.size(); i++) {
                if (callback(i, result)) return result;
            }
            //if start_index != 0 then we must circle to the beginning if end reached
            for (size_t i = 0; i < start_index; i++) {
                if (callback(i, result)) return result;
            }
            return result; //It is assumed that the lambda function returns a F value even when the function returns false. On the last iteration, we return that value.
        }

        void _grow_next() {
            size_t new_size = _policy->next_capacity(_index_probe.size());
            rehash(new_size);
        }

    public:
        // types
        using key_type = Key;
        using mapped_type = T;
        //using value_type = pair<const Key, T>;
        using hasher = Hash;
        using key_equal = Pred;

        using key_allocator_type = KeyAllocator;
        using key_pointer = typename allocator_traits<KeyAllocator>::pointer;
        using key_const_pointer = typename allocator_traits<KeyAllocator>::const_pointer;

        using value_allocator_type = ValueAllocator;
        using value_pointer = typename allocator_traits<ValueAllocator>::pointer;
        using value_const_pointer = typename allocator_traits<ValueAllocator>::const_pointer;

        //using reference = value_type&;
        //using const_reference = const value_type&;
        //using size_type = implementation-defined ; // see 24.2
        //using difference_type = implementation-defined ; // see 24.2
        //using local_iterator = implementation-defined ; // see 24.2
        //using const_local_iterator = implementation-defined ; // see 24.2
        //using node_type = unspecified ;
        //using insert_return_type = insert-return-type<iterator, node_type>;

        using key_iterator = typename std::vector<Key, KeyAllocator>::iterator;
        using key_const_iterator = typename std::vector<Key, KeyAllocator>::const_iterator;
//construct/copy/destroy

        //default
        discrete_map()
            : _policy(std::make_unique<P>()), _index_probe(_policy->min_capacity(), std::nullopt) {}

        explicit discrete_map(size_t n,
                              const hasher& hf = Hash(),
                              const key_equal& eql = Pred(),
                              const key_allocator_type& a1 = key_allocator_type(),
                              const value_allocator_type& a2 = value_allocator_type())
            : _policy(std::make_unique<P>()),
              _keys(n, a1),
              _values(n, a2),
              _index_probe(std::max(_policy->min_capacity(), 2*n), std::nullopt)
        {}

        template <class InputIterator>
        discrete_map(InputIterator first,
                     InputIterator last,
                     size_t n,
                     const hasher& hf = hasher(),
                     const key_equal& eq = key_equal(),
                     const key_allocator_type& a1 = key_allocator_type(),
                     const value_allocator_type& a2 = value_allocator_type())
            : _policy(std::make_unique<P>()),
              _keys(n, a1),
              _values(n, a2),
              _index_probe(std::max(_policy->min_capacity(), 2*n), std::nullopt)               
        {
            for (auto it = first; it != last; ++it) {
                const auto pair = *it;
                insert(pair.first, pair.second);
            }
        }

        // Copy constructor
        discrete_map(const discrete_map& other)
            //a class is a friend of itself so no worry about not accessing private members
            : _policy(std::make_unique<P>(*other._policy)), _index_probe(other._index_probe), _keys(other._keys), _values(other._values)
        {}

        // Move constructor
        discrete_map(discrete_map&& other)
            //same as copy but uses std::move
            : _policy(std::move(other._policy)), _index_probe(std::move(other._index_probe)), _keys(std::move(other._keys)), _values(std::move(other._values))
        {}

        explicit discrete_map(const KeyAllocator& a1, const KeyAllocator& a2)
            : unordered_map(0, hasher(), key_equal(), a1, a2)
        {}

        discrete_map(const discrete_map& other, const type_identity_t<KeyAllocator>& a1, const type_identity_t<ValueAllocator>& a2)
            : discrete_map(other)
        {}

        discrete_map(discrete_map&& other, const type_identity_t<KeyAllocator>& a1, const type_identity_t<ValueAllocator>& a2)
            : discrete_map(std::move(other))
        {}

        discrete_map(size_t n, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(n, hasher(), key_equal(), a1, a2)
        {}

        discrete_map(size_t n, const hasher& hf, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(n, hf(), key_equal(), a1, a2)
        {}

        template<class InputIterator>
        discrete_map(InputIterator f, InputIterator l, size_t n, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(f, l, n, hasher(), key_equal(), a1, a2)
        {}

        template<class InputIterator>
        discrete_map(InputIterator f, InputIterator l, size_t n, const hasher& hf, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(f, l, n, hf, a1, a2)
        {}

        //template<container-compatible-range <value_type> R>
        //discrete_map(from_range_t R&& rg, size_t n, const key_allocator_type& a1, const value_allocator_type& a2)
        //    : discrete_map
        //{}

        discrete_map(initializer_list<value_type> il, size_t n, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(il, n, hasher(), key_equal(), a1, a2)
        {}

        discrete_map(initializer_list<value_type> il, size_t n, const hasher& hf, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(il, n, hf, key_equal(), a1, a2)
        {}

        ~discrete_map() = default // <---------------------- DESTRUCTOR HERE


        discrete_map& operator=(const discrete_map& other) {
            if (this != &other) {
                _policy = std::make_unique<P>(*other._policy);
                _index_probe = other._index_probe;
                _keys = other._keys;
                _values = other._values;
            }
            return *this;
        }

        discrete_map& operator=(discrete_map&& other) {
            if (this != &other) {
                _policy = std::move(other._policy);
                _index_probe = std::move(other._index_probe);
                _keys = std::move(other._keys);
                _values = std::move(other._values);
            }
            return *this;
        }

//iterators

        class iterator {
            private:
                using ValueIterator = typename std::vector<T, ValueAllocator>::iterator;
                
                key_iterator _key_it;
                ValueIterator _val_it;

            public:
                iterator(std::vector<Key>::iterator kiter, std::vector<T>::iterator viter) : _key_it(kiter), _val_it(viter) {}
                iterator& operator++() requires BidirectionalIterator {
                    ++_key_it;
                    ++_val_it;
                    return *this;
                }
                iterator& operator--() requires BidirectionalIterator {
                    --_key_it;
                    --_val_it;
                    return *this;
                }
                std::pair<const Key&, T&> operator*() requires BidirectionalIterator {
                        return { *_key_it, *_val_it };
                }
                bool operator==(const iterator& other) requires BidirectionalIterator {
                        return _key_it == other._key_it;
                }
                bool operator!=(const iterator& other) requires BidirectionalIterator {
                        return _key_it != other._key_it;
                }
        };

        class const_iterator {
            private:
                using ValueIteratorConst = typename std::vector<T, ValueAllocator>::const_iterator;

                key_const_iterator _key_it;
                ValueIteratorConst _val_it;

            public:
                const_iterator(std::vector<Key>::const_iterator kter, std::vector<T>::const_iterator vter) : _key_it(kter), _val_it(vter) {}
                const_iterator& operator++() requires BidirectionalIterator {
                    ++_key_it;
                    ++_val_it;
                    return *this;
                }
                const_iterator& operator--() requires BidirectionalIterator {
                    --_key_it;
                    --_val_it;
                    return *this;
                }
                std::pair<const Key&, const T&> operator*() const requires BidirectionalIterator {
                    return { *_key_it, *_val_it };
                }
                bool operator==(const const_iterator& other) const requires BidirectionalIterator {
                    return _key_it == other._key_it;
                }
                bool operator!=(const const_iterator& other) const requires BidirectionalIterator {
                    return _key_it != other._key_it;
                }
        };

        iterator begin() noexcept {
            return iterator(_keys.begin(), _values.begin());
        }

        iterator end() noexcept {
            return iterator(_keys.end(), _values.end());
        }

        const_iterator begin() const noexcept {
            return const_iterator(_keys.begin(), _values.begin());
        }

        const_iterator end() const noexcept {
            return const_iterator(_keys.end(), _values.end());
        }

//getters

        const std::vector<Key>& keys() const noexcept {
            return _keys;
        }

        const std::vector<T>& values() const noexcept {
            return _values;
        }

//capacity

        [[nodiscard]] bool empty() const noexcept {
            return _keys.size() == 0;
        }

        size_t size() const noexcept {
            return _keys.size();
        }

        size_t max_size() const noexcept {
            return static_cast<size_t>(_policy->max_capacity() * _policy->threshold());
        }

//modifiers
       
        /**
         * Inserts a key and value in the map.
         */
        bool insert(const Key key, const T value) noexcept {
            //TODO object construction in-place
            //TODO move semantics
            //we're just using boring linear probing.

            auto ins_fn = [&](const size_t i) {
                std::optional<size_t>& current_kv_index = _index_probe.at(i);
                if (!current_kv_index.has_value()) {
                    //this slot is empty so we will store the kv index here.

                    //Check if we need to resize and rebalance. Do this first so we don't need to do 1 extra rebalance.
                    //TODO possible false positives/negatives due to floating point precision. Ignoring now because inconsequential.
                    if (load_factor() >= _policy->threshold()) {
                        _grow_next();
                    }

                    //since size() is always +1 higher than the last index we do this before inserting.
                    current_kv_index = _keys.size(); 

                    _keys.push_back(key);
                    _values.push_back(value);

                    return true;
                }
                //Check for special case -- overwriting value with existing key. We keep waiting if not same.
                else if (key_eq()(key, _keys[current_kv_index.value()])) {
                    _values[current_kv_index.value()] = value;
                    return true;
                }

                //could not insert. we will revisit at the next index.
                return false;
            };

            size_t k2i = _key2index(key, _index_probe.size());
            return _circular_traversal(k2i, ins_fn);
        }

        /**
         * Erases a pair, given a key.
         */
        bool erase(const Key& key) noexcept {
            auto erase_pair_if_index_match = [&](size_t i) {
                std::optional<size_t>& current_kv_index = _index_probe.at(i);

                if (current_kv_index.has_value() && key_eq()(key, _keys[current_kv_index.value()])) {
                    _keys.erase(_keys.begin() + current_kv_index.value());
                    _values.erase(_values.begin() + current_kv_index.value());
                    current_kv_index.reset();
                    return true;
                }
                //did not erase         
                return false;
            };


        /**
         * Erases a pair, given a key. This version is faster than erase() but it disregards insertion order.
         */
        bool erase_unordered(const Key& key) noexcept {
            auto erase_pair_if_index_match = [&](size_t i) {
                std::optional<size_t>& current_kv_index = _index_probe.at(i);

                if (current_kv_index.has_value() && key_eq()(key, _keys[current_kv_index.value()])) {
                    //swap and pop idiom

                    //we need to change the index of the back key to reflect its new position
                    const Key& back_key = _keys.back();
                    auto change_index_back_key = [&](size_t i){
                        std::optional<size_t>& ckv2 = _index_probe.at(i);
                        if (ckv2.has_value() && key_eq()(back_key, _keys[ckv2.value()])) {
                            ckv2 = _keys[current_kv_index.value()];
                            return true;
                        }
                        return false;
                    };
                    size_t back_k2i = _key2index(back_key, _index_probe.size())
                    _circular_traversal(back_k2i, change_index_back_key);

                    //no do the swap and pop
                    std::swap(_keys[current_kv_index.value()], back_key);
                    _keys.pop_back();

                    std::swap(_values[current_kv_index.value()], _values[current_kv_index.value()])
                    _keys.pop_back();

                    current_kv_index.reset();

                    return true;
                }
                //did not erase         
                return false;
            };
            //driver loop. get a starting index from hash function then move down the indices vector from there. circle to beginning of map if end reached.
            size_t k2i = _key2index(key, _index_probe.size());
            return _circular_traversal(k2i, erase_pair_if_index_match);
        }

    template<class H2, class P2>
        void merge(discrete_map<Key, T, H2, P2>& source) {

    }

//observers

        Hash hash_function() const {
            return Hash();
        }

        Pred key_eq() const {
            return Pred();
        }

//map operations

        auto find(const Key& key) const noexcept {
            auto find_logic = [&](size_t i, iterator& it) {
                const std::optional<size_t>& current_kv_index = _index_probe.at(i);
                if (current_kv_index.has_value() && key_eq()(key, _keys[current_kv_index.value()])) {
                    it = begin() + i;
                    return true;
                }
                //did not find
                it = end();
                return false;
            };

            //driver loop. get a starting index from hash function then move down the indices vector from there. circle to beginning of map if end reached.
            size_t k2i = _key2index(key, _index_probe.size());
            return _circular_traversal(k2i, find_logic);
        }

        bool contains(const K& k) {
            auto find_logic = [&](size_t i) {
                const std::optional<size_t>& current_kv_index = _index_probe.at(i);
                if (current_kv_index.has_value() && key_eq()(key, _keys[current_kv_index.value()])) {
                    value = _values[current_kv_index.value()];
                    return true;
                }
                //did not find
                return false;
            };

            //driver loop. get a starting index from hash function then move down the indices vector from there. circle to beginning of map if end reached.
            size_t k2i = _key2index(key, _index_probe.size());
            return _circular_traversal(k2i, find_logic);
        }

        size_t count(const K& k) {
            return contains(k) ? 1 : 0;
        }

        std::pair<key_iterator, key_iterator> equal_range(const K& k) {
            return std::equal_range(_keys.begin(), _keys.end(), k);
        }

        std::pair<key_const_iterator, key_const_iterator> equal_range(const K& k) const {
            return std::equal_range(_keys.begin(), _keys.end(), k);
        }

//element access

        T& operator[](const Key& k) {
            T* result;
            if(!find(k, result)) {
               insert(k, T{});
               find(k, result);
            }
            return *result;
        }

        T& operator[](Key&& k) {
            T* result;
            if(!find(k, result)) {
               insert(std::move(k), T{});
               find(k, result);
            }
            return *result;
        }

        std::expected<T&, std::error_code> at(const Key& k) {
            auto it = find(k);
            if (it != end()) {
                return it->second;
            }
            return std::make_error_code(std::errc::result_out_of_range);
        }
        
        std::expected<const T&, std::error_code> at(const Key& k) const {
            auto it = find(k);
            if (it != end()) {
                return it->second;
            }
            return std::make_error_code(std::errc::result_out_of_range);
        }

//hash policy

        [[nodiscard]] float load_factor() const noexcept {
            return static_cast<float>(_keys.size()) / static_cast<float>(_index_probe.size());
        }

        void reserve(size_t n) {
            _keys.reserve(n);
            _values.reserve(n);
            //TODO reserving the index probe?
        }

        void rehash(size_t n) {
            //TODO: benchmark whether calling reserve() at the end is beneficial

            if (n <= _index_probe.size()) {
                return;
            }

            //define a new array
            std::vector<std::optional<size_t>> the_bigger_probe(n, std::nullopt);

            auto move_index_if_slot_avail = [&](const size_t i) {
                std::optional<size_t>& current_kv_index = _index_probe.at(i);
                if (current_kv_index.has_value()) {
                    //as we reserve we must rehash. it's more performant to do rehashing here than calling rehash() at the end of this function.
                    size_t index_for_bigger_probe = _policy->get_index(
                        hash_function()(_keys[current_kv_index.value()]),
                        n
                    );
                    the_bigger_probe[index_for_bigger_probe] = std::move(old_kv_index);
                    //return true;
                }
                //by always returning false we are intentionally traversing the whole probe (from beginning), which we want here.
                return false;
            };

            _circular_traversal(0, move_index_if_slot_avail);
            _index_probe = the_bigger_probe;
        }
};

#endif
