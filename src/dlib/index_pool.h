#ifndef __INDEX_ARRAY_H__
#define __INDEX_ARRAY_H__

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

class IndexPool
{
	typedef	unsigned short INDEX;
	static const INDEX INVALID_SLOT = 0xffff;

public:
	IndexPool()
	{
		m_Pool = NULL;
	}

	~IndexPool()
	{
		assert(m_Pool == NULL);
	}


	/**
	 * fn - Create()
	 * Creates an index pool given a number of elements
	 * Warning! Does currently allocate memory dynamically.
	 */
	void Create(INDEX size)
	{
		assert(m_Pool == NULL);
		assert(size != USHRT_MAX);
		m_Capacity = size;

		m_Pool = (INDEX*)malloc(sizeof(INDEX) * size);

		for (INDEX i=0; i<size; i++)
			m_Pool[i] = i+1;
		m_Pool[size-1] = INVALID_SLOT;

		m_FirstFree = 0;


		m_Allocated = 0;
	}

	/**
	 * fn - Destroy()
	 * Destroys an existing index pool
	 * Does not check if pool is empty
	 */
	void Destroy()
	{
		assert(m_Pool);

		free(m_Pool);
		m_Pool = NULL;
	}

	/**
	 * fn - Alloc()
	 * Allocates an index from pool
	 */
	INDEX Alloc()
	{
		assert(m_Pool);
		assert(m_Allocated < m_Capacity);

		m_Allocated++;

		INDEX node = m_FirstFree;
		m_FirstFree = m_Pool[m_FirstFree];

		return node;
	}

	/**
	 * fn - Free()
	 * Frees an index in pool
	 */
	void Free(INDEX node)
	{
		assert(m_Pool);

		m_Pool[node] = m_FirstFree;
		m_FirstFree = node;
		m_Allocated--;
	}

private:
	INDEX*	m_Pool;
	INDEX	m_FirstFree;
	INDEX	m_Capacity;
	int		m_Allocated;
};


#endif // __INDEX_ARRAY_H__
