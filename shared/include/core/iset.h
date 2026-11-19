#ifndef ISET_H
#define ISET_H

#include "core/ienumerator.h"

template <class K>
class ISet : public IEnumerable<K> {
    public:
        virtual int get_count() const = 0;
        virtual bool contains(const K& key) const = 0;
        virtual bool add(K key) = 0;
        virtual bool remove(const K& key) = 0;
};

#endif
