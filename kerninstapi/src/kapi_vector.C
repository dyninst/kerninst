#include "kapi.h"
#include "common/h/Vector.h"

template<class T>
kapi_vector<T>::kapi_vector()
{
    data = new pdvector<T>();
}

template<class T>
kapi_vector<T>::kapi_vector(const kapi_vector<T> &src)
{
    data = new pdvector<T>(*src.data);
}

template<class T>
kapi_vector<T>::~kapi_vector()
{
    delete data;
}

template<class T>
void kapi_vector<T>::clear()
{
    data->clear();
}

template<class T>
void kapi_vector<T>::push_back(const T &item)
{
    data->push_back(item);
}

template<class T>
unsigned kapi_vector<T>::size() const
{
    return data->size();
}

// Const iterators
template<class T>
kapi_vector<T>::const_iterator kapi_vector<T>::begin() const
{
    return data->begin();
}

template<class T>
kapi_vector<T>::const_iterator kapi_vector<T>::end() const
{
    return data->end();
}

template<class T>
kapi_vector<T>::const_reference kapi_vector<T>::operator[](unsigned i) const
{
    return (*data)[i];
}

// Non-const iterators
template<class T>
kapi_vector<T>::iterator kapi_vector<T>::begin()
{
    return data->begin();
}

template<class T>
kapi_vector<T>::iterator kapi_vector<T>::end()
{
    return data->end();
}

template<class T>
kapi_vector<T>::reference kapi_vector<T>::operator[](unsigned i)
{
    return (*data)[i];
}

