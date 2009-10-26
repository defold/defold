#ifndef DDF_H
#define DDF_H

#include <stdint.h>

#define DDF_OFFSET_OF(T, F) (((uintptr_t) (&((T*) 16)->F)) - 16)

struct SDDFDescriptor;

struct SDDFEnumValueDescriptor
{
    const char*     m_Name;
    int32_t         m_Value;
};

struct SDDFEnumDescriptor
{
    uint16_t                 m_MajorVersion;
    uint16_t                 m_MinorVersion;
    const char*              m_Name;
    SDDFEnumValueDescriptor* m_EnumValues;
    uint8_t                  m_EnumValueCount;
};

struct SDDFFieldDescriptor
{
    const char*     m_Name;
    uint32_t        m_Number : 22;
    uint32_t        m_Type : 6;
    uint32_t        m_Label : 4;
    SDDFDescriptor* m_MessageDescriptor;
    uint32_t        m_Offset;
};

struct SDDFDescriptor
{
    uint16_t             m_MajorVersion;
    uint16_t             m_MinorVersion;
    const char*          m_Name;
    uint32_t             m_Size;
    SDDFFieldDescriptor* m_Fields;
    uint8_t              m_FieldCount;  // TODO: Where to check < 255...?
};

struct DDFRepeatedField
{
    uintptr_t m_Array;
    uint32_t  m_ArrayCount;
};

enum DDFLabel
{
    DDF_LABEL_OPTIONAL = 1,
    DDF_LABEL_REQUIRED = 2,
    DDF_LABEL_REPEATED = 3,
};

enum DDFType
{
    DDF_TYPE_DOUBLE         = 1,
    DDF_TYPE_FLOAT          = 2,
    DDF_TYPE_INT64          = 3,   // Not ZigZag encoded.  Negative numbers
    // take 10 bytes.  Use TYPE_SINT64 if negative
    // values are likely.
    DDF_TYPE_UINT64         = 4,
    DDF_TYPE_INT32          = 5,   // Not ZigZag encoded.  Negative numbers
    // take 10 bytes.  Use TYPE_SINT32 if negative
    // values are likely.
    DDF_TYPE_FIXED64        = 6,
    DDF_TYPE_FIXED32        = 7,
    DDF_TYPE_BOOL           = 8,
    DDF_TYPE_STRING         = 9,
    DDF_TYPE_GROUP          = 10,  // Tag-delimited aggregate.
    DDF_TYPE_MESSAGE        = 11,  // Length-delimited aggregate.

    // New in version 2.
    DDF_TYPE_BYTES          = 12,
    DDF_TYPE_UINT32         = 13,
    DDF_TYPE_ENUM           = 14,
    DDF_TYPE_SFIXED32       = 15,
    DDF_TYPE_SFIXED64       = 16,
    DDF_TYPE_SINT32         = 17,  // Uses ZigZag encoding.
    DDF_TYPE_SINT64         = 18,  // Uses ZigZag encoding.
};

enum DDFError
{
    DDF_ERROR_OK = 0,
    DDF_ERROR_FIELDTYPE_MISMATCH = 1,
    DDF_ERROR_WIRE_FORMAT = 2,
    DDF_ERROR_IO_ERROR = 3,
    DDF_ERROR_VERSION_MISMATCH = 4,
    DDF_ERROR_INTERNAL_ERROR = 1000,
};

enum DDFWireType
{
    DDF_WIRETYPE_VARINT           = 0,
    DDF_WIRETYPE_FIXED64          = 1,
    DDF_WIRETYPE_LENGTH_DELIMITED = 2,
    DDF_WIRETYPE_START_GROUP      = 3,
    DDF_WIRETYPE_END_GROUP        = 4,
    DDF_WIRETYPE_FIXED32          = 5,
};

/**
 * Load/decode a DDF message from buffer
 * @param buffer Input buffer
 * @param buffer_size Input buffer size in bytes
 * @param desc DDF descriptor
 * @param message Pointer to message
 * @return DDF_ERROR_OK on success
 */
DDFError DDFLoadMessage(const void* buffer, uint32_t buffer_size, const SDDFDescriptor* desc, void** message);

typedef bool (*DDFSaveFunction)(void* context, const void* buffer, uint32_t buffer_size);
DDFError DDFSaveMessage(const void* message, const SDDFDescriptor* desc, void* context, DDFSaveFunction save_function);

DDFError DDFSaveMessageToFile(const void* message, const SDDFDescriptor* desc, const char* file_name);

/**
 * Load/decode a DDF message from buffer. Template variant
 * @param buffer Input buffer
 * @param buffer_size Input buffer size in bytes
 * @param message Pointer to message
 * @return DDF_ERROR_OK on success
 */
template <typename T>
DDFError DDFLoadMessage(const void* buffer, uint32_t buffer_size, T** message)
{
    return DDFLoadMessage(buffer, buffer_size, T::m_DDFDescriptor, (void**) message);
}

/**
 * Load/decode a DDF message from file
 * @param file_name File name
 * @param desc DDF descriptor
 * @return Pointer to message
 */
DDFError DDFLoadMessageFromFile(const char* file_name, const SDDFDescriptor* desc, void** message);

/**
 * Get enum value for name. NOTE: Using this function for undefined names is considered as a fatal run-time error.
 * @param desc Enum descriptor
 * @param name Enum name
 * @return Enum value
 */
int32_t      DDFGetEnumValue(const SDDFEnumDescriptor* desc, const char* name);

/**
 * Get <b>*first*</b> enum name for value.
 * @param desc Descriptor
 * @param value Value to get name for
 * @return Enum name. NULL if none found.
 */
const char*  DDFGetEnumName(const SDDFEnumDescriptor* desc, int32_t value);

void DDFFreeMessage(void* message);

#endif // DDF_H
