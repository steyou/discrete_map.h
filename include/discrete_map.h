#ifndef DISCRETE_MAP_H
#define DISCRETE_MAP_H

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

template<class Key,
         class T,
         class P = BitwiseMapPolicy>
class discrete_map {
    private:
       std::hash<Key> _hash;
       std::unique_ptr<MapPolicy> _policy;

       //This vector acts as a container of indices, inspired by unordered_map buckets. It's used to store the shared index of a key-value pair, determined at insertion after the hash function derives the 'bucket'. Unlike the existing STL maps, this vector is what abstracts away probing, resizing, and collision management, keeping these operations separate from the key-value pairs. That allows the kv-pairs can be public const ref& exposed with little risk.
       std::vector<std::optional<size_t>> _index_probe;

       std::vector<Key> _keys;
       std::vector<T> _values;

       size_t _key2index(const Key& key, const size_t capacity) const noexcept {
           size_t hash_raw = _hash(key);
           return _policy->get_index(hash_raw, capacity);
       } 

       bool _circular_traversal(const size_t start_index, std::function<bool(const size_t)> callback) const {
           for (size_t i = start_index; i < _index_probe.size(); i++) {
               if (callback(i)) return true;
           }
           //if start_index != 0 then we must circle to the beginning
           for (size_t i = 0; i < start_index; i++) {
               if (callback(i)) return true;
           }

           return false; //the behavior of whatever we're calling did not happen. whether that's good or bad is not this function's responsibility.
       }

       template<class F>
       F _circular_traversal(const size_t start_index, std::function<bool(const size_t, F&)> callback) const {
           F result;
           for (size_t i = start_index; i < _index_probe.size(); i++) {
               if (callback(i, &result)) return std::move(result);
           }
           //if start_index != 0 then we must circle to the beginning
           for (size_t i = 0; i < start_index; i++) {
               if (callback(i, &result)) return std::move(result);
           }

           return std::move(result); //It is assumed that the lambda function returns a F value even when the function returns false. On the last iteration, we return that value.
       }

       void _grow_next() {
           size_t new_size = _policy->next_capacity(_index_probe.size());
           std::vector<std::optional<size_t>> the_bigger_probe(new_size, std::nullopt);

           auto move_index_if_slot_avail = [&](const size_t i, const std::optional<size_t>& old_kv_index) {
               if (!the_bigger_probe[i].has_value()) {
                   the_bigger_probe[i] = std::move(old_kv_index);
                   return true;
               }
               //did not move
               return false;
           };

           //traverse the old vector and rehash (rebalance) the values into it
           for (std::optional<size_t>& old_kv_index : _index_probe) {
               if (old_kv_index.has_value()) {
                   //get the key at old location using current index
                   Key& key = _keys[old_kv_index.value()];

                   //take that key and calculate a new kv-index respecting the new capacity
                   size_t new_hash_index = _key2index(key, new_size);

                   //put the new kv index at the new location. We don't actually move the key.
                   //TODO this loop is similar to _traverse_index_probe
                   for (size_t i = new_hash_index; i < the_bigger_probe.size(); i++) {
                       if (move_index_if_slot_avail(i, old_kv_index)) goto nested_for_escape;
                   }
                   for (size_t i = 0; i < new_hash_index; i++) {
                       if (move_index_if_slot_avail(i, old_kv_index)) goto nested_for_escape;
                   }

                   nested_for_escape:
               }
           }
           _index_probe = the_bigger_probe;
       }

    public:
//construct/copy/destroy

       discrete_map() : _policy(std::make_unique<P>()), _index_probe(_policy->min_capacity(), std::nullopt) {}

//iterators

       class iterator {
           private:
               std::vector<Key>::iterator _key_it;
               std::vector<T>::iterator _val_it;
           public:
               iterator(std::vector<Key>::iterator kter, std::vector<T>::iterator vter) : _key_it(kter), _val_it(vter) {}
               iterator& operator++() {
                   ++_key_it;
                   ++_val_it;
		   return *this;
               }
               iterator& operator--() {
                   --_key_it;
                   --_val_it;
		   return *this;
               }
	       std::pair<const Key&, const T&> operator*() const {
                   return { *_key_it, *_val_it };
	       }
	       bool operator==(const iterator& other) const {
                   return _key_it == other._key_it;
	       }
	       bool operator!=(const iterator& other) const {
                   return _key_it != other._key_it;
	       }
       };

       iterator begin() {
           return iterator(_keys.begin(), _values.begin());
       }

       iterator end() {
           return iterator(_keys.end(), _values.end());
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
           return _keys.size() > 0;
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
                   current_kv_index = _keys.size(); 

                   _keys.push_back(key);
                   _values.push_back(value);

                   //Check if we need to resize and rebalance
                   //TODO possible false positives/negatives due to floating point precision. Ignoring now because inconsequential.
                   if (load_factor() >= _policy->threshold()) {
                       _grow_next();
                   }
                   return true;
               }
               //Check for special case -- overwriting value with existing key. We keep waiting if not same.
               else if (key == _keys[current_kv_index.value()]) {
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
               //TODO there is a faster way involving swapping with the last element then pop_back().
	       //However, this damages the insertion order.
               std::optional<size_t>& current_kv_index = _index_probe.at(i);

               if (current_kv_index.has_value() && key == _keys[current_kv_index.value()]) {
                   _keys.erase(_keys.begin() + current_kv_index.value());
                   _values.erase(_values.begin() + current_kv_index.value());
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

//map operations

       /**
        * Given a key, checks if the corrosponding value is in this map and if so, returns it by reference.
        */
       bool find(const Key& key, T& value) const noexcept {
           auto find_logic = [&](size_t i) {
               const std::optional<size_t>& current_kv_index = _index_probe.at(i);
               if (current_kv_index.has_value() && key == _keys[current_kv_index.value()]) {
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

       auto find(const Key& key) const noexcept {
           auto find_logic = [&](size_t i, iterator& it) {
               const std::optional<size_t>& current_kv_index = _index_probe.at(i);
               if (current_kv_index.has_value() && key == _keys[current_kv_index.value()]) {
                   it = begin() + i;
                   return true;
               }
               //did not find
               it = end();
               return false;
           };

           //driver loop. get a starting index from hash function then move down the indices vector from there. circle to beginning of map if end reached.
           size_t k2i = _key2index(key, _index_probe.size());
           return std::move(_circular_traversal(k2i, find_logic));
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

       float load_factor() const noexcept {
           return static_cast<float>(_keys.size() / _index_probe.size());
       }

       //bool reserve(size_t n) {
       //    if (n < _index_probe.size()) {
       //        return false;
       //    }

       //    size_t size_diff = _index_probe.size() - n;

       //}
};

#endif
