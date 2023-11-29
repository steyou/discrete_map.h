#ifndef LINEAR_PROBER_H
#define LINEAR_PROBER_H

#include <vector>
#include <optional>

template<class Hash, class Key, class Size=size_t>
class linear_prober : public HashPolicy<Hash, Key, Size> {
    public:

        linear_prober(Size initial_capacity, const std::vector<Key, std::allocator<const Key>>& keys, std::function<Size(Size,Size)> indexer)
            : HashPolicy<Hash, Key, Size>(initial_capacity, keys, indexer)
        {}

        ~linear_prober() = default;

    private:
        using index_iterator = typename HashPolicy<Hash, Key, Size>::index_iterator;

    //protected:
        using super_iterator_type = typename HashPolicy<Hash, Key, Size>::template iterator<linear_prober>;
        class iterator : public super_iterator_type {
            private:
                //'import' the equivalent termanology from base class.
                //likely std::optional
                using index_element = typename HashPolicy<Hash, Key, Size>::index_element;

                //we're wrapping a vector iterator. Actually the this->_indices vector.
                index_iterator _begin;
                index_iterator _current;
                index_iterator _end;
            public:
                iterator(index_iterator first, index_iterator x, index_iterator last)
                    : _begin(first), _current(x), _end(last)
                {}
                void increment() {
                    ++_current;
                    if (_current == _end) {
                        _current = _begin;
                    }
                }
                iterator advance(int n) {
                    iterator self = *this;
                    int i = std::distance(self._begin, self._current) + n;

                    if (i >= std::distance(self._begin, self._end)) {
                        self._current = self._begin + (i - std::distance(self._begin, self._end));
                    }
                    else {
                        self._current += i;
                    }
                    return self;
                }
                index_element& dereference() const {
                    return *_current;
                }
                bool equals(const iterator& other) const {
                    return _current == other._current && _begin == other._begin && _end == other._end;
                }
                bool inequal(const iterator& other) const {
                    return !(*this == other);
                }
        };

        super_iterator_type begin_impl() noexcept {
            return iterator(this->_indices.begin(), this->_indices.begin(), this->_indices.end());
        }

        super_iterator_type end_impl() noexcept {
            return iterator(this->_indices.begin(), this->_indices.end(), this->_indices.begin());
        }

        //super_iterator_type begin() noexcept override {
        //    return iterator(this->_indices.begin(), this->_indices.begin(), this->_indices.end());
        //}

        //super_iterator_type end() noexcept override {
        //    return iterator(this->_indices.begin(), this->_indices.end(), this->_indices.begin());
        //}

        constexpr float threshold() const noexcept override {
            return 0.5f;
        }
};

#endif