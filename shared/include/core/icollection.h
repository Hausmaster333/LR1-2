#ifndef ICOLLECTION_H
#define ICOLLECTION_H

template <class T>
class ICollection {
    public:
        virtual const T& get(int index) const = 0;
        virtual int get_count() const = 0;
        virtual bool contains(const T& item) const = 0;
        virtual ~ICollection() = default;
};

#endif
