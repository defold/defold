
#ifndef DM_EVENT_H
#define DM_EVENT_H

#include <stdint.h>

namespace dmEvent
{
    /*!
    *   \struct Event
    *   \ingroup dmEvent
    *   \brief Event data desc used at dispatch callback. When an event is posted,
    *    the actual object is copied into the sockets internal buffer.
    */
    class Event
    {
		public:
        uint32_t m_ID;						//! Unique ID of event
        uint32_t m_DataSize;				//! Size of userdata in bytes 
		class Event *m_Next;				//! Ptr to next event (or 0 if last)
		uint8_t  m_Data[0];					//! userdata
    };

    /*
    * \fn Register
    * \param event_id Unique ID of the event to register
    * \param event_data_bytesize Size of data sent with this event, in bytes
    * \brief You must register an event before you can post it or it won't register.
    */
    void Register(uint32_t event_id, uint32_t event_data_bytesize);

    /*
    * \fn Unregister
    * \param event_id Unique if of the event to unregister
    * \brief You should unregister obsolete events to regain used resources.
    */
    void Unregister(uint32_t event_id);

    /*
    * \fn Post
    * \param socket_id Socket ID to to post the event to
    * \param event_id Event unique id reference
    * \param event_data Event data reference
    * \brief Posts an event to a socket
    */
    void Post(uint32_t socket_id, uint32_t event_id, void *event_data);

    /*
    * \fn Broadcast
    * \param event_id Unique ID of the event
    * \param event_data Event data reference
    * \brief Posts an Event to all existing sockets
    */
    void Broadcast(uint32_t event_id, void *event_data);

    /*
    * \fn Dispatch
    * \param Socket ID of the socket of which events to dispatch.
    * \param Dispatch_function the callback function that will be called for each event
	* \      dispatched. The callbacks parameters contains a pointer to a unique Event
	* \      for this post and user_ptr is the same as Dispatch called user_ptr.
    * \brief When dispatched, the events are considered destroyed.
    * \return Number of dispatched events
    */
	uint32_t Dispatch(uint32_t socket_id, void(*dispatch_function)(dmEvent::Event *event_object, void* user_ptr), void* user_ptr);

    /*
    * \fn CreateSocket
    * \param buffer_byte_size The max size of the buffer containing the eventdata pending dispatch.
    * \brief The ID returned is used by event functions to reference the created socket.
    * \return 0 if failed, 1 if successful, 2 if socket_id already exists
    */
    uint32_t CreateSocket(uint32_t socket_id, uint32_t buffer_byte_size);

    /*
    * \fn DestroySocket
    * \param socket ID of the socket to destroy
    * \brief Pending events in a socket is not called when a socket is destroyed. Be sure
    *  to call dispatch prior to destroying if this is a desired path.
    * \return false if socket_id didn't exist.
    */
    bool DestroySocket(uint32_t socket_id);

};

#endif // DM_EVENT_H

