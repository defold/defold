#ifndef DM_BUFFER
#define DM_BUFFER

#include <dmsdk/dlib/buffer.h>

namespace dmBuffer
{

// Used by the engine

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


// FOR UNIT TESTING
Result CalcStructSize(uint32_t num_streams, const StreamDeclaration* streams, uint32_t* size, uint32_t* offsets);

}

#endif
