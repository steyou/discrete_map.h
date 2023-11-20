#ifndef PARTIAL_MAP_H
#define PARTIAL_MAP_H

#include <vector>
#include <cstddef>
#include <optional>
#include <memory>
#include <functional>

#include "BitwiseMapPolicy.h"
#include "MapPolicy.h"

template<
    typename K,
    typename T,
    typename P = BitwiseMapPolicy
>
class partial_map {
    //private:
    public:
       std::hash<K> _hash;
       std::unique_ptr<MapPolicy> _policy;

       //This vector acts as a container of indices, inspired by unordered_map buckets. It's used to store the shared index of a key-value pair, determined at insertion after the hash function derives the 'bucket'. Unlike the existing STL maps, this vector is what abstracts away probing, resizing, and collision management, keeping these operations separate from the key-value pairs. That allows the kv-pairs can be public const ref& exposed with little risk.
       std::vector<std::optional<size_t>> _index_probe;

       std::vector<K> _keys;
       std::vector<T> _values;

       float _load_factor() const noexcept {
           return _keys.size() / _index_probe.size();
       }

       size_t _key2index(const K& key, const size_t capacity) const noexcept {
           size_t hash_raw = _hash(key);
           return _policy->get_index(hash_raw, capacity);
       } 

       bool _circular_traversal(const size_t start_index, std::function<bool(const size_t)> callback) const noexcept {
           for (size_t i = start_index; i < _index_probe.size(); i++) {
               if (callback(i)) return true;
           }
           //if start_index != 0 then we must circle to the beginning
           for (size_t i = 0; i < start_index; i++) {
               if (callback(i)) return true;
           }

           return false; //the behavior of whatever we're calling did not happen. whether that's good or bad is not this function's responsibility.
       }

       void _resize() {
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
                   K& key = _keys[old_kv_index.value()];

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

    //public:
       partial_map() : _policy(std::make_unique<P>()), _index_probe(_policy->min_capacity(), std::nullopt) {}

       const std::vector<K>& keys() const noexcept {
           return _keys;
       }

       const std::vector<T>& values() const noexcept {
           return _values;
       }

       /**
        * Inserts a key and value in the map.
        */
       bool insert(const K key, const T value) {
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
                   if (_load_factor() > _policy->threshold()) {
                       _resize();
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
        * Given a key, checks if the corrosponding value is in this map and if so, returns it by reference.
        */
       bool find(const K& key, T& value) const noexcept {
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

       bool erase(const K& key) {
           auto erase_pair_if_index_match = [&](size_t i) {
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
};

#endif
