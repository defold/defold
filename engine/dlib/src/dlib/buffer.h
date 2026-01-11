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

#ifndef DM_BUFFER
#define DM_BUFFER

#include <dmsdk/dlib/buffer.h>

namespace dmBuffer
{
    /*#
     * Initializes the buffer context
     */
    void NewContext();


    /*#
     * Destroys the buffer context
     */
    void DeleteContext();


    // Internal API
    uint32_t GetStructSize(HBuffer hbuffer);

    /*# get the number of streams in a buffer
     *
     * Gets the number of streams in a buffer
     * @name dmBuffer::GetNumStreams
     * @param buffer [type:dmBuffer::HBuffer] The buffer
     * @param num_streams [type:uint32_t*] The output
     * @return count [type:uint32_t] The number of streams
     * @return result [type:dmBuffer::Result] RESULT_OK if the buffer is valid
    */
    Result GetNumStreams(HBuffer buffer, uint32_t* num_streams);


    /*# get the hashed name of a stream
     *
     * Gets the hashed name of the stream
     * @name dmBuffer::GetStreamName
     * @param buffer [type:dmBuffer::HBuffer] The buffer
     * @param index [type:uint32_t] The index of the stream
     * @param stream_name [type:dmhash_t*] The out variable that receives the name
     * @return result [type:dmBuffer::Result] RESULT_OK if the stream exists
    */
    Result GetStreamName(HBuffer buffer, uint32_t index, dmhash_t* stream_name);

    /*# get offset of a stream in bytes
     *
     * Gets byte offset of a stream within the buffer.
     * @name dmBuffer::GetStreamOffset
     * @param buffer [type:dmBuffer::HBuffer] The buffer
     * @param index [type:uint32_t] The index of the stream
     * @param offset [type:uint32_t*] The out variable that receives the byte offset
     * @return result [type:dmBuffer::Result] RESULT_OK on success
    */
    Result GetStreamOffset(HBuffer buffer, uint32_t index, uint32_t* offset);

    /*# create a buffer clone
     *
     * Creates a new clone of a buffer
     * @name dmBuffer::Clone
     * @param buffer [type:dmBuffer::HBuffer] The source buffer that will be cloned
     * @param buffer [type:dmBuffer::HBuffer] The cloned buffer
     * @return result [type:dmBuffer::Result] RESULT_OK on success
    */
    Result Clone(const HBuffer src_buffer, HBuffer* out_buffer);

    Result CalcStructSize(uint32_t num_streams, const StreamDeclaration* streams, uint32_t* size, uint32_t* offsets);
}

#endif
