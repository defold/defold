#ifndef DM_MESSAGE_H
#define DM_MESSAGE_H

#include <stdint.h>
#include <dlib/hash.h>

namespace dmMessage
{
    /**
     * Result enum
     */
    enum Result
    {
        RESULT_OK = 0,                      //!< RESULT_OK
        RESULT_SOCKET_EXISTS = -1,          //!< RESULT_SOCKET_EXISTS
        RESULT_SOCKET_NOT_FOUND = -2,       //!< RESULT_SOCKET_NOT_FOUND
        RESULT_SOCKET_OUT_OF_RESOURCES = -3,//!< RESULT_SOCKET_OUT_OF_RESOURCES
    };

    /**
     * Message data desc used at dispatch callback. When an message is posted,
     * the actual object is copied into the sockets internal buffer.
     */
    struct Message
    {
        dmhash_t        m_ID;           //! Unique ID of message
        uint32_t        m_DataSize;     //! Size of userdata in bytes
        struct Message* m_Next;         //! Ptr to next message (or 0 if last)
        uint8_t         m_Data[0];      //! Payload
    };

    /**
     * Socket handle
     */
    typedef uint32_t HSocket;

    /**
     * @see #Dispatch
     */
    typedef void(*DispatchCallback)(dmMessage::Message *message, void* user_ptr);

    /**
     * Create a new socket
     * @param name Socket name
     * @param socket Socket handle (out value)
     * @return RESULT_OK on success
     */
    Result NewSocket(const char* name, HSocket* socket);

    /**
     * Delete a socket
     * @note  The socket must not have any pending messages
     * @param socket Socket to delete
     */
    void DeleteSocket(HSocket socket);

    /**
     * Get socket by name
     * @param name Socket name
     * @return Socket handle. 0 if the socket doesn't exists.
     */
    HSocket GetSocket(const char *name);

    /**
     * Post an message to a socket
     * @note Message data is copied by value
     * @param socket Socket handle
     * @param message_id Message id
     * @param message_data Message data reference
     * @param message_data_size Message data size in bytes
     */
    void Post(HSocket socket, dmhash_t message_id, const void* message_data, uint32_t message_data_size);

    /**
     * Dispatch messages
     * @note When dispatched, the messages are considered destroyed. Messages posted during dispatch
     *       are handled in the next invocation to #Dispatch
     * @param socket Socket handle of the socket of which messages to dispatch.
     * @param dispatch_callback Callback function that will be called for each message
     *        dispatched. The callbacks parameters contains a pointer to a unique Message
     *        for this post and user_ptr is the same as Dispatch called user_ptr.
     * @return Number of dispatched messages
     */
    uint32_t Dispatch(HSocket socket, DispatchCallback dispatch_callback, void* user_ptr);

    /**
     * Consume all pending messages
     * @param socket Socket handle
     * @return Number of dispatched messages
     */
    uint32_t Consume(HSocket socket);
};

#endif // DM_MESSAGE_H
