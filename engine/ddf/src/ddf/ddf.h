#ifndef DM_DDF_H
#define DM_DDF_H

#include <stdint.h>
#include <dlib/static_assert.h>
#include <dlib/array.h>
#include <dlib/hash.h>

#define DDF_OFFSET_OF(T, F) (((uintptr_t) (&((T*) 16)->F)) - 16)
#define DDF_MAX_FIELDS (128)

namespace dmDDF
{
    struct Descriptor;

    struct EnumValueDescriptor
    {
        const char*     m_Name;
        int32_t         m_Value;
    };

    struct EnumDescriptor
    {
        uint16_t             m_MajorVersion;
        uint16_t             m_MinorVersion;
        const char*          m_Name;
        EnumValueDescriptor* m_EnumValues;
        uint8_t              m_EnumValueCount;
    };

    struct FieldDescriptor
    {
        const char* m_Name;
        uint32_t    m_Number : 22;
        uint32_t    m_Type : 6;
        uint32_t    m_Label : 4;
        Descriptor* m_MessageDescriptor;
        uint32_t    m_Offset;
        const char* m_DefaultValue;
    };

    struct Descriptor
    {
        uint16_t         m_MajorVersion;
        uint16_t         m_MinorVersion;
        const char*      m_Name;
        uint64_t         m_NameHash;
        uint32_t         m_Size;
        FieldDescriptor* m_Fields;
        uint8_t          m_FieldCount;  // TODO: Where to check < 255...?
        void*            m_NextDescriptor;
    };

    struct RepeatedField
    {
        uintptr_t m_Array;
        uint32_t  m_ArrayCount;
    };

    enum Label
    {
        LABEL_OPTIONAL = 1,
        LABEL_REQUIRED = 2,
        LABEL_REPEATED = 3,
    };

    enum Type
    {
        TYPE_DOUBLE         = 1,
        TYPE_FLOAT          = 2,
        TYPE_INT64          = 3,   // Not ZigZag encoded.  Negative numbers
        // take 10 bytes.  Use TYPE_SINT64 if negative
        // values are likely.
        TYPE_UINT64         = 4,
        TYPE_INT32          = 5,   // Not ZigZag encoded.  Negative numbers
        // take 10 bytes.  Use TYPE_SINT32 if negative
        // values are likely.
        TYPE_FIXED64        = 6,
        TYPE_FIXED32        = 7,
        TYPE_BOOL           = 8,
        TYPE_STRING         = 9,
        TYPE_GROUP          = 10,  // Tag-delimited aggregate.
        TYPE_MESSAGE        = 11,  // Length-delimited aggregate.

        // New in version 2.
        TYPE_BYTES          = 12,
        TYPE_UINT32         = 13,
        TYPE_ENUM           = 14,
        TYPE_SFIXED32       = 15,
        TYPE_SFIXED64       = 16,
        TYPE_SINT32         = 17,  // Uses ZigZag encoding.
        TYPE_SINT64         = 18,  // Uses ZigZag encoding.
    };

    enum Result
    {
        RESULT_OK = 0,
        RESULT_FIELDTYPE_MISMATCH = 1,
        RESULT_WIRE_FORMAT_ERROR = 2,
        RESULT_IO_ERROR = 3,
        RESULT_VERSION_MISMATCH = 4,
        RESULT_MISSING_REQUIRED = 5,
        RESULT_INTERNAL_ERROR = 1000,
    };

    enum WireType
    {
        WIRETYPE_VARINT           = 0,
        WIRETYPE_FIXED64          = 1,
        WIRETYPE_LENGTH_DELIMITED = 2,
        WIRETYPE_START_GROUP      = 3,
        WIRETYPE_END_GROUP        = 4,
        WIRETYPE_FIXED32          = 5,
    };

    /// Store pointers as offset from base address. Needed when serializing entire messages (copy)
    const uint32_t OPTION_OFFSET_POINTERS = (1 << 0);

    /**
     * Internal. Do not use.
     */
    struct InternalRegisterDescriptor
    {
        InternalRegisterDescriptor(Descriptor* descriptor);
    };

    /**
     * Register all ddf-types available
     */
    void RegisterAllTypes();

    /**
     * Get Descriptor from name
     * @param name type name
     * @return Descriptor. NULL of not found
     */
    const Descriptor* GetDescriptor(const char* name);

    /**
     * Get Descriptor from hash
     * @param hash hash of type name
     * @return Descriptor. NULL of not found
     */
    const Descriptor* GetDescriptorFromHash(dmhash_t hash);

    /**
     * Load/decode a DDF message from buffer
     * @param buffer Input buffer
     * @param buffer_size Input buffer size in bytes
     * @param desc DDF descriptor
     * @param message Pointer to message
     * @return RESULT_OK on success
     */
    Result LoadMessage(const void* buffer, uint32_t buffer_size, const Descriptor* desc, void** message);

    /**
     * Load/decode a DDF message from buffer
     * @param buffer Input buffer
     * @param buffer_size Input buffer size in bytes
     * @param desc DDF descriptor
     * @param message Pointer to message
     * @param options options, eg OPTION_OFFSET_POINTERS
     * @param size load message size [out]
     * @return RESULT_OK on success
     */
    Result LoadMessage(const void* buffer, uint32_t buffer_size, const Descriptor* desc, void** message, uint32_t options, uint32_t* size);

    /**
     * Save function call-back
     * @param context Save context
     * @param buffer Buffer to write
     * @param buffer_size Buffer size
     * @return true on success
     */
    typedef bool (*SaveFunction)(void* context, const void* buffer, uint32_t buffer_size);

    /**
     * Save message
     * @param message Message
     * @param desc DDF descriptor
     * @param context Save context
     * @param save_function Save function
     * @return RESULT_OK on success
     */
    Result SaveMessage(const void* message, const Descriptor* desc, void* context, SaveFunction save_function);

    /**
     * Save message to array
     * @param message Message
     * @param desc DDF descriptor
     * @param buffer Buffer to save to
     * @return RESULT_OK on success
     */
    Result SaveMessageToArray(const void* message, const Descriptor* desc, dmArray<uint8_t>& array);

    /**
     * Save message to file
     * @param message Message
     * @param desc DDF descriptor
     * @param file_name File name
     * @return RESULT_OK on success
     */
    Result SaveMessageToFile(const void* message, const Descriptor* desc, const char* file_name);

    /**
     * Calculates capacity needed for a message
     * @param message Message
     * @param desc DDF descriptor
     * @param size Returned size
     * @return RESULT_OK on success
     */
    Result SaveMessageSize(const void* message, const Descriptor* desc, uint32_t* size);

    /**
     * Load/decode a DDF message from buffer. Template variant
     * @param buffer Input buffer
     * @param buffer_size Input buffer size in bytes
     * @param message Pointer to message
     * @return RESULT_OK on success
     */
    template <typename T>
    Result LoadMessage(const void* buffer, uint32_t buffer_size, T** message)
    {
        return LoadMessage(buffer, buffer_size, T::m_DDFDescriptor, (void**) message);
    }

    /**
     * Load/decode a DDF message from file
     * @param file_name File name
     * @param desc DDF descriptor
     * @return Pointer to message
     */
    Result LoadMessageFromFile(const char* file_name, const Descriptor* desc, void** message);

    /**
     * If the message was loaded with the flag OPTION_OFFSET_POINTERS, all pointers have their offset stored.
     * This function resolves those offsets into actual pointers
     * @param desc DDF descriptor
     * @param message The message
     * @return RESULT_OK on success
     */
    Result ResolvePointers(const Descriptor* desc, void* message);

    /**
     * Get enum value for name. NOTE: Using this function for undefined names is considered as a fatal run-time error.
     * @param desc Enum descriptor
     * @param name Enum name
     * @return Enum value
     */
    int32_t GetEnumValue(const EnumDescriptor* desc, const char* name);

    /**
     * Get <b>*first*</b> enum name for value.
     * @param desc Descriptor
     * @param value Value to get name for
     * @return Enum name. NULL if none found.
     */
    const char* GetEnumName(const EnumDescriptor* desc, int32_t value);

    /**
     * Free message
     * @param message Message
     */
    void FreeMessage(void* message);
}

#endif // DM_DDF_H
