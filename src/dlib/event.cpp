
#include <string.h>
#include <assert.h>
#include "hashtable.h"
#include "event.h"
#include "atomic.h"

namespace dmEvent
{

	struct SEventSocket
	{
		uint8_t			*m_Queue;				//! Datablock containing the event (all inclusive)
		uint32_t		m_QueueCapacity;		//! Capacity in bytes of m_Queue
		uint32_atomic_t	m_QueueWritePos;		//! Current offset to event writepos
		uint32_atomic_t	m_QueueHeader;			//! Current offset to head of queue
		uint32_atomic_t	m_QueueTail;			//! Current offset to tail of queue
	};

	struct SEventInfo
	{
		uint32_t m_ID;			//! Unique ID of event
		uint32_t m_DataSize;	//! Size of data in bytes
	};


	dmHashTable<uint32_t, SEventInfo>	m_RegisteredEvents; //(512);
	dmHashTable<uint32_t, SEventSocket>	m_Sockets; //(32);


	void Register(uint32_t event_id, uint32_t event_data_bytesize)
	{
		static bool hash_initialized = false;
		if(!hash_initialized)
		{
			hash_initialized = true;
			m_RegisteredEvents.SetCapacity(512, 4096);
		}
		if(!m_RegisteredEvents.Empty() && m_RegisteredEvents.Get(event_id))
			return;

		SEventInfo event_info;
		event_info.m_ID = event_id;
		event_info.m_DataSize = event_data_bytesize;
		m_RegisteredEvents.Put(event_id, event_info);
	}


	void Unregister(uint32_t event_id)
	{
		m_RegisteredEvents.Erase(event_id);
	}


	uint32_t CreateSocket(uint32_t socket_id, uint32_t buffer_byte_size)
	{
		static bool hash_initialized = false;
		if(!hash_initialized)
		{
			hash_initialized = true;
			m_Sockets.SetCapacity(32, 1024);
		}

		if(m_Sockets.Get(socket_id))
			return 2;

		SEventSocket socket;

		buffer_byte_size += sizeof(Event);

		socket.m_Queue = new uint8_t[buffer_byte_size];
		if(!socket.m_Queue)
			return 0;
		socket.m_QueueCapacity = buffer_byte_size;

		Event *header = (Event *) socket.m_Queue;
		memset(header, 0, sizeof(Event));
		header->m_Next = 0xffffffff;
		socket.m_QueueWritePos = sizeof(Event);
		socket.m_QueueHeader = 0;
		socket.m_QueueTail = 0;

        m_Sockets.Put(socket_id, socket);
		return 1;
	}


	bool DestroySocket(uint32_t socket_id)
	{
		SEventSocket *socket = m_Sockets.Get(socket_id);
		if(!socket)
			return false;

		delete[] socket->m_Queue;
		m_Sockets.Erase(socket_id);
		return true;
	}


	void Post(uint32_t socket_id, uint32_t event_id, void *event_data)
	{
		// get socket and event
		SEventSocket *socket = 	m_Sockets.Get(socket_id);
		if(!socket)
		{
			assert(false && "Trying to post event to invalid socket id");
			return;
		}

		SEventInfo *event_info = m_RegisteredEvents.Get(event_id);
		if(!event_info)
		{
			assert(false && "Trying to post unregistered event");
			return;
		}

		// get a valid bufferpointer to store eventdata to
		uint32_t data_size = sizeof(Event) + event_info->m_DataSize;
		uint32_t eventdata_capacity = socket->m_QueueCapacity;
		assert((data_size <= eventdata_capacity) && "Eventdata is larger than eventdata buffersize");
		volatile uint32_t offset = dmAtomicAdd32(&socket->m_QueueWritePos, data_size) % eventdata_capacity;

		// check dataqueue wrap - if databuffer end is intersecting our segment, use next segment
        uint32_t fence = socket->m_QueueHeader;
        if((offset + data_size) > eventdata_capacity)
		{
			if(fence >= offset)
			{
				assert(false && "buffer overrun");
			}

			// insert dummy "fence wrap" object
			uint8_t *segment = &socket->m_Queue[offset];
			Event *event_object = (Event *) segment;
			event_object->m_ID = 0xffffffff;
			event_object->m_DataSize = data_size - sizeof(Event);
			event_object->m_Next = 0xffffffff;

			uint32_t current_tail_offset = dmAtomicStore32(&socket->m_QueueTail, offset);
			uint8_t *prev_segment = &socket->m_Queue[current_tail_offset];
			Event *prev_event_object = (Event *) prev_segment;
			prev_event_object->m_Next = offset;

			offset = dmAtomicAdd32(&socket->m_QueueWritePos, data_size) % eventdata_capacity;

			if((offset + data_size) > fence)
			{
				assert(false && "buffer overrun");
			}
		}
		else
		{
			// check buffer overrun
			if((offset <= fence) && ((offset+data_size) > fence))
			{
				assert(false && "buffer overrun");
			}
		}


		// set up data in segment
		uint8_t *segment = &socket->m_Queue[offset];
		Event *event_object = (Event *) segment;
		event_object->m_ID = event_id;
		event_object->m_DataSize = data_size - sizeof(Event);
		event_object->m_Next = 0xffffffff;
		if(event_data)
		{
			memcpy(segment + sizeof(Event), event_data, data_size - sizeof(Event));
		}

		// swap current tail with this segment and set current tail->next to this (single-link list)
		uint32_t current_tail_offset = dmAtomicStore32(&socket->m_QueueTail, offset);
		uint8_t *prev_segment = &socket->m_Queue[current_tail_offset];
		Event *prev_event_object = (Event *) prev_segment;
		prev_event_object->m_Next = offset;
	}


	void Broadcast(uint32_t event_id, void *event_data)
	{
		// get event and validate id
		SEventInfo *event_info = m_RegisteredEvents.Get(event_id);
		if(!event_info)
			return;

		assert(false && "Not implemented yet");
		// .. loop through sockets, getting id
		// PostEvent(socket_id, event_id, event_data);
		// ..
	}


	uint32_t Dispatch(uint32_t socket_id, void(*dispatch_function)(dmEvent::Event *event_object, void* user_ptr), void* user_ptr)
	{
		SEventSocket *socket = 	m_Sockets.Get(socket_id);
		if(!socket)
		{
			assert(false && "Invalid socket parameter");
			return 0;
		}

		uint32_t write_offset = socket->m_QueueWritePos;
		uint32_t header_offset = socket->m_QueueHeader;
		uint32_t tail_offset = socket->m_QueueTail;
		uint32_t eventdata_capacity = socket->m_QueueCapacity;
		uint8_t *segment = &socket->m_Queue[header_offset];
		Event *event_object = (Event *) segment;

		uint32_t offset_base = header_offset;
		uint32_t offset = event_object->m_Next;

		// dispatch loop
		uint32_t dispatch_count = 0;
		while(offset != 0xffffffff && offset != write_offset )
		{
			segment = &socket->m_Queue[offset];
			event_object = (Event *) segment;

			if(event_object->m_ID != 0xffffffff)
				dispatch_function(event_object, user_ptr);
            else
                dispatch_count--;

            header_offset = offset;
			offset = event_object->m_Next;
			dispatch_count ++;
		}

		dmAtomicStore32(&socket->m_QueueHeader, header_offset);

		return dispatch_count;
	}







};