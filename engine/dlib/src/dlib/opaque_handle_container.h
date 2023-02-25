
#ifndef DM_OPAQUE_HANDLE_CONTAINER_H
#define DM_OPAQUE_HANDLE_CONTAINER_H

typedef uint32_t HOpaqueHandle;

static const HOpaqueHandle INVALID_OPAQUE_HANDLE = 0xFFFFFFFF;

template <typename T>
class OpaqueHandleContainer
{
public:
    OpaqueHandleContainer()
    : m_Objects(0)
    , m_Capacity(0)
    , m_Version(0)
    {}

    OpaqueHandleContainer(uint32_t inital_capacity)
    : OpaqueHandleContainer()
    {
        Allocate(inital_capacity);
    }

    bool Allocate(uint32_t count)
    {
        if (m_Objects == 0x0)
        {
            m_Objects        = (T**) malloc(count * sizeof(sizeof(T*)));
            m_ObjectVersions = (uint16_t*) malloc(count * sizeof(uint16_t));
        }
        else
        {
            uint32_t new_capacity = m_Capacity + count;
            m_Objects             = (T**) realloc(m_Objects, new_capacity * sizeof(T*));
            m_ObjectVersions      = (uint16_t*) realloc(m_ObjectVersions, new_capacity * sizeof(uint16_t));
        }

        memset(m_Objects + m_Capacity, 0, count * sizeof(T*));
        memset(m_ObjectVersions + m_Capacity, 0, count * sizeof(uint16_t));

        m_Capacity += count;

        return m_Objects != 0x0 && m_ObjectVersions != 0x0;
    }

    T* Get(HOpaqueHandle handle)
    {
        if (handle == 0)
        {
            return 0;
        }

        uint32_t version = handle >> 16;
        uint32_t index   = handle & 0xFFFF;
        T* entry         = GetByIndex(index);

        if (entry == 0 || m_ObjectVersions[index] != version)
        {
            return 0;
        }

        return entry;
    }

    HOpaqueHandle Put(T* obj)
    {
        uint32_t index = GetFirstEmptyIndex();
        if (index == INVALID_OPAQUE_HANDLE)
        {
            return INVALID_OPAQUE_HANDLE;
        }

        m_Version++;
        if (m_Version == 0)
        {
            // Set to 1 to avoid potentially producing a 0 handle
            m_Version = 1;
        }

        m_ObjectVersions[index] = m_Version;
        m_Objects[index]        = obj;

        return m_Version << 16 | index;
    }

    void Release(HOpaqueHandle handle)
    {
        uint32_t index = handle & 0xFFFF;
        if (!Get(handle))
        {
            return;
        }
        m_Objects[index]        = 0;
        m_ObjectVersions[index] = 0;
    }

    inline uint32_t Capacity()
    {
        return m_Capacity;
    }

    bool Full()
    {
        return GetFirstEmptyIndex() != INVALID_OPAQUE_HANDLE;
    }

private:
    T**       m_Objects;
    uint16_t* m_ObjectVersions;
    uint32_t  m_Capacity;
    uint16_t  m_Version;

    inline T* GetByIndex(uint32_t i)
    {
        assert(i < m_Capacity);
        return m_Objects[i];
    }

    uint32_t GetFirstEmptyIndex()
    {
        for (int i = 0; i < m_Capacity; ++i)
        {
            if (m_Objects[i] == 0)
            {
                return i;
            }
        }
        return INVALID_OPAQUE_HANDLE;
    }

    OpaqueHandleContainer(const OpaqueHandleContainer<T>&) {}
    void operator =(const OpaqueHandleContainer<T>&) {}
    void operator ==(const OpaqueHandleContainer<T>&) {}
};

#endif // DM_OPAQUE_HANDLE_CONTAINER_H
