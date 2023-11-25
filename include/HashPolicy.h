#ifndef HASH_POLICY_H
#define HASH_POLICY_H

#include <cstddef>

template<class Hash, class Size>
class HashPolicy {
public:
    using hasher = Hash;
    using size_type = Size;

    virtual size_type size() const noexcept = 0;

    template<class I>
    virtual I begin() const noexcept = 0;

    template<class I>
    virtual I end() const noexcept = 0;

    virtual hasher hash_function() const = 0;

    virtual constexpr float threshold() const noexcept = 0;

    virtual float load_factor(size_type num_elements) const noexcept = 0;

    template<class Key>
    virtual std::optional<std::pair<Iterator, bool> probe(size_type start, std::function<bool>(index_type&) callback) = 0;

    template<class Key>
    virtual std::optional<std::tuple<Iterator, bool, R>> probe(size_type start, std::function<bool>(index_type&, R*) callback) = 0;

    template<class Key>
    virtual void rehash(size_type n, std::vector<const Key>& keys, std::function<size_type>(size_type, size_type) indexer) = 0;
};

#endif