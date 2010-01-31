
//#include <string.h>
#include <assert.h>
#include "event.h"
#include "atomic.h"
#include "hashtable.h"

namespace dmEvent
{

	struct SEventSocket
	{
		Event *m_Header;
		Event *m_Tail;
	};

	struct SEventInfo
	{
		uint32_t m_ID;			//! Unique ID of event
		uint32_t m_DataSize;	//! Size of data in bytes
	};


	dmHashTable<uint32_t, SEventInfo>	m_RegisteredEvents;
	dmHashTable<uint32_t, SEventSocket>	m_Sockets;


	void Register(uint32_t event_id, uint32_t event_data_bytesize)
	{
		static bool hash_initialized = false;
		if(!hash_initialized)
		{
			hash_initialized = true;
			m_RegisteredEvents.SetCapacity(512,4096);
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
		socket.m_Header = 0;
		socket.m_Tail = 0;
		m_Sockets.Put(socket_id, socket);
		return 1;
	}


	bool DestroySocket(uint32_t socket_id)
	{
		SEventSocket *socket = m_Sockets.Get(socket_id);
		if(!socket)
			return false;
	
		//assert(socket->m_Header == 0 && "Destroying socket with nondispatched events, memory leak..");
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

		uint32_t data_size = sizeof(Event) + event_info->m_DataSize;
		Event *new_event = (Event *) new uint8_t[data_size];
		new_event->m_ID = event_id;
		new_event->m_DataSize = event_info->m_DataSize;
		new_event->m_Next = 0;
		memcpy(&new_event->m_Data[0], event_data, event_info->m_DataSize);


		// mutex lock

		if(!socket->m_Header)
		{
			socket->m_Header = new_event;
			socket->m_Tail = new_event;
		}
		else
		{
			socket->m_Tail->m_Next = new_event;
			socket->m_Tail = new_event;
		}

		// mutex unlock
	}


	void Broadcast(uint32_t event_id, void *event_data)
	{
		assert(false && "Not implemented yet");
		// .. loop through sockets, getting id
		// PostEvent(socket_id, event_id, event_data);
		// ..

		// get event and validate id
		SEventInfo *event_info = m_RegisteredEvents.Get(event_id);
		if(!event_info)
			return;
	}


	uint32_t Dispatch(uint32_t socket_id, void(*dispatch_function)(dmEvent::Event *event_object, void* user_ptr), void* user_ptr)
	{
		SEventSocket *socket = 	m_Sockets.Get(socket_id);
		if(!socket)
		{
			assert(false && "Invalid socket parameter");
			return 0;
		}

		if(!socket->m_Header)
			return 0;
		uint32_t dispatch_count = 0;

		// mutex lock

		Event *event_object = socket->m_Header;
		Event *last_event_object = socket->m_Tail;
		socket->m_Header = 0;
		socket->m_Tail = 0;

		// mutex unlock

		while(event_object)
		{
			dispatch_function(event_object, user_ptr);
			uint8_t *old_object = (uint8_t *) event_object;
			event_object = event_object->m_Next;
			delete[] old_object;
			dispatch_count++;
		}

		return dispatch_count;
	}

};

