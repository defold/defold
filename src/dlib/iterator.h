
#ifndef DLIB_ITERATOR_H
#define DLIB_ITERATOR_H

#include <stdint.h>
#include <assert.h>

/**
 * Iterator class with bound checking.
 */
template <typename T>
struct Iterator
{
    /**
     * Creates an iterator with initial value and range.
     * @param p Initial value
     * @param min Minimum value
     * @param max Maximum value
     */
    Iterator(T* p, T* min, T* max) :
    m_P(p),
    m_Min(min),
    m_Max(max)
    {
        assert(m_P >= m_Min && m_P <= m_Max);
    }

    // operator +
    T* operator+(int32_t val)
    {
        assert(m_P+val < m_Max);
        return m_P+val;
    }

    // operator -
    T* operator-(int32_t val)
    {
        assert(m_P-val >= m_Min);
        return m_P-val;
    }

    // operator ++
    void operator++(int32_t)
    {
        assert(m_P < m_Max);
        m_P++;
    }

    // operator +=
    void operator+=(int32_t val)
    {
        assert(m_P+val < m_Max);
        m_P+=val;
    }

    // operator --
    void operator--(int32_t)
    {
        assert(m_P >= m_Min);
        m_P--;
    }

    // operator -=
    void operator-=(int32_t val)
    {
        assert(m_P-val >= m_Min);
        m_P-=val;
    }

    // != operator ref
    bool operator!=(Iterator<T>& other)
    {
        return m_P != other.m_P;
    }

    // != operator
    bool operator!=(T* other)
    {
        return *m_P != *other;
    }

    // == operator ref
    bool operator==(Iterator<T>& other)
    {
        return m_P == other.m_P;
    }

    // == operator
    bool operator==(T* other)
    {
        return *m_P == *other;
    }

    // > operator
    bool operator> (Iterator<T>& other)
    {
        return m_P > other.m_P;
    }

    // >= operator
    bool operator>=(Iterator<T>& other)
    {
        return m_P >= other.m_P;
    }

    // < operator
    bool operator<(Iterator<T>& other)
    {
        return m_P < other.m_P;
    }

    // <= operator
    bool operator<=(Iterator<T>& other)
    {
        return m_P <= other.m_P;
    }

    // operator *
    T operator*()
    {
        return *m_P;
    }

    T* m_P;
    T* m_Min;
    T* m_Max;
};

#endif // DLIB_ITERATOR_H
