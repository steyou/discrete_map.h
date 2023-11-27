#ifndef LINEAR_PROBER_H
#define LINEAR_PROBER_H

#include <vector>
#include <optional>

template<class Hash, class Key, class Size=size_t>
class linear_prober : public HashPolicy<Hash, Key, Size>
{
    public:
        linear_prober() = default;
        ~linear_prober() = default;

    private:

        class iterator {
            private:
                using index_type = typename HashPolicy<Hash, Key, Size>::index_type;
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
                iterator operator+(int n) const {
                    int i = std::distance(_begin, _current) + n;

                    if (i >= std::distance(_begin, _end)) {
                        _current = _begin + (i - std::distance(_begin, _end));
                    }
                    else {
                        _current += i;
                    }

                    return *this;
                }
                index_type& operator*() const {
                    return *_current;
                }
                bool operator==(const iterator& other) const {
                    return _current == other._current;
                }
                bool operator!=(const iterator& other) const {
                    return !(_current == other._current);
                }
        };

        iterator begin() noexcept override {
            return iterator(_indices.begin(), _indices.begin(), _indices.end());
        }

        iterator end() noexcept override {
            return iterator(_indices.begin(), _indices.end(), _indices.begin());
        }

        constexpr float threshold() const noexcept override {
            return 0.7f;
        }
};

#endif