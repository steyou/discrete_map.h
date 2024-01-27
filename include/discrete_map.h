#ifndef DISCRETE_MAP_H
#define DISCRETE_MAP_H

//see https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/n4950.pdf
//§ 24.5.4.1

#include <stdexcept>
#include <string>
#include <type_traits>
#include <system_error>
#include <expected>
#include <iterator>
#include <concepts>
#include <utility>
#include <vector>
#include <cstddef>
#include <optional>
#include <expected>
#include <memory>
#include <functional>

#include "BitwiseGrowthPolicy.h"
#include "GrowthPolicy.h"
#include "HashPolicy.h"
#include "linear_prober.h"

#define __STATIC_CAST_K_TO_REAL(k) static_cast<const key_type&>(k)
#define __IGNORE_CONST_QUALIF(type, method, ...) const_cast<type>(static_cast<const this_type&>(*this).method(__VA_ARGS__))

template<class Key,
         class T,
         class Hash = std::hash<Key>,
         class Pred = std::equal_to<Key>,
         class KeyAllocator = std::allocator<const Key>,
         class ValueAllocator = std::allocator<T>,
         class Growth = BitwiseGrowthPolicy,
         template<class> class Probe = linear_prober>
class discrete_map {
    private:
        template<class Size>
        struct SizeTraits {
            using size_type = Size;
            using indices_type = std::optional<size_type>;
        };
        using size_traits = SizeTraits<size_t>;

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
        using size_type = typename size_traits::size_type;

        using key_collection_type = std::vector<key_type, key_allocator_type>;
        using value_collection_type = std::vector<mapped_type, value_allocator_type>;

        using key_iterator = typename key_collection_type::iterator;
        using key_const_iterator = typename key_collection_type::const_iterator;

    private:
        using indices_type = typename size_traits::indices_type;

        using growth_policy_type = Growth;
        using hash_policy_type = HashPolicy<Probe, size_traits>;

        using this_type = discrete_map<key_type, mapped_type, hasher, key_equal, key_allocator_type, value_allocator_type, growth_policy_type, Probe>;

        key_collection_type _keys;
        value_collection_type _values;

        GrowthPolicy<growth_policy_type> _growth_pol;
        hash_policy_type _hash_pol;

        //methods
        
        const indices_type& probe_find(const key_type& k, bool stop_empty=true) const {

            const auto does_key_match = [this, k](size_type current_kv_index) {
                return key_eq()(k, _keys[current_kv_index]);
            };

            const size_type key_to_index = _growth_pol.get_index(
                _hash_pol.size(),
                hash_function()(k)
            );

            return _hash_pol.probe(key_to_index, does_key_match, stop_empty);
        } 

        const indices_type& probe_find(key_type&& k, bool stop_empty=true) const {
            return __IGNORE_CONST_QUALIF(indices_type&, probe_find, std::move(k), stop_empty);
        }
        
        indices_type& probe_find(const key_type& k, bool stop_empty=true) {
            return __IGNORE_CONST_QUALIF(indices_type&, probe_find, k, stop_empty);
        } 

        indices_type& probe_find(key_type&& k, bool stop_empty=true) {
            return __IGNORE_CONST_QUALIF(indices_type&, probe_find, std::move(k), stop_empty);
        }

        template<bool is_const=true>
        class iterator_impl {
            private:
                // local polymorphism
                friend iterator_impl<true>;
                friend iterator_impl<false>;

                using keys_constness_type = typename std::conditional<is_const,
                    const key_collection_type,
                    key_collection_type
                >::type;

                using values_ref_type = typename std::conditional<is_const,
                    const value_collection_type,
                    value_collection_type
                >::type;

                // same regardless of constness
                size_type _index;

                keys_constness_type* _keys;
                values_ref_type* _values;

            public:
                iterator_impl(size_type i, keys_constness_type& keys, values_ref_type& values)
                    : _index(i),
                      _keys(&keys),
                      _values(&values)
                {}
                iterator_impl(iterator_impl<true>& cit)
                    : _index(cit._index),
                      _keys(
                          const_cast<key_collection_type*>(cit._keys)
                      ),
                      _values(
                          const_cast<value_collection_type*>(cit._values)
                      )
                {}
                // dereference
                const value_type operator*() const {
                    return {std::as_const(_keys->at(_index)), _values->at(_index)};
                }
                //postincrement
                iterator_impl& operator++() {
                    _index++;
                    return *this;
                }
                iterator_impl& operator--() {
                    _index--;
                    return *this;
                }
                //preincrement 
                iterator_impl<is_const> operator++(int) {
                    iterator_impl<is_const> temp = *this;
                    ++(*this);
                    return temp;
                }
                iterator_impl<is_const> operator--(int) {
                    iterator_impl<is_const> temp = *this;
                    --(*this);
                    return &temp;
                }
                // +/-
                iterator_impl<is_const> operator+(size_type n) const {
                    return iterator_impl<is_const>(_index + n, *_keys, *_values);
                }
                iterator_impl<is_const> operator-(size_type n) const {
                    return *this + (-n);
                }
                // == / !=
                bool operator==(const iterator_impl& other) const {
                    return _index == other._index;
                }
                bool operator!=(const iterator_impl& other) const {
                    return !(*this == other);
                }
        };

    public:

        using const_iterator = iterator_impl<true>;
        using iterator = iterator_impl<false>;

        iterator begin() noexcept {
            return iterator(0, _keys, _values);
        }

        iterator end() noexcept {
            return iterator(size(), _keys, _values);
        }

        const_iterator begin() const noexcept {
            return const_iterator(0, _keys, _values);
        }

        const_iterator end() const noexcept {
            return const_iterator(size(), _keys, _values);
        }

        const_iterator cbegin() const noexcept {
            return const_iterator(0, _keys, _values);
        }

        const_iterator cend() const noexcept {
            return const_iterator(size(), _keys, _values);
        }

    public:
//construct/copy/destroy

        //default
        discrete_map()
            : _keys(),
              _values(),
              _hash_pol(
                  _growth_pol.min_capacity() 
              )
        {}

        explicit discrete_map
            (
                size_type n,
                const hasher& hfn = hasher(),
                const key_equal& eql = key_equal(),
                const key_allocator_type& a1 = key_allocator_type(),
                const value_allocator_type& a2 = value_allocator_type()
            )
            : _keys(n, a1),
              _values(n, a2),
              _hash_pol(
                  _growth_pol.min_capacity()
              )
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
            :  _keys(n, a1),
              _values(n, a2),
              _hash_pol(
                  _growth_pol.min_capacity()
              )
        {
            for (auto it = first; it != last; ++it) {
                const auto pair = *it;
                insert(pair);
            }
        }

        // Copy constructor
        discrete_map(const discrete_map& other)
              : _growth_pol(other._growth_pol),
                _hash_pol(_growth_pol.min_capacity()),
                _keys(other._keys),
                _values(other._values)
        {}

        // Move constructor
        discrete_map(discrete_map&& other)
              : _growth_pol(std::move(other._growth_pol)),
                _hash_pol(std::move(_growth_pol.min_capacity())),
                _keys(std::move(other._keys)),
                _values(std::move(other._values))
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

        discrete_map& operator=(const discrete_map& other) {
            discrete_map(std::forward<const discrete_map&>(other));
            return *this;
        }

        discrete_map& operator=(discrete_map&& other) {
            discrete_map(std::forward<discrete_map&&>(other));
            return *this;
        }

        ~discrete_map() = default; // <---------------------- DESTRUCTOR HERE


//iterators

//getters

        const key_collection_type& keys() const noexcept {
            return _keys;
        }

        const value_collection_type& values() const noexcept {
            return _values;
        }

        key_allocator_type get_key_allocator() const noexcept {
            return key_allocator_type();
        }

        value_allocator_type get_value_allocator() const noexcept {
            return value_allocator_type();
        }

//capacity

        [[nodiscard]] bool empty() const noexcept {
            return size() == 0;
        }

        size_type size() const noexcept {
            //for every key is one value. therefore keys size == values size.
            return _keys.size();
        }

        size_type max_size() const noexcept {
            return static_cast<size_type>(_growth_pol.max_capacity() * _hash_pol.threshold());
        }

//modifiers
       
       template<class... Args>
       std::pair<iterator, bool> emplace(Args&&... args) {
           return insert(
               std::make_pair(std::forward<Args>(args)...)
           );
       }

       std::pair<iterator, bool> insert(const value_type& obj) {

           indices_type& maybe_index = probe_find(obj.first);

           // probe_find stops either if the keys match or there was no key. therefore we just check for presence of key, as it's implied to be the key we're searching for.
           if (maybe_index.has_value()) {
               //according to STL this version doesn't update existing keys.
               return std::make_pair(begin() + maybe_index.value(), false);
           }

           //handling of where the probe found empty slot. Here we actually do an insert.

           // can't use the public interface load_factor() because we're forward looking, which that function isn't.
           if (_hash_pol.load_factor(this->size() + 1) >= _hash_pol.threshold()) {
               rehash(
                   // I want to avoid `+ 1` in case the growth policy is based on primes or power2
                   _growth_pol.next_capacity(_hash_pol.size())
               );
           }

           maybe_index = size();
           _keys.push_back(obj.first);
           _values.push_back(obj.second);
           
           return {end()-1, true};
       }

       //std::pair<iterator, bool> insert(value_type&& obj) {
       //    return insert(std::move(obj));
       //}

       template<class P>
       std::pair<iterator, bool> insert(P&& obj) {
           return insert(std::forward<P>(obj));
       }

       iterator insert(const_iterator hint, const value_type& obj) {

           if (!hint.has_value()) {

           }

           indices_type maybe_index = probe_find(obj.first);

           if (maybe_index.has_value()) {

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

       iterator erase(iterator position) {

           if (position == end()) {
               return position;
           }

           //We're about to call _hash_pol.probe, which takes some setup. We need a full reference to the element in the probe, not just the resulting value.

           value_type kv_pair = *position;

           indices_type& maybe_index = probe_find(kv_pair.first, false);

           // Now do the erasing.
           // Normally you'd check for empty optional but in this context it's always the value we want.
           _keys.erase(_keys.begin() + maybe_index.value());
           _values.erase(_values.begin() + maybe_index.value());
           maybe_index = std::nullopt;

           return position;
       }

       iterator erase(const_iterator position) {
           return erase(
              iterator(position)
           );
       }

       bool erase(const key_type& k) noexcept {

           indices_type maybe_index = probe_find(k.first);

           //if the probe found that keys match...
           if (maybe_index.has_value()) {
               // erasey timey
               _keys.erase(_keys.begin() + maybe_index.value());
               _values.erase(_values.begin() + maybe_index.value());
               maybe_index = std::nullopt;
               return true;
           }

           return false;
       }

       iterator erase(const_iterator first, const_iterator last) {

           //erase until the two iterators are identical.
           //this works because erase() deletes at the current position and shifts everything beyond the deleted element into its place.
           //this logic assumes that first doesn't change in erase().
           iterator mut_first = iterator(first);
           iterator mut_last = iterator(last);

           while (mut_first != mut_last) {
                erase(mut_first);
               mut_first++;
           }
           return mut_first;

       }

       value_type extract(const_iterator position) {

       }

       value_type extract(const key_type& x) {
           indices_type maybe_index = find(x);
           if (maybe_index.has_value()) {
               
           }
       }

       void clear() noexcept {
           _keys.clear();
           _values.clear();
           _hash_pol.clear();
       }

       template<class H2, class P2>
       void merge(discrete_map& source) {

       }

//observers

        key_equal key_eq() const {
            return Pred();
        }

        hasher hash_function() const {
            return hasher();
        }

//map operations

        iterator find(const key_type& k) {
            indices_type result = probe_find(k);
            if (result.has_value()) {
                return begin() + result.value();
            }
            else {
                return end();
            }
        }

        template<class K,
                 typename = std::enable_if_t<std::is_convertible<K, key_type>::value>>
        iterator find(const K& k) {
            return find(__STATIC_CAST_K_TO_REAL(k));
        }

        const_iterator find(const key_type& k) const {
            indices_type result = probe_find(k);
            if (result.has_value()) {
                return cbegin() + result.value();
            }
            else {
                return cend();
            }
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
            indices_type& result = probe_find(k);

            if (result.has_value()) {
                return _values[result.value()];
            }

            result = size();
            _keys.emplace_back(k);
            _values.emplace_back(T{});

            return _values.back();
        }

        mapped_type& operator[](const key_type&& k) {
            return operator[](std::forward<const key_type&>(k));
        }
        
        const mapped_type& at(const Key& k) const {
            const indices_type& result = probe_find(k);
            if (result.has_value()) {
                return _values[result.value()];
            }
            throw std::out_of_range("discrete_map::at() const thrown exception: key out of range.");
        }

        mapped_type& at(const Key& k) {
            return __IGNORE_CONST_QUALIF(mapped_type&, at, k);
        }

//hash policy

        [[nodiscard]] float load_factor() const noexcept {
            return _hash_pol.load_factor(size());
        }

        void reserve(size_type n) {
            _keys.reserve(n);
            _values.reserve(n);
            //TODO reserving the index probe?
        }

        void rehash(size_type next) {
            _hash_pol.rehash(next, [this, next](size_type existing_key_index){
                return _growth_pol.get_index(
                    next,
                    hash_function()(_keys[existing_key_index])
                );
            });
        }
};

#endif
