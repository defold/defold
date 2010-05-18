#ifndef DM_EVENT_H
#define DM_EVENT_H

#include <stdint.h>

namespace dmEvent
{
    /**
     * @brief Event data desc used at dispatch callback. When an event is posted,
     *        the actual object is copied into the sockets internal buffer.
     */
    class Event
    {
		public:
        uint32_t m_ID;						//! Unique ID of event
        uint32_t m_DataSize;				//! Size of userdata in bytes 
		class Event *m_Next;				//! Ptr to next event (or 0 if last)
		uint8_t  m_Data[0];					//! userdata
    };

    /**
     * Register. You must register an event before you can post it or it won't register.
     * @param event_id Unique ID of the event to register
     * @param event_data_bytesize Size of data sent with this event, in bytes
     */
    void Register(uint32_t event_id, uint32_t event_data_bytesize);

    /**
     * Unregister. You should unregister obsolete events to regain used resources.
     * @param event_id Unique if of the event to unregister
     */
    void Unregister(uint32_t event_id);

    /**
     * Post Posts an event to a socket
     * @param socket_id Socket ID to to post the event to
     * @param event_id Event unique id reference
     * @param event_data Event data reference
     */
    void Post(uint32_t socket_id, uint32_t event_id, void *event_data);

    /*!
    * Broadcast
    * @param event_id Unique ID of the event
    * @param event_data Event data reference
    * \brief Posts an Event to all existing sockets
    */
    void Broadcast(uint32_t event_id, void *event_data);

    /**
    * Dispatch
    * @note When dispatched, the events are considered destroyed.
    * @param socket_id Socket ID of the socket of which events to dispatch.
    * @param dispatch_function Dispatch_function the callback function that will be called for each event
	*        dispatched. The callbacks parameters contains a pointer to a unique Event
	*        for this post and user_ptr is the same as Dispatch called user_ptr.
    * @return Number of dispatched events
    */
	uint32_t Dispatch(uint32_t socket_id, void(*dispatch_function)(dmEvent::Event *event_object, void* user_ptr), void* user_ptr);

	/**
	 * Consume all pending events
	 * @param socket_id Socket ID
	 * @return uint32_t socket_id, void(*dispatch_function)(dmEvent::Event *event_object, void* user_ptr), void* user_ptr);
	 */
	uint32_t Consume(uint32_t socket_id);

    /**
     * CreateSocket
     * @brief The ID returned is used by event functions to reference the created socket.
     * @param buffer_byte_size The max size of the buffer containing the eventdata pending dispatch.
     * @return 0 if failed, 1 if successful, 2 if socket_id already exists
     */
    uint32_t CreateSocket(uint32_t socket_id, uint32_t buffer_byte_size);

    /**
     * DestroySocket
     * @param socket ID of the socket to destroy
     * @brief Pending events in a socket is not called when a socket is destroyed. Be sure
     *  to call dispatch prior to destroying if this is a desired path.
     * @return false if socket_id didn't exist.
     */
    bool DestroySocket(uint32_t socket_id);

};

#endif // DM_EVENT_H

