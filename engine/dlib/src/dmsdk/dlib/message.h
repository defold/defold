// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#ifndef DMSDK_MESSAGE_H
#define DMSDK_MESSAGE_H

#include <new>
#include <stdint.h>
#include <string.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/align.h>


/*# Message API documentation
 *
 * Api for sending messages
 *
 * @document
 * @name Message
 * @namespace dmMessage
 * @language C++
 */

namespace dmMessage
{
    /*#
     * Result enum
     * @enum
     * @name dmMessage::Result
     * @member RESULT_OK = 0
     * @member RESULT_SOCKET_EXISTS = -1
     * @member RESULT_SOCKET_NOT_FOUND = -2
     * @member RESULT_SOCKET_OUT_OF_RESOURCES = -3
     * @member RESULT_INVALID_SOCKET_NAME = -4
     * @member RESULT_MALFORMED_URL = -5
     * @member RESULT_NAME_OK_SOCKET_NOT_FOUND = -6
     */
    enum Result
    {
        RESULT_OK = 0,
        RESULT_SOCKET_EXISTS = -1,
        RESULT_SOCKET_NOT_FOUND = -2,
        RESULT_SOCKET_OUT_OF_RESOURCES = -3,
        RESULT_INVALID_SOCKET_NAME = -4,
        RESULT_MALFORMED_URL = -5,
        RESULT_NAME_OK_SOCKET_NOT_FOUND = -6,
    };

    /*#
     * Socket handle
     * @typedef
     * @name HSocket
     */
    typedef dmhash_t HSocket;

    /*#
     * URL specifying a sender/receiver of messages
     * @note Currently has a hard limit of 32 bytes
     * @note This struct is a part of the save file APi (see script_table.cpp)
     * @struct
     * @name dmMessage::URL
     */
    struct URL
    {
        URL()
        {
            memset(this, 0, sizeof(URL));
        }

        HSocket     m_Socket;
        dmhash_t    _reserved; // Reserved for sub component path
        dmhash_t    m_Path;
        dmhash_t    m_Fragment;
    };


    /*#
     * Helper struct for parsing a string of the form "socket:path#fragment"
     * @note The sizes do not include the null character. There is no null character since the dmMessage::ParseURL is non destructive.
     * @struct
     * @name dmMessage::StringURL
     * @member m_Socket [type: const char*] The socket
     * @member m_SocketSize [type: uint32_t] The socket length
     * @member m_Path [type: const char*] The path
     * @member m_PathSize [type: uint32_t] The path length
     * @member m_Fragment [type: const char*] The fragment
     * @member m_FragmentSize [type: uint32_t] The fragment length
     */
    struct StringURL
    {
        StringURL()
        {
            memset(this, 0, sizeof(StringURL));
        }
        const char* m_Socket;
        uint32_t    m_SocketSize;
        const char* m_Path;
        uint32_t    m_PathSize;
        const char* m_Fragment;
        uint32_t    m_FragmentSize;
    };

    /*#
     * Resets the given URL to default values.
     * @note Previously the URL wasn't reset in the constructor and certain calls to ResetURL might currently be redundant
     * @name ResetUrl
     * @param url [type: dmMessage::URL] URL to reset
     */
    void ResetURL(URL* url);

    /*#
     * Get the message socket
     * @name GetSocket
     * @param url [type: dmMessage::URL] url
     * @return socket [type: dmMessage::HSocket]
     */
    HSocket GetSocket(const URL* url);

    /*#
     * Set the socket
     * @name SetSocket
     * @param url [type: dmMessage::URL] url
     * @param socket [type: dmMessage::HSocket]
     */
    void SetSocket(URL* url, HSocket socket);

    /*#
     * Tests if a socket is valid (not deleted).
     * @name IsSocketValid
     * @param socket [type: dmMessage::HSocket] Socket
     * @return result [type: bool] if the socket is valid or not
     */
    bool IsSocketValid(HSocket socket);

    /*#
     * Get socket name
     * @name GetSocketName
     * @param socket [type: dmMessage::HSocket] Socket
     * @return name [type: const char*] socket name. 0 if it was not found
     */
    const char* GetSocketName(HSocket socket);

    /*#
     * Get socket name hash
     * @name GetSocketNameHash
     * @param socket [type: dmMessage::HSocket] Socket
     * @return name_hash [type: dmhash_t] socket name hash. 0 if it was not found
     */
    dmhash_t GetSocketNameHash(HSocket socket);

    /*#
     * Get the message path
     * @name GetPath
     * @param url [type: dmMessage::URL] url
     * @return path [type: dmhash_t]
     */
    dmhash_t GetPath(const URL* url);

    /*#
     * Set the message path
     * @name SetPath
     * @param url [type: dmMessage::URL] url
     * @param path [type: dmhash_t]
     */
    void SetPath(URL* url, dmhash_t path);

    /*#
     * Get the message fragment
     * @name GetFragment
     * @param url [type: dmMessage::URL] url
     * @return fragment [type: dmhash_t]
     */
    dmhash_t GetFragment(const URL* url);

    /*#
     * Set the message fragment
     * @name SetFragment
     * @param url [type: dmMessage::URL] url
     * @param fragment [type: dmhash_t]
     */
    void SetFragment(URL* url, dmhash_t fragment);


    /*#
     * @struct
     * @name Message
     */
    struct Message;

    /*#
     * A callback for messages that needs cleanup after being dispatched. E.g. for freeing resources/memory.
     * @typedef
     * @name dmMessage::MessageDestroyCallback
     * @see #dmMessage::Post
     */
    typedef void(*MessageDestroyCallback)(dmMessage::Message* message);

    /*#
     * Message data desc used at dispatch callback. When a message is posted,
     * the actual object is copied into the sockets internal buffer.
     * Use dmMessage::GetData() and dmMessage::GetDataSize() to access the payload.
     * The payload is aligned to 16 bytes and is valid for the lifetime of the message.
     * @struct
     * @name Message
     * @member m_Sender [type: dmMessage::URL] Sender uri
     * @member m_Receiver [type: dmMessage::URL] Receiver uri
     * @member m_Id [type: dmhash_t] Unique id of message
     * @member m_UserData1 [type: uintptr_t] User data pointer
     * @member m_UserData2 [type: uintptr_t] User data pointer
     * @member m_Descriptor [type: uintptr_t] User specified descriptor of the message data
     * @member m_DataSize [type: uint32_t] Size of message data in bytes
     * @member m_Next [type: dmMessage::Message*] Ptr to next message (or 0 if last)
     * @member m_DestroyCallback [type: dmMessage::MessageDestroyCallback] If set, will be called after each dispatch
     */
    struct DM_ALIGNED(16) Message
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
    };

    /*# Pointer to the sender URL in the message.
     * @name GetSender
     * @param message [type: const dmMessage::Message*] message
     * @return sender [type: const dmMessage::URL*] sender URL
     */
    inline const URL* GetSender(const Message* message)
    {
        return &message->m_Sender;
    }

    /*# Copy the sender URL into the message.
     * @name SetSender
     * @param message [type: dmMessage::Message*] message
     * @param sender [type: const dmMessage::URL*] sender URL
     */
    inline void SetSender(Message* message, const URL* sender)
    {
        message->m_Sender = *sender;
    }

    /*# Pointer to the receiver URL in the message.
     * @name GetReceiver
     * @param message [type: const dmMessage::Message*] message
     * @return receiver [type: const dmMessage::URL*] receiver URL
     */
    inline const URL* GetReceiver(const Message* message)
    {
        return &message->m_Receiver;
    }

    /*# Copy the receiver URL into the message.
     * @name SetReceiver
     * @param message [type: dmMessage::Message*] message
     * @param receiver [type: const dmMessage::URL*] receiver URL
     */
    inline void SetReceiver(Message* message, const URL* receiver)
    {
        message->m_Receiver = *receiver;
    }

    /*# Get the message identifier.
     * @name GetId
     * @param message [type: const dmMessage::Message*] message
     * @return id [type: dmhash_t] message identifier
     */
    inline dmhash_t GetId(const Message* message)
    {
        return message->m_Id;
    }

    /*# Set the message identifier.
     * @name SetId
     * @param message [type: dmMessage::Message*] message
     * @param id [type: dmhash_t] message identifier
     */
    inline void SetId(Message* message, dmhash_t id)
    {
        message->m_Id = id;
    }

    /*# Get the first user data value.
     * @name GetUserData1
     * @param message [type: const dmMessage::Message*] message
     * @return user_data [type: uintptr_t] first user data value
     */
    inline uintptr_t GetUserData1(const Message* message)
    {
        return message->m_UserData1;
    }

    /*# Set the first user data value.
     * @name SetUserData1
     * @param message [type: dmMessage::Message*] message
     * @param user_data [type: uintptr_t] first user data value
     */
    inline void SetUserData1(Message* message, uintptr_t user_data)
    {
        message->m_UserData1 = user_data;
    }

    /*# Get the second user data value.
     * @name GetUserData2
     * @param message [type: const dmMessage::Message*] message
     * @return user_data [type: uintptr_t] second user data value
     */
    inline uintptr_t GetUserData2(const Message* message)
    {
        return message->m_UserData2;
    }

    /*# Set the second user data value.
     * @name SetUserData2
     * @param message [type: dmMessage::Message*] message
     * @param user_data [type: uintptr_t] second user data value
     */
    inline void SetUserData2(Message* message, uintptr_t user_data)
    {
        message->m_UserData2 = user_data;
    }

    /*# Get the message descriptor.
     * @name GetDescriptor
     * @param message [type: const dmMessage::Message*] message
     * @return descriptor [type: uintptr_t] user-supplied message descriptor
     */
    inline uintptr_t GetDescriptor(const Message* message)
    {
        return message->m_Descriptor;
    }

    /*# Set the message descriptor.
     * @name SetDescriptor
     * @param message [type: dmMessage::Message*] message
     * @param descriptor [type: uintptr_t] user-supplied message descriptor
     */
    inline void SetDescriptor(Message* message, uintptr_t descriptor)
    {
        message->m_Descriptor = descriptor;
    }

    /*# Get the payload size in bytes.
     * @name GetDataSize
     * @param message [type: const dmMessage::Message*] message
     * @return size [type: uint32_t] payload size in bytes
     */
    inline uint32_t GetDataSize(const Message* message)
    {
        return message->m_DataSize;
    }

    /*# Set the number of payload bytes used. The size must fit in the existing storage.
     * @name SetDataSize
     * @param message [type: dmMessage::Message*] message
     * @param size [type: uint32_t] payload size in bytes
     */
    inline void SetDataSize(Message* message, uint32_t size)
    {
        message->m_DataSize = size;
    }

    /*# Get the next linked message.
     * @name GetNext
     * @param message [type: const dmMessage::Message*] message
     * @return next [type: dmMessage::Message*] next message, or 0 at the end of the list
     */
    inline Message* GetNext(const Message* message)
    {
        return message->m_Next;
    }

    /*# Link a caller-owned message to its successor. Socket queues manage this link for posted messages.
     * @name SetNext
     * @param message [type: dmMessage::Message*] message
     * @param next [type: dmMessage::Message*] next message, or 0 at the end of the list
     */
    inline void SetNext(Message* message, Message* next)
    {
        message->m_Next = next;
    }

    /*# Get the message cleanup callback.
     * @name GetDestroyCallback
     * @param message [type: const dmMessage::Message*] message
     * @return callback [type: dmMessage::MessageDestroyCallback] cleanup callback, or 0
     */
    inline MessageDestroyCallback GetDestroyCallback(const Message* message)
    {
        return message->m_DestroyCallback;
    }

    /*# Set the callback invoked when a posted message is dispatched or discarded.
     * @name SetDestroyCallback
     * @param message [type: dmMessage::Message*] message
     * @param callback [type: dmMessage::MessageDestroyCallback] cleanup callback, or 0
     */
    inline void SetDestroyCallback(Message* message, MessageDestroyCallback callback)
    {
        message->m_DestroyCallback = callback;
    }

    /*#
     * Get the message payload.
     * @name GetData
     * @param message [type: dmMessage::Message*] message
     * @return data [type: uint8_t*] message payload
     */
    inline uint8_t* GetData(Message* message)
    {
        return (uint8_t*)(message + 1);
    }

    /*#
     * Get read-only access to the message payload.
     * @name GetData
     * @param message [type: const dmMessage::Message*] message
     * @return data [type: const uint8_t*] message payload
     */
    inline const uint8_t* GetData(const Message* message)
    {
        return (const uint8_t*)(message + 1);
    }

    // Return the aggregate by value so placement construction initializes each URL only once.
    inline Message MakeMessageHeaderInternal(const URL* sender, const URL* receiver, dmhash_t message_id, uintptr_t user_data1, uintptr_t user_data2,
                                             uintptr_t descriptor, uint32_t data_size, MessageDestroyCallback destroy_callback)
    {
        Message header = {
            sender ? *sender : URL(), receiver ? *receiver : URL(), message_id, user_data1, user_data2,
            descriptor, data_size, 0, destroy_callback
        };
        return header;
    }

    /*# initialize a message for writing its payload in place
     * Constructs a message in caller-owned storage and clears its URLs, user data, queue link and cleanup callback.
     * Write the payload through GetData() before dispatch.
     * @name InitMessage
     * @param buffer [type: void*] 16-byte-aligned storage for sizeof(dmMessage::Message) + data_size bytes
     * @param message_id [type: dmhash_t] message identifier
     * @param descriptor [type: uintptr_t] user-supplied message descriptor
     * @param data_size [type: uint32_t] payload size in bytes
     * @return message [type: dmMessage::Message*] message in the supplied buffer
     */
    inline Message* InitMessage(void* buffer, dmhash_t message_id, uintptr_t descriptor, uint32_t data_size)
    {
        return new (buffer) Message(MakeMessageHeaderInternal(0, 0, message_id, 0, 0, descriptor, data_size, 0));
    }

    /*# initialize a message and copy its payload
     * Constructs a message in caller-owned storage and copies data_size bytes into its payload.
     * @name InitMessage
     * @param buffer [type: void*] 16-byte-aligned storage for sizeof(dmMessage::Message) + data_size bytes
     * @param sender [type: const dmMessage::URL*] sender URL, or 0
     * @param receiver [type: const dmMessage::URL*] receiver URL, or 0
     * @param message_id [type: dmhash_t] message identifier
     * @param user_data1 [type: uintptr_t] first user data value
     * @param user_data2 [type: uintptr_t] second user data value
     * @param descriptor [type: uintptr_t] user-supplied message descriptor
     * @param data [type: const void*] source bytes outside the destination buffer, or 0 when data_size is zero
     * @param data_size [type: uint32_t] payload size in bytes
     * @param destroy_callback [type: dmMessage::MessageDestroyCallback] cleanup callback, or 0
     * @return message [type: dmMessage::Message*] message in the supplied buffer
     */
    inline Message* InitMessage(void* buffer, const URL* sender, const URL* receiver, dmhash_t message_id, uintptr_t user_data1, uintptr_t user_data2,
                                uintptr_t descriptor, const void* data, uint32_t data_size, MessageDestroyCallback destroy_callback)
    {
        Message* message = new (buffer) Message(MakeMessageHeaderInternal(sender, receiver, message_id, user_data1, user_data2, descriptor, data_size, destroy_callback));
        if (data_size)
        {
            memcpy(GetData(message), data, data_size);
        }
        return message;
    }

    /**
     * Post a message to a socket
     * @note Message data is copied by value
     * @name Post
     * @param sender [type: dmMessage::URL*] The sender URL if the receiver wants to respond. 0x0 is accepted
     * @param receiver [type: dmMessage::URL*] The receiver URL, must not be 0x0
     * @param message_id [type: dmhash_t] Message id
     * @param user_data1 [type: uintptr_t] User data that can be used when both the sender and receiver are known
     * @param user_data2 [type: uintptr_t] User data that can be used when both the sender and receiver are known.
     * @param descriptor [type: uintptr_t] User specified descriptor of the message data
     * @param message_data [type: void*] Message data reference
     * @param message_data_size [type: uint32_t] Message data size in bytes
     * @param destroy_callback [type: dmMessage::MessageDestroyCallback] if set, will be called after each message dispatch
     * @return RESULT_OK on success, RESULT_SOCKET_NOT_FOUND for an unknown receiver, or RESULT_SOCKET_OUT_OF_RESOURCES if a queue page could not be allocated.
     */
    Result Post(const URL* sender, const URL* receiver, dmhash_t message_id, uintptr_t user_data1, uintptr_t user_data2,
                uintptr_t descriptor, const void* message_data, uint32_t message_data_size, MessageDestroyCallback destroy_callback);

    /** post a ddf message to a socket
     * Post a DDF message to a socket. A helper wrapper for Post()'ing a DDF message
     * @note Message data is copied by value
     * @name PostDDF
     * @tparam T Must be a DDF type
     * @param message [type: T*] Message data reference
     * @param sender [type: dmMessage::URL*] The sender URL if the receiver wants to respond. 0x0 is accepted
     * @param receiver [type: dmMessage::URL*] The receiver URL, must not be 0x0
     * @param user_data1 [type: uintptr_t] User data that can be used when both the sender and receiver are known
     * @param user_data2 [type: uintptr_t] User data that can be used when both the sender and receiver are known.
     * @param destroy_callback [type: dmMessage::MessageDestroyCallback] if set, will be called after each message dispatch
     * @return The result of dmMessage::Post().
     * @examples
     *
     * ```cpp
     * // dmMessage::URL sender, receiver;
     * uintptr_t request_id = 1;
     *
     * dmGameSystemDDF::PlayAnimation msg;
     * msg.m_Id = animation_id;
     * msg.m_Offset = params.m_CursorStart;
     * msg.m_PlaybackRate = params.m_PlaybackRate;
     *
     * dmMessage::Result result = dmMessage::PostDDF(&msg, &sender, &receiver, request_id, 0, 0);
     * ```
     */
    template <typename T>
    Result PostDDF(const T* message, const URL* sender, const URL* receiver, uintptr_t user_data1, uintptr_t user_data2, MessageDestroyCallback destroy_callback)
    {
        return Post(sender, receiver, message->m_DDFDescriptor->m_NameHash, user_data1, user_data2,
                    (uintptr_t)message->m_DDFDescriptor, message, sizeof(T), destroy_callback);
    }

    /*#
     * Convert a string to a URL struct
     * @note No allocation occurs, thus no cleanup is needed.
     * @name ParseUrl
     * @param uri [type: const char*] string of the format [socket:][path][#fragment]
     * @param out [type: dmMessage::StringUrl] url in string format, must not be 0x0
     * @return
     * - RESULT_OK on success
     * - RESULT_MALFORMED_URL if the uri could not be parsed
     */
    Result ParseURL(const char* uri, StringURL* out_url);
};

#endif // DMSDK_MESSAGE_H
