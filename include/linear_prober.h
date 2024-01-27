#ifndef LINEAR_PROBER_H
#define LINEAR_PROBER_H

#include <functional>
#include <memory>
#include <vector>
#include <optional>

template<class SizeTraits>
class linear_prober {
    private:
        using size_type = typename SizeTraits::size_type;
        using indices_type = typename SizeTraits::indices_type;

        using indices_collection_type = std::vector<indices_type>;

        template<bool is_const>
        class iterator_impl {
            private:
                using inds_cltn_constness_type = typename std::conditional<is_const,
                    const indices_collection_type,
                    indices_collection_type
                >::type;

                using inds_t_constness_type = typename std::conditional<is_const,
                    const indices_type,
                    indices_type
                >::type;

                size_type _current;
                inds_cltn_constness_type* _indices;

            public:
                iterator_impl(size_type current, inds_cltn_constness_type& indices)
                    : _current(current), _indices(&indices)
                {}
                void operator++() {
                    ++_current;
                    if (_current >= _indices->size()) {
                        _current = 0;
                    }
                }
                iterator_impl<is_const> operator+(size_type n) const {
                    size_type i = _current + n;
                    return i > _indices->size()
                        ? iterator_impl<is_const>(i - _indices->size(), *_indices)
                        : iterator_impl<is_const>(i, *_indices);
                }
                inds_t_constness_type& operator*() const {
                    return (*_indices)[_current];
                }
                bool operator==(const iterator_impl& other) const {
                    return _current == other._current;
                }
                bool operator!=(const iterator_impl& other) const {
                    return !(*this == other);
                }
        };

    public:
        using const_iterator = iterator_impl<true>;
        using iterator = iterator_impl<false>;

        iterator begin(indices_collection_type& indices) noexcept {
            return iterator(0, indices);
        }

        iterator end(indices_collection_type& indices) noexcept {
            return iterator(indices.size(), indices);
        }

        const_iterator cbegin(const indices_collection_type& indices) const noexcept {
            return const_iterator(0, indices);
        }

        const_iterator cend(const indices_collection_type& indices) const noexcept {
            return const_iterator(indices.size(), indices);
        }

        constexpr float threshold() const noexcept {
            return 0.5f;
        }
};

#endif
