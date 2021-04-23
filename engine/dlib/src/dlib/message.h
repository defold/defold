// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_MESSAGE_H
#define DM_MESSAGE_H

#include <stdint.h>
#include <string.h>
#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/message.h>

namespace dmMessage
{
    /**
     * Message data desc used at dispatch callback. When a message is posted,
     * the actual object is copied into the sockets internal buffer.
     */
    struct Message
    {
        URL                    m_Sender;            //! Sender uri
        URL                    m_Receiver;          //! Receiver uri
        dmhash_t               m_Id;                //! Unique id of message
        uintptr_t              m_UserData1;         //! User data pointer
        uintptr_t              m_UserData2;         //! User data pointer
        uintptr_t              m_Descriptor;        //! User specified descriptor of the message data
        uint32_t               m_DataSize;          //! Size of message data in bytes
        struct Message*        m_Next;              //! Ptr to next message (or 0 if last)
        MessageDestroyCallback m_DestroyCallback;   //! If set, will be called after each dispatch
        uint8_t DM_ALIGNED(16) m_Data[0];           //! Payload
    };

    // Page size must be a multiple of dmMessage::ALIGNMENT (see message.cpp).
    // This simplifies the allocation scheme
    const uint32_t DM_MESSAGE_PAGE_SIZE = 4096U;
    const uint32_t DM_MESSAGE_MAX_DATA_SIZE = DM_MESSAGE_PAGE_SIZE - sizeof(Message);

    /**
     * @see #Dispatch
     */
    typedef void(*DispatchCallback)(dmMessage::Message *message, void* user_ptr);


    /**
     * Create a new socket
     * @param name Socket name. Its length must be more than 0 and it cannot contain the characters '#' or ':' (@see ParseURL)
     * @param socket Socket handle (out value)
     * @return RESULT_OK on success
     */
    Result NewSocket(const char* name, HSocket* socket);

    /**
     * Delete a socket
     * @note  The socket must not have any pending messages
     * @param socket Socket to delete
     * @return RESULT_OK if the socket was deleted
     */
    Result DeleteSocket(HSocket socket);

    /**
     * Get socket by name
     * @param name Socket name
     * @param out_socket The socket as an out-parameter. The handle pointed to is only written to if the function is successfull
     * @return RESULT_OK if the socket was found, RESULT_NAME_OK_SOCKET_NOT_FOUND if socket wasn't found but the name was ok
     */
    Result GetSocket(const char *name, HSocket* out_socket);

    /**
     * Get socket name
     * @param socket Socket
     * @return socket name, 0x0 if it was not found
     */
    const char* GetSocketName(HSocket socket);

    /**
     * Tests if a socket is valid (not deleted).
     * @param socket Socket
     * @return if the socket is valid or not
     */
    bool IsSocketValid(HSocket socket);

    /**
     * Test if a socket has any messages
     * @param socket Socket
     * @return if the socket has messages or not
     */
    bool HasMessages(HSocket socket);

    /**
     * Resets the given URL to default values.
     * @note Previously the URL wasn't reset in the constructor and certain calls
     *       to ResetURL might currently be redundant
     * @param url URL to reset
     */
    void ResetURL(const URL& url);

    // Internal legacy function
    Result Post(const URL* sender, const URL* receiver, dmhash_t message_id, uintptr_t user_data1, uintptr_t descriptor, const void* message_data, uint32_t message_data_size, MessageDestroyCallback destroy_callback);

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
     * Dispatch messages blocking. The function will return as soon at least one message
     * is dispatched.
     * See Dispatch() for additional information
     * @param socket socket
     * @param dispatch_callback dispatch callback
     * @param user_ptr user data
     * @return Number of dispatched messages
     */
    uint32_t DispatchBlocking(HSocket socket, DispatchCallback dispatch_callback, void* user_ptr);

    /**
     * Consume all pending messages
     * @param socket Socket handle
     * @return Number of dispatched messages
     */
    uint32_t Consume(HSocket socket);

    /**
     * Convert a string to a URL struct
     * @param uri string of the format [socket:][path][#fragment]
     * @param parsed url in string format, must not be 0x0
     * @return
     * - RESULT_OK on success
     * - RESULT_MALFORMED_URL if the uri could not be parsed
     */
    Result ParseURL(const char* uri, StringURL* out_url);
};

#endif // DM_MESSAGE_H
