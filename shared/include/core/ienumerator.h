#ifndef IENUMERATOR_H
#define IENUMERATOR_H

#include "ptr/unq_ptr.h"

template <class T>
class IEnumerator {
    public:
        virtual bool move_next() = 0;
        virtual const T& get_current() const = 0;
        virtual void reset() = 0;
        virtual ~IEnumerator() = default;
};

template <class T>
class IEnumerable {
    public:
        virtual UnqPtr<IEnumerator<T>> get_enumerator() const = 0;
        virtual ~IEnumerable() = default;
};

#endif
