// Copyright 2020-2024 The Defold Foundation
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

#include <stdint.h>
#include <string.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/align.h>


/*# Message API documentation
 * [file:<dmsdk/dlib/message.h>]
 *
 * Api for sending messages
 *
 * @document
 * @name Message
 * @namespace dmMessage
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
     * @typedef
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
     * @name dmMMessage::MessageDestroyCallback
     * @see #dmMessage::Post
     */
    typedef void(*MessageDestroyCallback)(dmMessage::Message* message);

    /*#
     * Message data desc used at dispatch callback. When a message is posted,
     * the actual object is copied into the sockets internal buffer.
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
     * @member m_Data [type: uint8_t*] Payload
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

    /**
     * Post an message to a socket
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
     * @return RESULT_OK if the message was posted
     */
    Result Post(const URL* sender, const URL* receiver, dmhash_t message_id, uintptr_t user_data1, uintptr_t user_data2,
                uintptr_t descriptor, const void* message_data, uint32_t message_data_size, MessageDestroyCallback destroy_callback);

    /** post a ddf message to a socket
     * Post a DDF message to a socket. A helper wrapper for Post()'ing a DDF message
     * @note Message data is copied by value
     * @name PostDDF
     * @tparam TDDFType [type: TDDFType] Must be a DDF type
     * @param message [type: TDDFType*] Message data reference
     * @param sender [type: dmMessage::URL*] The sender URL if the receiver wants to respond. 0x0 is accepted
     * @param receiver [type: dmMessage::URL*] The receiver URL, must not be 0x0
     * @param user_data1 [type: uintptr_t] User data that can be used when both the sender and receiver are known
     * @param user_data2 [type: uintptr_t] User data that can be used when both the sender and receiver are known.
     * @param destroy_callback [type: dmMessage::MessageDestroyCallback] if set, will be called after each message dispatch
     * @return RESULT_OK if the message was posted
     * @examples
     *
     * ```cpp
     * // dmMessage::URL sender, receiver;
     * // dmGameObject::HInstance go;
     *
     * dmGameSystemDDF::PlayAnimation msg;
     * msg.m_Id = animation_id;
     * msg.m_Offset = params.m_CursorStart;
     * msg.m_PlaybackRate = params.m_PlaybackRate;
     *
     * dmMessage::Result result = dmMessage::Post(&msg, &sender, &receiver, (uintptr_t)go, 0, 0));
     * ```
     */
    template <typename TDDFType>
    Result PostDDF(const TDDFType* message, const URL* sender, const URL* receiver, uintptr_t user_data1, uintptr_t user_data2, MessageDestroyCallback destroy_callback)
    {
        return Post(sender, receiver, message->m_DDFDescriptor->m_NameHash, user_data1, user_data2,
                    (uintptr_t)message->m_DDFDescriptor, message, sizeof(TDDFType), destroy_callback);
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
