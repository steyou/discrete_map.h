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

       size_t _key2index(const K& key, size_t capacity) const noexcept {
           size_t hash_raw = _hash(key);
           return _policy->get_index(hash_raw, capacity);
       } 

       void _resize() {
           size_t new_size = _policy->next_capacity(_index_probe.size());
	   std::vector<std::optional<size_t>> the_bigger_probe(new_size, std::nullopt);

	   //skip reading this declaration until you read the code below. Then this will make sense.
	   auto move_index_if_slot_avail = [&](size_t i, const std::optional<size_t>& kv_pair_index) -> bool {
               if (!the_bigger_probe[i].has_value()) {
                   the_bigger_probe[i] = std::move(kv_pair_index);
                   return true;
               }
               return false;
	   };

	   //traverse the old vector and rehash (rebalance) the values into it
           for (std::optional<size_t>& kv_pair_index : _index_probe) {
               if (kv_pair_index.has_value()) {
                   //get the key at old location
                   K& key = _keys[kv_pair_index.value()];

                   //take that key and calculate a new kv-index for the new capacity
                   size_t new_hash_index = _key2index(key, new_size);

		   //do collision detection again like we did in insert(). start at the index given by the hash and circle around if we reach the end.
		   for (size_t i = new_hash_index; i < the_bigger_probe.size(); i++) {
                       if (move_index_if_slot_avail(i, kv_pair_index)) {
                           goto nested_for_escape;
                       }
                   }
		   for (size_t i = 0; i < new_hash_index; i++) {
                       if (move_index_if_slot_avail(i, kv_pair_index)) {
                           goto nested_for_escape;
                       }
                   }
		   //TODO: if we reach here then something's gone wrong. We can't rehash because the probe is somehow full. unlikely but should provide some logic for that.
		   
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
           //we're just using boring linear probing.

           auto ins_fn = [&](size_t i) -> bool {
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

               return false;
           };

           //loop the _index_probe starting at the index derived from the hash function. go to index 0 if index_probe is full to the end. stop when we're back at where we started.
           size_t probe_ix = _key2index(key, _index_probe.size());
           for (size_t h = probe_ix; h < _index_probe.size(); h++) {
               //if (ins_fn(h, key, value, _index_probe, _keys, _values, _policy, _load_factor, _resize)) return true;
               if (ins_fn(h)) return true;
           }
           for (size_t h = 0; h < probe_ix; h++) {
               //if (ins_fn(h, key, value, _index_probe, _keys, _values, _policy, _load_factor, _resize)) return true;
               if (ins_fn(h)) return true;
           }

           return false; //unlikely but good measure. some maps would infinite loop instead.
       }

       /**
        * Given a key, checks if the corrosponding value is in this map and if so, returns it by reference.
        */
       bool find(const K key, T& value) const noexcept {
           auto find_logic = [&](size_t i) -> bool {
               const std::optional<size_t>& current_kv_index = _index_probe.at(i);
               if (current_kv_index.has_value() && key == _keys[current_kv_index.value()]) {
                   value = _values[current_kv_index.value()];
                   return true;
               }
               return false;
           };

           //driver loop. get a starting index from hash function then move down the indices vector from there. circle to beginning of map if end reached.
           size_t k2i = _key2index(key, _index_probe.size());
           for (size_t i = k2i; i < _index_probe.size(); i++) {
               if (find_logic(i)) return true;
           }
           for (size_t i = 0; i < k2i; i++) {
               if (find_logic(i)) return true;
           }
           return false; // unlikely to reach here but good measure. 
       }

       bool erase(const K& key) {
           auto erase_pair_if_index_match = [&](size_t i) -> bool {
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
           for (size_t i = k2i; i < _index_probe.size(); i++) {
               if (erase_pair_if_index_match(i)) return true;
           }
           for (size_t i = 0; i < k2i; i++) {
               if (erase_pair_if_index_match(i)) return true;
           }
           return false; // unlikely to reach here but good measure. 
       }
};

#endif
