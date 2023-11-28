#ifndef DISCRETE_MAP_H
#define DISCRETE_MAP_H

//see https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/n4950.pdf
//§ 24.5.4.1

#include <type_traits>
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
    public:
        // types
        using key_type = Key;
        using mapped_type = T;
        using value_type = std::pair<const Key, T>;
        using hasher = Hash;
        using key_equal = Pred;

        using key_allocator_type = KeyAllocator;
        //using key_pointer = typename allocator_traits<KeyAllocator>::pointer;
        using key_const_pointer = typename std::allocator_traits<key_allocator_type>::const_pointer;

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

        using key_iterator = typename std::vector<key_type, key_allocator_type>::iterator;
        using key_const_iterator = typename std::vector<key_type, key_allocator_type>::const_iterator;
    private:
        using index_iterator = HashPolicy<hasher, key_type, size_type>::index_iterator;
        using const_index_iterator = HashPolicy<hasher, key_type, size_type>::const_index_iterator;

        std::unique_ptr<GrowthPolicy> _growth_pol;
        std::unique_ptr<HashPolicy<hasher, key_type, size_type>> _hash_pol;

        std::vector<key_type, key_allocator_type> _keys;
        std::vector<mapped_type, value_allocator_type> _values;

        //methods

        void _grow_next() {
            size_type new_size = _growth_pol->next_capacity(_hash_pol.size());
            rehash(new_size);
        }

        size_type _key2index(const Key& k) const noexcept {
            return _growth_pol->get_index(
                _hash_pol->hash_function()(k),
                _hash_pol->size()
            );
        } 

        index_iterator probe_find(const key_type& k) {

            auto does_key_match = [&](size_type current_kv_index) {
                return key_eq()(k.first, _keys[current_kv_index]);
            };

            size_type k2i = _key2index(k.first);
            return _hash_pol->probe(k2i, does_key_match);
        }

        const_index_iterator probe_find(const key_type& k) const {

            auto does_key_match = [&](size_type current_kv_index) {
                return key_eq()(k.first, _keys[current_kv_index]);
            };

            size_type k2i = _key2index(k.first);
            return _hash_pol->probe(k2i, does_key_match);
        }

    public:
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
                const key_equal& eql = key_equal(),
                const key_allocator_type& a1 = key_allocator_type(),
                const value_allocator_type& a2 = value_allocator_type()
            )
            : _growth_pol(std::make_unique<G>()),
              _hash_pol(
                std::make_unique<H>(std::max(2*n, _growth_pol->min_capacity())),
                _growth_pol->get_index
              ),
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
            : _growth_pol(std::make_unique<G>()),
              _keys(n, a1),
              _values(n, a2),
              _hash_pol(
                std::make_unique<H>(std::max(2*n, _growth_pol->min_capacity())),
                _growth_pol->get_index
              )
        {
            for (auto it = first; it != last; ++it) {
                const auto pair = *it;
                insert(pair);
            }
        }

        // Copy constructor
        discrete_map(const discrete_map& other)
            //a class is a friend of itself so no worry about not accessing private members
            : _growth_pol(std::make_unique<G>(*other._growth_pol)),
              _hash_pol(other._hash_pol),
              _keys(other._keys, other.key_allocator_type),
              _values(other._values, other.value_allocator_type)
        {}

        // Move constructor
        discrete_map(discrete_map&& other)
            //same as copy but uses std::move
            : _growth_pol(std::move(other._growth_pol)),
              _hash_pol(std::move(other._hash_pol)),
              _keys(std::move(other._keys), std::move(other.key_allocator_type)),
              _values(std::move(other._values), std::move(other.key_allocator_type))
        {}

        explicit discrete_map(const KeyAllocator& a1, const KeyAllocator& a2)
            : discrete_map(0, hasher(), key_equal(), a1, a2)
        {}

        discrete_map(const discrete_map& other, const std::type_identity_t<KeyAllocator>& a1, const std::type_identity_t<ValueAllocator>& a2)
            : discrete_map(other)
        {}

        discrete_map(discrete_map&& other, const std::type_identity_t<KeyAllocator>& a1, const std::type_identity_t<ValueAllocator>& a2)
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

        discrete_map(std::initializer_list<value_type> il, size_type n, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(il, n, hasher(), key_equal(), a1, a2)
        {}

        discrete_map(std::initializer_list<value_type> il, size_type n, const hasher& hf, const key_allocator_type& a1, const value_allocator_type& a2)
            : discrete_map(il, n, hf, key_equal(), a1, a2)
        {}

        ~discrete_map() = default; // <---------------------- DESTRUCTOR HERE


        discrete_map& operator=(const discrete_map& other) {
            if (this != &other) {
                _growth_pol = std::make_unique<G>(*other._growth_pol);
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
            return key_allocator_type();
        }

        value_allocator_type get_value_allocator() const noexcept {
            return value_allocator_type();
        }

//getters

        const std::vector<Key>& keys() const noexcept {
            return _keys;
        }

        const std::vector<T>& values() const noexcept {
            return _values;
        }

//iterators

        class iterator {
            private:
                size_type likely_index;

                index_iterator reverse_lookup(size_type target) {
                    return find(_keys[target]);
                }

            public:
                iterator(size_type kv_index)
                    : i(kv_index), probe(reverse_lookup(i))
                iterator(index_iterator p)
                    : probe(p)
                {}
                iterator& next() {

                    //We just want the next valid value in the probe and don't need any other condition.
                    auto always_true = [&](size_type current_kv_index) {
                        //return !(key_eq()(_keys[i], _keys[current_kv_index]));
                        return true;
                    };

                    index_iterator tmp;
                    
                    for (size_type i = 0; !*(tmp).has_value(); i++) {
                        tmp = _hash_pol->probe(
                            find(_keys[likely_index]) + 1,
                            always_true
                        ) + i;
                    }

                    likely_index = tmp.value();
                }
                bool has_value() {
                    return *(probe).has_value();
                }
                iterator& operator++() {
                    likely_index++;
                    return *this;
                }
                iterator& operator--() {
                    likely_index--;
                    return *this;
                }
                value_type operator*() {
                    return { _keys[likely_index], _values[likely_index] };
                }
                bool operator==(const iterator& other) {
                    return likely_index == other.likely_index;
                }
                bool operator!=(const iterator& other) {
                    return !(likely_index == other.likely_index);
                }
        };

        iterator begin() noexcept {
            return iterator(0);
        }

        iterator end() noexcept {
            return iterator(size()-1);
        }

        const_iterator cbegin() const noexcept {
            return const_iterator(_keys.begin(), _values.begin());
        }

        const_iterator cend() const noexcept {
            return const_iterator(_keys.end(), _values.end());
        }

//capacity

        [[nodiscard]] bool empty() const noexcept {
            return size() == 0;
        }

        size_type size() const noexcept {
            return _keys.size();
        }

        size_type max_size() const noexcept {
            return static_cast<size_type>(_growth_pol->max_capacity() * _hash_pol->threshold());
        }

//modifiers
       
        template<class... Args>
        std::pair<iterator, bool> emplace(Args&&... args) {
            value_type kv = std::make_pair(std::forward<Args>(args)...);
            return insert(kv);
        }

        std::pair<iterator, bool> insert(const value_type& obj) {

            index_iterator ix_it = probe_find(obj.first);

            //if the probe found that the keys match (the only time it can have a value is if keys match)...
            if (*(ix_it).has_value()) {
                //according to STL this version doesn't update existing keys.
                return { iterator(ix_it), false };
            }

            //handling of where the probe found empty slot.

            iterator result = end();

            *(ix_it) = size();
            _keys.push_back(obj.first);
            _values.push_back(obj.second);
            
            return { result, true };
        }

        std::pair<iterator, bool> insert(value_type&& obj) {
//TODO forwarding
            index_iterator ix_it = probe_find(obj.first);

            //if the probe found that the keys match (the only time it can have a value is if keys match)...
            if (*(ix_it).has_value()) {
                //according to STL this version doesn't update existing keys.
                return { iterator(ix_it), false };
            }

            //handling of where the probe found empty slot.

            iterator result = end();

            *(ix_it) = _keys.size();
            _keys.emplace_back(std::move(obj.first));
            _values.emplace_back(std::move(obj.second));

            return { result, true };
        }

        template<class P>
        std::pair<iterator, bool> insert(P&& obj) {
            value_type kv = std::make_pair(std::forward<P>(obj));
            return insert(kv);
        }

        iterator insert(const_iterator hint, const value_type& obj) {

            if (!hint.has_value()) {

            }

            index_iterator ix_it = probe_find(obj.first);

            if (*(ix_it).has_value()) {

            }


        }

        iterator insert(const_iterator hint, value_type&& obj) {

        }

        template<class P>
        iterator insert(const_iterator, P&& obj) {

        }

        void insert(std::initializer_list<value_type> li) {
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
        bool erase(const key_type& k) noexcept {

            index_iterator ix_it = probe_find(k.first);

            //if the probe found that keys match...
            if (*(ix_it).has_value()) {
                // erasey timey
                _keys.erase(_keys.begin() + *(ix_it).value());
                _values.erase(_values.begin() + *(ix_it).value());
                *(ix_it) = std::nullopt;
            }

            return false;
        }

        /**
         * Erases a pair, given a key. This version is faster than erase() but it disregards insertion order.
         */
        bool erase_unordered(const key_type& k) noexcept {
            //swap and pop idiom.

            index_iterator ix_it = probe_find(k.first);

            if (*(ix_it).has_value()) {

                //do a reverse lookup on the last key so that we can swap the index with the one we're removing.
                //TODO remove dirty direction
                std::optional<size_type> index_of_last_key = *(find(_keys[size() - 1]));
                
                std::optional<size_type> i = *(ix_it).value();

                std::swap(_keys[i], _keys.end());
                std::swap(_values[i], _values.end());

                _keys.pop_back();
                _values.pop_back();
                i = index_of_last_key;
                index_of_last_key = std::nullopt;

            }
            return false;
        }

        value_type extract(const_iterator position) {

        }

        value_type extract(const key_type& x) {
            index_iterator maybe_index = find(x);
            if (*maybe_index.has_value()) {
                
            }
        }

        void clear() noexcept {
            _keys.clear();
            _values.clear();
            _hash_pol->clear();
        }

        template<class H2, class P2>
        void merge(discrete_map& source) {

        }

//observers

        key_equal key_eq() const {
            return Pred();
        }

        hasher hash_function() const {
            return _hash_pol->hash_function();
        }

//map operations

        iterator find(const key_type& k) {
            return iterator(
                probe_find(k.first)
            );
        }

        template<class K,
                 typename = std::enable_if_t<std::is_convertible<K, key_type>::value>>
        iterator find(const K& k) {
            return find(__STATIC_CAST_K_TO_REAL(k));
        }

        const_iterator find(const key_type& k) const {
            return const_iterator(
                probe_find(k.first)
            );
        }

        template<class K,
                 typename = std::enable_if_t<std::is_convertible<K, key_type>::value>>
        const_iterator find(const K& k) const {
            return find(__STATIC_CAST_K_TO_REAL(k));
        }

        size_type count(const key_type& k) {
            return contains(k) ? 1 : 0;
        }

        template<class K,
                 typename = std::enable_if_t<std::is_convertible<K, key_type>::value>>
        size_type count(const K& k) {
            return count(__STATIC_CAST_K_TO_REAL(k));
        }

        bool contains(const key_type& k) {
            iterator result = find(k);
            return result.has_value();
        }

        template<class K,
                 typename = std::enable_if_t<std::is_convertible<K, key_type>::value>>
        bool contains(const K& k) {
            return contains(__STATIC_CAST_K_TO_REAL(k));
        }

        std::pair<iterator, iterator> equal_range(const key_type& k) {
            return std::equal_range(begin(), end(), k);
        }

        std::pair<const_iterator, const_iterator> equal_range(const key_type& k) const {
            return std::equal_range(cbegin(), cend(), k);
        }

        template<class K,
                 typename = std::enable_if_t<std::is_convertible<K, key_type>::value>>
        std::pair<iterator, iterator> equal_range(const K& k) {
            return equal_range(__STATIC_CAST_K_TO_REAL(k));
        }

        template<class K,
                 typename = std::enable_if_t<std::is_convertible<K, key_type>::value>>
        std::pair<const_iterator, const_iterator> equal_range(const K& k) const {
            return equal_range(__STATIC_CAST_K_TO_REAL(k));
        }

//element access

        mapped_type& operator[](const key_type& k) {
            T* result;
            if(!find(k, *result)) {
               insert(k, T{});
            }
            return *result;
        }

        const_iterator operator[](const key_type& k) const {
            T* result;
            index_iterator maybe_index = find(k);
            if(!find(k)) {
               insert(k, T{});
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

        void rehash(size_type next) {
            _hash_pol->rehash(next, _keys);
        }
};

#endif
