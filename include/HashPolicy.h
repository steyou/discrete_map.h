#ifndef HASH_POLICY_H
#define HASH_POLICY_H

#include <cstddef>

template<class Hash, class Key, class Size=size_t>
class HashPolicy {
    private:
        //these two are private because you're supposed to know this elsewhere. I want to encourage that.
        using hasher = Hash;
        using size_type = Size;
    public:
        using index_element = std::optional<size_type>;
        using index_iterator = std::vector<index_element>::iterator;

        virtual constexpr float threshold() const noexcept = 0;
    protected:
        std::vector<index_element> _indices;

        //We need a reference to the keys for rehashing. Conceeding to this allows resizing under otherwise errornous cases because table full.
        const std::vector<Key, std::allocator<const Key>>& _keys;

        //Static copy of GrowthPolicy::get_index
        std::function<size_type(size_type, size_type)> _indexer;

        template<class Impl>
        class iterator {
            //using CRTP to ensure consistent interface with STL.
            public:
                virtual ~iterator() = default;

                Impl& operator++() {
                    return static_cast<const Impl*>(this)->increment();
                }

                Impl operator+(int n) {
                    return static_cast<const Impl*>(this)->advance(n);
                }

                index_element& operator*() const {
                    return static_cast<const Impl*>(this)->dereference();
                }

                bool operator==(const Impl& other) const {
                    return static_cast<const Impl*>(this)->equals(other);
                }

                bool operator!=(const Impl& other) const {
                    return static_cast<const Impl*>(this)->inequal(other);
                }
        };

        template<class Impl>
        iterator<Impl> begin() noexcept {
            return static_cast<const Impl*>(this)->begin_impl();
        }

        template<class Impl>
        iterator<Impl> end() noexcept {
            return static_cast<const Impl*>(this)->end_impl();
        }

        //virtual iterator end() noexcept = 0;

        //virtual std::unique_ptr<iterator> begin() noexcept = 0;

        //virtual std::unique_ptr<iterator> end() noexcept = 0;

//begin concrete
    public:

        HashPolicy(size_type initial_capacity, const std::vector<Key, std::allocator<const Key>>& keys, std::function<size_type(size_type,size_type)> indexer)
            : _indices(initial_capacity, std::nullopt),
              _keys(keys),
              _indexer(indexer)
        {}

        ~HashPolicy() = default;

        size_type size() const noexcept {
            return _indices.size();
        }

        void clear() noexcept {
            _indices.clear();
        }

        hasher hash_function() const {
            return Hash();
        }

        [[nodiscard]] float load_factor(size_type num_elements) const noexcept {
            return static_cast<float>(num_elements) / static_cast<float>(_indices.size());
        }

        index_element& probe(size_type start, std::function<bool(size_type&)> callback) {
            return probe(begin() + start, callback);
        }

        index_element& probe(index_iterator start, std::function<bool(size_type&)> callback) {
            //loop until some condition happens in the callback.
            for (auto it = start; it != end(); ++it) {
                index_element& index = *it;
                if (index.has_value()) {
                    if (callback(index.value())) {
                        return index;
                    }
                    //if false, the callback indicated to continue probing.
                }
                else {
                    //empty slot. under simple open addressing we stop here.
                    return index;
                }
            }
            //If we reach here something's gone wrong. It implies _indices is full. It shouldn't be full. Do an emergency resize, but we're probably going to Davy Jones' Locker.
            //TODO the start becomes invalidated
            rehash(2*_keys.size());
            return probe(start, callback);
        }

        void rehash(size_type next) {

            if (next <= _indices.size()) {
                return;
            }

            //define a new array
            std::vector<index_element> the_bigger_probe(next, std::nullopt);

            //probe for defined values.
            for (auto it = begin(); it != end(); ++it) {

                index_element index = *it;

                if (index.has_value()) {
                    size_type index_for_bigger_probe = this->_indexer(
                        hash_function()(_keys[index.value()]),
                        next
                    );
                    the_bigger_probe[index_for_bigger_probe] = std::move(index);
                }

            }

            _indices = the_bigger_probe;
        }

};

#endif
