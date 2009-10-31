#ifndef DDF_LOAD_H
#define DDF_LOAD_H

#include <stdint.h>
#include "ddf_inputbuffer.h"
#include "ddf_message.h"
#include "ddf_loadcontext.h"

namespace dmDDF
{

    Result SkipField(InputBuffer* input_buffer, uint32_t type);

    Result DoLoadMessage(LoadContext* load_context, InputBuffer* input_buffer,
                              const Descriptor* desc, Message* message);
}

#endif // DDF_LOAD_H 
