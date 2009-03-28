#ifndef DDF_MESSAGE_H
#define DDF_MESSAGE_H

class CDDFLoadContext;

#include <stdint.h>
#include "ddf.h"
#include "ddf_inputbuffer.h"
#include "ddf_loadcontext.h"

class CDDFMessage
{
public:
    CDDFMessage(const SDDFDescriptor* message_descriptor, char* buffer, uint32_t buffer_size, bool dry_run);

    DDFError ReadField(CDDFLoadContext* load_context,
                       DDFWireType wire_type, 
                       const SDDFFieldDescriptor* field, 
                       CDDFInputBuffer* input_buffer);

    void     SetScalar(const SDDFFieldDescriptor* field, const void* buffer, int buffer_size);
    void*    AddScalar(const SDDFFieldDescriptor* field, const void* buffer, int buffer_size);
    void*    AddMessage(const SDDFFieldDescriptor* field);
    void     AllocateRepeatedBuffer(CDDFLoadContext* load_context, const SDDFFieldDescriptor* field, int element_count);
    void     SetRepeatedBuffer(const SDDFFieldDescriptor* field, void* buffer);
    void     SetString(CDDFLoadContext* load_context, const SDDFFieldDescriptor* field, const char* buffer, int buffer_len);
    void     AddString(CDDFLoadContext* load_context, const SDDFFieldDescriptor* field, const char* buffer, int buffer_len);

private:
    DDFError ReadScalarField(CDDFLoadContext* load_context,
                             DDFWireType wire_type, 
                             const SDDFFieldDescriptor* field, 
                             CDDFInputBuffer* input_buffer);


    DDFError ReadStringField(CDDFLoadContext* load_context,
                             DDFWireType wire_type, 
                             const SDDFFieldDescriptor* field, 
                             CDDFInputBuffer* input_buffer);

    DDFError ReadMessageField(CDDFLoadContext* load_context,
                              DDFWireType wire_type, 
                              const SDDFFieldDescriptor* field, 
                              CDDFInputBuffer* input_buffer);

    const SDDFDescriptor* m_MessageDescriptor;
    char*                 m_Start;
    char*                 m_End;
    bool                  m_DryRun;
};

#endif // DDF_MESSAGE_H 
