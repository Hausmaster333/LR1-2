#ifndef IDICTIONARY_H
#define IDICTIONARY_H

#include "core/ienumerator.h"
#include "core/key_value.h"

template <class K, class V>
class IDictionary : public IEnumerable<KeyValue<K, V>> {
    public:
        virtual int get_count() const = 0;
        virtual bool contains_key(const K& key) const = 0;

        virtual const V& get(const K& key) const = 0;
        virtual V& get(const K& key) = 0;

        virtual void add(K key, V value) = 0;
        virtual void remove(const K& key) = 0;
};

#endif
