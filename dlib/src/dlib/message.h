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
        RESULT_INVALID_SOCKET_NAME = -4,    //!< RESULT_INVALID_SOCKET_NAME
        RESULT_MALFORMED_URI = -5           //!< RESULT_MALFORMED_URI
    };

    /**
     * Socket handle
     */
    typedef uint32_t HSocket;

    /**
     * URI specifying a receiver of messages
     */
    struct URI
    {
        URI();

        HSocket m_Socket;       //! Socket
        dmhash_t m_Path;        //! Path of the receiver
        dmhash_t m_Fragment;    //! Fragment of the receiver
    };

    /**
     * Message data desc used at dispatch callback. When a message is posted,
     * the actual object is copied into the sockets internal buffer.
     */
    struct Message
    {
        URI             m_Sender;       //! Sender uri
        URI             m_Receiver;     //! Receiver uri
        dmhash_t        m_ID;           //! Unique ID of message
        uintptr_t       m_Descriptor;   //! User specified descriptor of the message data
        uint32_t        m_DataSize;     //! Size of userdata in bytes
        struct Message* m_Next;         //! Ptr to next message (or 0 if last)
        uint8_t         m_Data[0];      //! Payload
    };

    /**
     * @see #Dispatch
     */
    typedef void(*DispatchCallback)(dmMessage::Message *message, void* user_ptr);

    /**
     * Create a new socket
     * @param name Socket name. Its length must be more than 0 and it cannot contain the characters '#' or ':' (@see ParseURI)
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
     * @param out_socket The socket as an out-parameter. The handle pointed to is only written to if the function is successfull
     * @return RESULT_OK if the socket was found
     */
    Result GetSocket(const char *name, HSocket* out_socket);

    /**
     * Post an message to a socket
     * @note Message data is copied by value
     * @param sender The sender URI if the receiver wants to respond. 0x0 is accepted
     * @param receiver The receiver URI, must not be 0x0
     * @param message_id Message id
     * @param descriptor User specified descriptor of the message data
     * @param message_data Message data reference
     * @param message_data_size Message data size in bytes
     */
    void Post(const URI* sender, const URI* receiver, dmhash_t message_id, uintptr_t descriptor, const void* message_data, uint32_t message_data_size);

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

    /**
     * Convert a string to a URI struct
     * @param uri string of the format [socket:][path][#fragment]
     * @param out_uri URI struct as out parameter
     * @return
     * - RESULT_OK on success
     * - RESULT_MALFORMED_URI if the uri could not be parsed
     * - RESULT_SOCKET_NOT_FOUND if the socket in the uri could not be found
     */
    Result ParseURI(const char* uri, URI* out_uri);
};

#endif // DM_MESSAGE_H
