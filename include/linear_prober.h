#ifndef LINEAR_PROBER_H
#define LINEAR_PROBER_H

#include <optional>

template<class Hash,
         class Size = size_t>
class linear_prober : public HashPolicy<Hash, Size> {
    private:
        using super = HashPolicy<Hash, Size>
    public:
        using hasher = typename super::hasher;
        using size_type = typename super::size_type;

        linear_prober(size_type initial_capacity)
            : _indices(initial_capacity, std::nullopt)
        {}

        size_type size() const noexcept {
            return _indices.size();
        }

        class iterator {
            private:
                using IndexIterator = typename std::vector<index_type>::iterator; 
                IndexIterator _current;
                IndexIterator _begin;
                IndexIterator _end;
            public:
                iterator(IndexIterator first, IndexIterator start, IndexIterator last)
                    : _begin(first), _current(start), _end(last)
                {}
                iterator& operator++() {
                    if (++_current == _end) {
                        _current = _begin;
                    }
                    return *this;
                }
                index_type& operator*() const {
                    return *_current;
                }
                bool operator==(iterator& other) const {
                    return _current == other;
                }
                bool operator!=(iterator& other) const {
                    return !(_current == other);
                }
        }

        iterator begin() noexcept override {
            return iterator(_indices.begin(), _indices.begin(), _indices.end());
        }

        iterator end() noexcept override {
            return iterator(_indices.begin(), _indices.end(), _indices.begin());
        }

        hasher hash_function() const override {
            return Hash();
        }

        constexpr float threshold() const noexcept override {
            return 0.7f;
        }

        [[nodiscard]] float load_factor(size_type num_elements) const noexcept override {
            return static_cast<float>(num_elements) / static_cast<float>(_indices.size());
        }

        std::optional<std::pair<iterator, bool>> probe(size_type start, std::function<bool(index_type&)> callback) const override {
            //loop until some condition happens in the callback.
            for (auto it = begin() + start; it != end(); ++it) {
                std::optional<size_type> index = *it;
                if (index.has_value()) {
                    if (callback(index.value())) return std::make_pair(it, true);
                }
                else {
                    //empty slot
                    return std::make_pair(it, false);
                }
            }
            //if we reach here something's gone wrong. It implies _indices is full, which is undefinend behavior.
            return std::nullopt; //the behavior of whatever we're calling did not happen. whether that's good or bad is not this function's responsibility.
        }

        template<class R>
        std::optional<std::tuple<iterator, bool R>> probe(size_type start, std::function<bool(index_type&, R*)> callback) const override {
            //loop until some condition happens in the callback.
            R result = nullptr;
            for (auto it = begin() + start; it != end(); ++it) {
                std::optional<size_type> index = *it;
                if (index.has_value()) {
                    if (callback(index, R)) return std::make_tuple(it, true, result);
                }
                else {
                    return std::make_tuple(it, false, result);
                }
            }
            //if we reach here something's gone wrong. It implies _indices is full, which is undefinend behavior.
            return std::nullopt; //the behavior of whatever we're calling did not happen. whether that's good or bad is not this function's responsibility.
        }

        void rehash(size_type n, std::vector<const Key>& keys, std::function<size_type>(size_type, size_type) indexer) {
            //TODO: benchmark whether calling reserve() at the end is beneficial

            if (n <= _indices.size()) {
                return;
            }

            //define a new array
            std::vector<index_type> the_bigger_probe(n, std::nullopt);

            for (auto it = begin(); it != end(); ++it) {

                std::optional<size_type> index = *it;

                if (index.has_value()) {
                    size_type index_for_bigger_probe = indexer(
                        hash_function()(_keys[index.value()]),
                        n
                    );
                    the_bigger_probe[index_for_bigger_probe] = std::move(old_kv_index);
                }

            }

            _indices = the_bigger_probe;
        }

    private:
        std::vector<index_type> _indices;
}

#endif