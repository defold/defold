#include <assert.h>
#include "message.h"
#include "atomic.h"
#include "hashtable.h"
#include "profile.h"

namespace dmMessage
{
    struct SMessageSocket
    {
        Message *m_Header;
        Message *m_Tail;
    };

    struct SMessageInfo
    {
        uint32_t m_ID; //! Unique ID of message
        uint32_t m_DataSize; //! Size of data in bytes
    };

    dmHashTable<uint32_t, SMessageSocket> m_Sockets;

    Result CreateSocket(uint32_t socket_id, uint32_t buffer_byte_size)
    {
        static bool hash_initialized = false;
        if (!hash_initialized)
        {
            hash_initialized = true;
            m_Sockets.SetCapacity(32, 1024);
        }

        if (m_Sockets.Get(socket_id))
            return RESULT_SOCKET_EXISTS;

        SMessageSocket socket;
        socket.m_Header = 0;
        socket.m_Tail = 0;
        m_Sockets.Put(socket_id, socket);
        return RESULT_OK;
    }

    Result DestroySocket(uint32_t socket_id)
    {
        SMessageSocket *socket = m_Sockets.Get(socket_id);
        if (!socket)
            return RESULT_SOCKET_NOT_FOUND;

        //assert(socket->m_Header == 0 && "Destroying socket with nondispatched messages, memory leak..");
        m_Sockets.Erase(socket_id);
        return RESULT_OK;
    }

    void Post(uint32_t socket_id, uint32_t message_id, const void* message_data, uint32_t message_data_size)
    {
        DM_PROFILE(Message, "Post")
        DM_COUNTER("Messages", 1)
        // get socket and message
        SMessageSocket *socket = m_Sockets.Get(socket_id);
        if (!socket)
        {
            assert(false && "Trying to post message to invalid socket id");
            return;
        }

        /*
        SMessageInfo *message_info = m_RegisteredMessages.Get(message_id);
        if (!message_info)
        {
            assert(false && "Trying to post unregistered message");
            return;
        }*/

        uint32_t data_size = sizeof(Message) + message_data_size;
        Message *new_message = (Message *) new uint8_t[data_size];
        new_message->m_ID = message_id;
        new_message->m_DataSize = message_data_size;
        new_message->m_Next = 0;
        memcpy(&new_message->m_Data[0], message_data, message_data_size);

        // mutex lock

        if (!socket->m_Header)
        {
            socket->m_Header = new_message;
            socket->m_Tail = new_message;
        }
        else
        {
            socket->m_Tail->m_Next = new_message;
            socket->m_Tail = new_message;
        }

        // mutex unlock
    }

    uint32_t Dispatch(uint32_t socket_id, DispatchCallback dispatch_callback, void* user_ptr)
    {
        SMessageSocket *socket = m_Sockets.Get(socket_id);
        if (!socket)
        {
            assert(false && "Invalid socket parameter");
            return 0;
        }

        if (!socket->m_Header)
            return 0;
        uint32_t dispatch_count = 0;

        // mutex lock

        Message *message_object = socket->m_Header;
        //Message *last_message_object = socket->m_Tail;
        socket->m_Header = 0;
        socket->m_Tail = 0;

        // mutex unlock

        while (message_object)
        {
            dispatch_callback(message_object, user_ptr);
            uint8_t *old_object = (uint8_t *) message_object;
            message_object = message_object->m_Next;
            delete[] old_object;
            dispatch_count++;
        }

        return dispatch_count;
    }

    static void ConsumeCallback(dmMessage::Message*, void*)
    {
    }

    uint32_t Consume(uint32_t socket_id)
    {
        return Dispatch(socket_id, &ConsumeCallback, 0);
    }

};

