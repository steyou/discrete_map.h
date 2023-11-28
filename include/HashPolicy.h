#ifndef HASH_POLICY_H
#define HASH_POLICY_H

#include <cstddef>

template<class Hash, class Key, class Size=size_t>
class HashPolicy {
    public:
        using hasher = Hash;

    private:
        using size_type = Size; //private because you're supposed to know this elsewhere.

        const std::vector<Key>& _keys;
        const std::function<size_type(size_type, size_type)>& _indexer;

    protected:
        using index_type = std::optional<size_type>;
    public:
        using index_iterator = std::vector<index_type>::iterator;

    protected:
        std::vector<index_type> _indices;

//begin virtual methods

    public:
        virtual index_iterator begin() noexcept = 0;

        virtual index_iterator end() noexcept = 0;

        virtual constexpr float threshold() const noexcept = 0;


//begin concrete

        HashPolicy(size_type initial_capacity, std::function<size_type(size_type,size_type)> indexer)
            : _indices(initial_capacity, std::nullopt),
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

        index_iterator probe(size_type start, std::function<bool(index_type&)> callback) {
            return probe(begin() + start, callback);
        }

        index_iterator probe(index_iterator start, std::function<bool(index_type&)> callback) {
            //loop until some condition happens in the callback.
            for (auto it = start; it != end(); ++it) {
                index_type index = *it;
                if (index.has_value()) {
                    if (callback(index.value())) {
                        return it;
                    }
                    //if false, the callback indicated to continue probing.
                }
                else {
                    //empty slot. under simple open addressing we stop here.
                    return it;
                }
            }
            //If we reach here something's gone wrong. It implies _indices is full. It shouldn't be full. Do an emergency resize, but we're probably going to Davy Jones' Locker.
            rehash(2*_keys.size());
            return probe(start, callback);
        }

        void rehash(size_type next) {

            if (next <= _indices.size()) {
                return;
            }

            //define a new array
            std::vector<index_type> the_bigger_probe(next, std::nullopt);

            //probe for defined values.
            for (auto it = begin(); it != end(); ++it) {

                index_type index = *it;

                if (index.has_value()) {
                    size_type index_for_bigger_probe = this->indexer(
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
