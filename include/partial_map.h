#ifndef PARTIAL_MAP_H
#define PARTIAL_MAP_H

#include <vector>
#include <cstddef>
#include <optional>
#include <memory>

#include "BitwiseMPolicy.h"
#include "MapPolicy.h"

template<
    typename K,
    typename T,
    typename P = BitwiseMPolicy
>
class partial_map {
    private:
       std::hash _hash;
       std::unique_ptr<MapPolicy<>> _policy;

       //This vector is inspired by the concept of buckets in unordered_map. We let the hash function 'arbitrarily' choose where to store the pair, and point to _keys and _values by storing their shared index here. No need for pointers. However, probing and resizing is abstracted away from the kv pairs (because public getter interface) and managed using this vector too.
       std::vector<std::optional<size_t>> _hash2kv_probe;

       std::vector<K> _keys;
       std::vector<T> _values;

       float _load_factor() const noexcept {
           return static_cast<float>(_keys.size() / _hash2kv_probe.size());
       }

       size_t _probe_index(const K& key) const noexcept {
           size_t hash = _hash(key);
	   return _policy.get_index(hash, _hash2kv_probe.size());
       } 

       void _resize() {
           size_t new_size = _policy.next_capacity(_hash2kv_probe.size());

	   //create a new vecttor that will be larger than the one currently.
	   std::vector<std::optional<size_t>> new_h2kvp(new_size, std::nullopt);

	   //traverse the old vector and rehash (rebalance) the values into it
	   for (const auto& kv : _hash2kv_probe) {
               if (kv.hasValue()) {
		   //get the key at old location
                   K& key = _keys[kv.value()];

		   //create a hash index that respects the new capacity
                   size_t new_hash_index = _probe_index(key);

		   //move the old index location into the new location. we never touch the kv pairs or forget where to find them.
		   new_h2kvp[new_hash_index] = std::move(kv.value());
	       }
	   }

	   _hash2kv_probe = newh2kvp;
       }

    public:
       partial_map() : _hash(std::hash<K>{}) {
	   _policy = std::make_unique(P);
           _hash2kv_probe(_policy.min_capacity(), std::nullopt);
       }

       const std:vector<K>& keys() const noexcept {
           return _keys;
       }

       const std::vector<T>& values() const noexcept {
           return _values;
       }

       /**
	* Inserts a key and value in the map.
	*/
       bool insert(const K, const T) noexcept {
	   //we're just using boring linear probing.

	   //loop the _hash2kv_probe starting at the index derived from the hash function
	   for (size_t h  = _probe_index(K); h < _hash2kv_probe.size(); h++) {

	       //loop through until we find an empty slot.
	       size_t& current_kv_index = _hash2kv_probe.at(h);

	       if (current_kv_index.hasValue()) {
		   //Check for special case -- overwriting value with existing key
		   //You can't inline this if statement with the first using && because you want to do nothing if the keys don't match. In that case, the loop is incrementing until one of the return blocks become true.
                   if (K == _keys[current_kv_index.value()]) {
		       _values[c] = T;
		       return true;
		   }
	       }
	       else {
	           //no value here and no duplicate key found - insert here!
		   current_kv_index = _keys.size(); //this is intentionally the first insertion because it's always 1 higher than the last index
                   _keys.push_back(K);
		   _values.push_back(T);

		   //Check if we need to resize and rebalance
		   //TODO possible false positives/negatives due to floating point precision. Ignoring now because inconsequential.
		   if (_load_factor() > _policy.threshold()) {
                       _resize();
		   }

		   return true;
               }
	   }
	   return false;
       }

       /**
	* Given a key, checks if the corrosponding value is in this map and if so, returns it by reference.
	*/
       bool find(const K key, T& value) const noexcept {
	   //driver loop. get a starting index from hash function then move down the indices vector from there.
	   for (size_t h = _probe_index(K); h < _hash2kv_probe.size(); h++) {
	       
	       size_t& current_kv_index = _hash2kv_probe.at(h);

	       if (current_kv_index.hasValue()) {
		   if (K == _keys[current_kv_index.value()]) {
		       T = _values[current_kv_index.value()];
		       return true;
		   }
	       }
	       //didn't find existing key. we assume if there is an empty spot, the key doesn't exist. the insert() function doesn't skip empty slots.
	       else {
                   return false;
               }
	   }
           return false; // unlikely to reach here but good measure. 
       }

       bool erase(const K& key) {
	   //driver loop. get a starting index from hash function then move down the indices vector from there.
	   for (size_t h = _probe_index(K); h < _hash2kv_probe.size(); h++) {
	       
	       size_t& current_kv_index = _hash2kv_probe.at(h);

	       if (current_kv_index.hasValue()) {
		   if (K == _keys[current_kv_index.value()]) {
		       _keys.erase(_keys.begin() + current_kv_index.value());
		       _values.erase(_values.begin() + current_kv_index.value());
		       current_kv_index.reset();
		       return true;
		   }
	       }
	       //didn't find existing key. we assume if there is an empty spot, the key doesn't exist. the insert() function doesn't skip empty slots.
	       else {
                   return false;
               }
	   }
           return false; // unlikely to reach here but good measure. 
       }
}

#endif
