#ifndef DM_MESSAGE_H
#define DM_MESSAGE_H

#include <stdint.h>

namespace dmMessage
{
    /**
     * @brief Message data desc used at dispatch callback. When an message is posted,
     *        the actual object is copied into the sockets internal buffer.
     */
    struct Message
    {
        uint32_t        m_ID;           //! Unique ID of message
        uint32_t        m_DataSize;     //! Size of userdata in bytes
        struct Message* m_Next;         //! Ptr to next message (or 0 if last)
        uint8_t         m_Data[0];      //! Payload
    };

    /**
     * Result enum
     */
    enum Result
    {
        RESULT_OK = 0,               //!< RESULT_OK
        RESULT_SOCKET_EXISTS = -1,   //!< RESULT_SOCKET_EXISTS
        RESULT_SOCKET_NOT_FOUND = -2,//!< RESULT_SOCKET_NOT_FOUND
    };

    /**
     * Posts an message to a socket
     * @param socket_id Socket ID to to post the message to
     * @param message_id Message unique id reference
     * @param message_data Message data reference
     * @param message_data_size Message data size in bytes
     */
    void Post(uint32_t socket_id, uint32_t message_id, const void* message_data,
            uint32_t message_data_size);

    /**
     * Dispatch
     * @note When dispatched, the messages are considered destroyed.
     * @param socket_id Socket ID of the socket of which messages to dispatch.
     * @param dispatch_function Dispatch_function the callback function that will be called for each message
     *        dispatched. The callbacks parameters contains a pointer to a unique Message
     *        for this post and user_ptr is the same as Dispatch called user_ptr.
     * @return Number of dispatched messages
     */
    uint32_t Dispatch(uint32_t socket_id, void(*dispatch_function)(dmMessage::Message *message, void* user_ptr), void* user_ptr);

    /**
     * Consume all pending messages
     * @param socket_id Socket ID
     * @return Number of dispatched messages
     */
    uint32_t Consume(uint32_t socket_id);

    /**
     * CreateSocket
     * @brief The ID returned is used by message functions to reference the created socket.
     * @param buffer_byte_size The max size of the buffer containing the message-data pending dispatch.
     * @return RESULT_OK on success
     */
    Result CreateSocket(uint32_t socket_id, uint32_t buffer_byte_size);

    /**
     * DestroySocket
     * @param socket ID of the socket to destroy
     * @brief Pending messages in a socket is not called when a socket is destroyed. Be sure
     *        to call dispatch prior to destroying if this is a desired path.
     * @return RESULT_OK on success
     */
    Result DestroySocket(uint32_t socket_id);

};

#endif // DM_MESSAGE_H
