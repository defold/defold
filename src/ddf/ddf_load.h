#ifndef DDF_LOAD_H
#define DDF_LOAD_H

#include <stdint.h>
#include "ddf_inputbuffer.h"
#include "ddf_message.h"
#include "ddf_loadcontext.h"

DDFError DDFSkipField(CDDFInputBuffer* input_buffer, uint32_t type);

DDFError DDFDoLoadMessage(CDDFLoadContext* load_context, CDDFInputBuffer* input_buffer, 
                          const SDDFDescriptor* desc, CDDFMessage* message);


#endif // DDF_LOAD_H 
