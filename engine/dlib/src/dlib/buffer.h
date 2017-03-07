#ifndef DM_BUFFER
#define DM_BUFFER

#include <dmsdk/dlib/buffer.h>

namespace dmBuffer
{

// These functions are used by our scripting bindings

/*#
 * Gets the number of streams
 * @name dmBuffer::GetNumStreams
 * @param buffer [type:dmBuffer::HBuffer] The buffer
 * @return count [type:uint32_t] The number of streams
*/
uint32_t GetNumStreams(dmBuffer::HBuffer buffer);

/*#
 * Gets the hashed name of the stream
 * @name dmBuffer::GetStreamName
 * @param buffer [type:dmBuffer::HBuffer] The buffer
 * @param index [type:uint32_t] The index of the stream
 * @param stream_name [type:dmhash_t*] The out variable that receives the name
 * @return result [type:dmBuffer::Result] RESULT_OK if the stream exists
*/
Result GetStreamName(dmBuffer::HBuffer buffer, uint32_t index, dmhash_t* stream_name);

}

#endif
