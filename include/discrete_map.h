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

#include "BitwiseGrowthPolicy.h"
#include "GrowthPolicy.h"
#include "HashPolicy.h"
#include "linear_prober.h"

template<class Key,
         class T,
         class Hash = std::hash<Key>,
         class Pred = std::equal_to<Key>,
         class G = BitwiseGrowthPolicy,
         class H = linear_prober<Hash, size_t>,
         class KeyAllocator = std::allocator<const Key>,
         class ValueAllocator = std::allocator<T>>
class discrete_map {
    private:
        std::unique_ptr<GrowthPolicy> _growth_pol;
        std::unique_ptr<HashPolicy> _hash_pol;

        //This vector acts as a container of indices, inspired by unordered_map buckets. It's used to store the shared index of a key-value pair, determined at insertion after the hash function derives the 'bucket'. Unlike the existing STL maps, this vector is what abstracts away probing, resizing, and collision management, keeping these operations separate from the key-value pairs. That allows the kv-pairs can be public const ref& exposed with little risk.

        std::vector<const Key, KeyAllocator> _keys;
        std::vector<T, ValueAllocator> _values;

        //methods

        size_type _key2index(const Key& k) const noexcept {
            return _growth_pol->get_index(
                _hash_pol->hash_function()(k),
                _hash_pol->size()
            );
        } 

        void _grow_next() {
            size_type new_size = _growth_pol->next_capacity(_hash_pol.size());
            rehash(new_size);
        }

    public:
        // types
        using key_type = Key;
        using mapped_type = T;
        using value_type = pair<const Key, T>;
        using hasher = Hash;
        using key_equal = Pred;

        using key_allocator_type = KeyAllocator;
        //using key_pointer = typename allocator_traits<KeyAllocator>::pointer;
        using key_const_pointer = typename allocator_traits<KeyAllocator>::const_pointer;

        using value_allocator_type = ValueAllocator;
        //using value_pointer = typename allocator_traits<ValueAllocator>::pointer;
        //using value_const_pointer = typename allocator_traits<ValueAllocator>::const_pointer;

        //using reference = value_type&;
        //using const_reference = const value_type&;
        using size_type = size_t; // see 24.2
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
            : _growth_pol(std::make_unique<G>()),
              _hash_pol(std::make_unique<H>(_growth_pol->min_capacity()))
        {}

        explicit discrete_map
            (
                size_type n,
                const hasher& hf = hasher(),
                const key_equal& eql = equal_to(),
                const key_allocator_type& a1 = key_allocator_type(),
                const value_allocator_type& a2 = value_allocator_type()
            )
            : _growth_pol(std::make_unique<G>()),
              _hash_pol(std::make_unique<H>(std::max(2*n, _growth_pol->min_capacity()))),
              _keys(n, a1),
              _values(n, a2)
        {}

        template <class InputIterator>
        discrete_map
            (
                InputIterator first,
                InputIterator last,
                size_type n,
                const hasher& hf = hasher(),
                const key_equal& eq = key_equal(),
                const key_allocator_type& a1 = key_allocator_type(),
                const value_allocator_type& a2 = value_allocator_type()
            )
            : _growth_pol(std::make_unique<P>()),
              _keys(n, a1),
              _values(n, a2),
              _hash_pol(std::make_unique<H>(std::max(2*n_growth_pol->min_capacity())))               
        {
            for (auto it = first; it != last; ++it) {
                const auto pair = *it;
                insert(pair.first, pair.second);
            }
        }

        // Copy constructor
        discrete_map(const discrete_map& other)
            //a class is a friend of itself so no worry about not accessing private members
            : _growth_pol(std::make_unique<P>(*other._growth_pol)),
              _hash_pol(other._hash_pol),
              _keys(other._keys, other.key_allocator_type),
              _values(other._values, other.value_allocator_type)
        {}

        // Move constructor
        discrete_map(discrete_map&& other)
            //same as copy but uses std::move
            : _growth_pol(std::move(other._growth_pol)),
              _hash_pol(std::move(other._hash_pol)),
              _keys(std::move(other._keys), other.key_allocator_type),
              _values(std::move(other._values), other.key_allocator_type)
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

        discrete_map(size_type n, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(n, hasher(), key_equal(), a1, a2)
        {}

        discrete_map(size_type n, const hasher& hf, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(n, hf(), key_equal(), a1, a2)
        {}

        template<class InputIterator>
        discrete_map(InputIterator f, InputIterator l, size_type n, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(f, l, n, hasher(), key_equal(), a1, a2)
        {}

        template<class InputIterator>
        discrete_map(InputIterator f, InputIterator l, size_type n, const hasher& hf, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(f, l, n, hf, a1, a2)
        {}

        //template<container-compatible-range <value_type> R>
        //discrete_map(from_range_t R&& rg, size_type n, const key_allocator_type& a1, const value_allocator_type& a2)
        //    : discrete_map
        //{}

        discrete_map(initializer_list<value_type> il, size_type n, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(il, n, hasher(), key_equal(), a1, a2)
        {}

        discrete_map(initializer_list<value_type> il, size_type n, const hasher& hf, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(il, n, hf, key_equal(), a1, a2)
        {}

        ~discrete_map() = default // <---------------------- DESTRUCTOR HERE


        discrete_map& operator=(const discrete_map& other) {
            if (this != &other) {
                _growth_pol = std::make_unique<P>(*other._growth_pol);
                _hash_pol = other._hash_pol;
                _keys = other._keys;
                _values = other._values;
            }
            return *this;
        }

        discrete_map& operator=(discrete_map&& other) {
            if (this != &other) {
                _growth_pol = std::move(other._growth_pol);
                _hash_pol = std::move(other._hash_pol);
                _keys = std::move(other._keys);
                _values = std::move(other._values);
            }
            return *this;
        }

        key_allocator_type get_key_allocator() const noexcept {
            return KeyAllocator;
        }

        value_allocator_type get_value_allocator() const noexcept {
            return ValueAllocator;
        }

//getters

        const std::vector<Key>& keys() const noexcept {
            return _keys;
        }

        const std::vector<T>& values() const noexcept {
            return _values;
        }

        std::unique_ptr<T> values() const noexcept {
            return std::make_unique(_values.data());
        }

//iterators

        class iterator {
            private:
                using ValueIterator = typename std::vector<T, ValueAllocator>::iterator;
                
                key_iterator _key_it;
                ValueIterator _val_it;

            public:
                iterator(std::vector<Key>::iterator kiter, std::vector<T>::iterator viter)
                    : _key_it(kiter), _val_it(viter)
                {}
                iterator& operator++() requires std::bidirectional_iterator {
                    ++_key_it;
                    ++_val_it;
                    return *this;
                }
                iterator& operator--() requires std::bidirectional_iterator {
                    --_key_it;
                    --_val_it;
                    return *this;
                }
                std::pair<const Key&, T&> operator*() requires std::bidirectional_iterator {
                        return { *_key_it, *_val_it };
                }
                bool operator==(const iterator& other) requires std::bidirectional_iterator {
                        return _key_it == other._key_it;
                }
                bool operator!=(const iterator& other) requires std::bidirectional_iterator {
                        return _key_it != other._key_it;
                }
        };

        class const_iterator {
            private:
                using ValueIteratorConst = typename std::vector<T, ValueAllocator>::const_iterator;

                key_const_iterator _key_it;
                ValueIteratorConst _val_it;

            public:
                const_iterator(std::vector<Key>::const_iterator kter, std::vector<T>::const_iterator vter)
                    : _key_it(kter), _val_it(vter)
                {}
                const_iterator& operator++() requires std::bidirectional_iterator {
                    ++_key_it;
                    ++_val_it;
                    return *this;
                }
                const_iterator& operator--() requires std::bidirectional_iterator {
                    --_key_it;
                    --_val_it;
                    return *this;
                }
                std::pair<const Key&, const T&> operator*() const requires std::bidirectional_iterator {
                    return { *_key_it, *_val_it };
                }
                bool operator==(const const_iterator& other) const requires std::bidirectional_iterator {
                    return _key_it == other._key_it;
                }
                bool operator!=(const const_iterator& other) const requires std::bidirectional_iterator {
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

        const_iterator cbegin() const noexcept {
            return const_iterator(_keys.begin(), _values.begin());
        }

        const_iterator cend() const noexcept {
            return const_iterator(_keys.end(), _values.end());
        }

//capacity

        [[nodiscard]] bool empty() const noexcept {
            return _keys.size() == 0;
        }

        size_type size() const noexcept {
            return _keys.size();
        }

        size_type max_size() const noexcept {
            return static_cast<size_type>(_growth_pol->max_capacity() * _growth_pol->threshold());
        }

//modifiers
       
        template<class... Args>
        std::pair<iterator, bool> emplace(Args&&... args) {

        }

        template<class... Args>
        iterator emplace_hint(const_iterator position, Args&&... args) {

        }

        std::pair<iterator, bool> insert(const value_type& obj) {
            //TODO object construction in-place
            //TODO move semantics
            //we're just using boring linear probing.

            auto insert_empty = [&](size_type current_kv_index) {
                if (!current_kv_index.has_value()) {
                    //this slot is empty so we will store the kv index here.

                    //Check if we need to resize and rebalance. Do this first so we don't need to do 1 extra rebalance.
                    if (_hash_pol->load_factor() >= _hash_pol->threshold()) {
                        _grow_next();
                    }

                    current_kv_index = _keys.size(); 
                    _keys.push_back(key);
                    _values.push_back(value);

                    return true;
                }
                //Check for special case -- overwriting value with existing key. We keep waiting if not same.
                else if (key_eq()(key, _keys[current_kv_index.value()])) {
                    //This version doesn't update existing key. Just return true to exit probing.
                    //_values[current_kv_index.value()] = value;
                    return true;
                }

                //could not insert. we will revisit at the next index.
                return false;
            };

            size_type k2i = _key2index(key);
            return _hash_pol->probe(k2i, insert_empty);
        }

        std::pair<iterator, bool> insert(value_type&& obj) {
            
        }

        template<class P>
        std::pair<iterator, bool> insert(P&& obj) {

        }

        iterator insert(const_iterator hint, const value_type& obj) {

        }

        iterator insert(const_iterator hint, value_type&& obj) {

        }

        template<class P>
        iterator insert(const_iterator, P&& obj) {

        }

        void insert(initializer_list<value_type> li) {
            for (auto it = li.begin(); it != li.end(); ++li) {
                insert(it);
            }
        }

        template<class InputIterator>
        void insert(InputIterator first, InputIterator last) {
            for (auto it = first; it != last; ++it) {
                insert(it);
            }
        }

        /**
         * Erases a pair, given a key.
         */
        bool erase(const Key& k) noexcept {
            auto erase_pair_if_index_match = [&](std::optional<size_type>& current_kv_index) {

                if (key_eq()(key, _keys[current_kv_index.value()])) {
                    _keys.erase(_keys.begin() + current_kv_index.value());
                    _values.erase(_values.begin() + current_kv_index.value());
                    current_kv_index.reset();
                    return true;
                }
                //did not erase         
                return false;
            };
            size_type k2i = _key2index(k);
            _hash_pol->probe(k2i, erase_pair_if_index_match)
        }

        /**
         * Erases a pair, given a key. This version is faster than erase() but it disregards insertion order.
         */
        //bool erase_unordered(const Key& key) noexcept {
        //    auto erase_pair_if_index_match = [&](std::optional<size_type>& current_kv_index) {

        //        if (current_kv_index.has_value() && key_eq()(key, _keys[current_kv_index.value()])) {
        //            //swap and pop idiom

        //            //we need to change the index of the back key to reflect its new position
        //            const Key& back_key = _keys.back();
        //            auto change_index_back_key = [&](size_type i){
        //                std::optional<size_type>& ckv2 = _hash_pol.at(i);
        //                if (ckv2.has_value() && key_eq()(back_key, _keys[ckv2.value()])) {
        //                    ckv2 = _keys[current_kv_index.value()];
        //                    return true;
        //                }
        //                return false;
        //            };
        //            size_type back_k2i = _key2index(back_key, _hash_pol.size())
        //            _circular_traversal(back_k2i, change_index_back_key);

        //            //no do the swap and pop
        //            std::swap(_keys[current_kv_index.value()], back_key);
        //            _keys.pop_back();

        //            std::swap(_values[current_kv_index.value()], _values[current_kv_index.value()])
        //            _keys.pop_back();

        //            current_kv_index.reset();

        //            return true;
        //        }
        //        //did not erase         
        //        return false;
        //    };
        //    //driver loop. get a starting index from hash function then move down the indices vector from there. circle to beginning of map if end reached.
        //    size_type k2i = _key2index(key, _hash_pol.size());
        //    return _circular_traversal(k2i, erase_pair_if_index_match);
        //}

    template<class H2, class P2>
    void merge(discrete_map& source) {

    }

//observers

        key_equal key_eq() const {
            return Pred();
        }

//map operations

        auto find(const Key& key) const noexcept {
            auto find_logic = [&](size_type current_kv_index) {
                if (key_eq()(key, _keys[current_kv_index.value()])) {
                    return true;
                }
                return false;
            };

            //driver loop. get a starting index from hash function then move down the indices vector from there. circle to beginning of map if end reached.
            size_type k2i = _key2index(key);
            std::optional<auto> possible_iterator = _hash_pol->probe(k2i, find_logic);
            return possible_iterator.has_value() ? possible_iterator.value() : _hash_pol->end();
        }

        bool contains(const K& k) {
            bool found = false;
            auto contains_logic = [&](std::optional<size_type>& current_kv_index) {
                if (current_kv_index.has_value() && key_eq()(key, _keys[current_kv_index.value()])) {
                    found = true;
                    return true;
                }
                //did not find
                return false;
            };

            //driver loop. get a starting index from hash function then move down the indices vector from there. circle to beginning of map if end reached.
            size_type k2i = _key2index(key);
            _hash_pol->probe(k2i, contains_logic);
            return found;
        }

        size_type count(const K& k) {
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
            return _hash_pol->load_factor(_keys.size());
        }

        void reserve(size_type n) {
            _keys.reserve(n);
            _values.reserve(n);
            //TODO reserving the index probe?
        }

        void rehash(size_type n) {
            _hash_pol->rehash(n, _keys, _growth_pol->next_index)
        }
};

#endif
