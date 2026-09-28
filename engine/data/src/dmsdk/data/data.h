// Copyright 2026 The Defold Foundation
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

#ifndef DMSDK_DATA_H
#define DMSDK_DATA_H

#include <stdint.h>

/*# Data API documentation
 *
 * Experimental C API for querying, reading and writing values in an engine-owned data store.
 * Store/table/row management, reset and binary I/O are internal engine operations.
 * Rows are mutable from creation; scalar/math reads and writes do not allocate.
 * Writes preserve iterators and reset defaults. String replacements are copied into owned payload storage
 * retained until the owning registration/table is reset or destroyed internally.
 * Functions use the Data prefix. Copying accessors enforce the declared kind. Pointer access
 * validates the requested kind at binding; the caller must use its corresponding typed getter.
 * Type, tag and property names are caller-provided 64-bit hashes (e.g. dmHashString64).
 * Each table has shared property metadata; all its rows use the same fixed layout and kinds.
 * Lock store structure with [ref:DataStoreLock] while iterating; finish with [ref:DataStoreUnlock].
 * Iterator lifetimes are caller obligations; access does not validate them.
 * Stores are single-threaded; callers must synchronize concurrent access.
 * Allocation follows the engine containers' policy: allocation failure is fatal.
 * All pointers must be valid unless explicitly documented as optional.
 *
 * @document
 * @name Data
 * @language C
 */
#ifdef __cplusplus
extern "C"
{
#endif

    /*# Store handle
     *
     * Borrowed handle to an engine-owned store. The engine controls its lifetime;
     * SDK callers must not destroy it. Destroy queries before the store is shut down.
     *
     * @typedef
     * @name HDataStore
     */
    typedef struct DataStore* HDataStore;

    /*# Query handle
     *
     * Opaque live query created by [ref:DataCreateQuery]. Destroy it with
     * [ref:DataDestroyQuery] before destroying its store. It must outlive its iterators.
     *
     * @typedef
     * @name HDataQuery
     */
    typedef struct DataQuery* HDataQuery;

    /*# Row identifier
     *
     * Store-local uint64_t containing a 32-bit generation and a 32-bit slot index.
     * Zero is invalid. IDs remain valid when other rows move; removed rows' IDs become
     * stale. Never use an ID in another store. IDs are not persisted by serialization.
     *
     * @typedef
     * @name DataId
     */
    typedef uint64_t DataId;

    /*# Logical owner identifier
     *
     * Caller-defined uint64_t. Multiple rows may have the same owner, including zero.
     * An owner does not have to be a game object or a row in this store.
     *
     * @typedef
     * @name DataOwnerId
     */
    typedef uint64_t DataOwnerId;

    /*# Operation results
     *
     * END is normal iterator completion. Output arguments are unchanged on errors unless
     * the operation documents an exception.
     *
     * @enum
     * @name DataResult
     * @member DATA_RESULT_OK Operation succeeded (0).
     * @member DATA_RESULT_END No more batches, rows or fields (1).
     * @member DATA_RESULT_NOT_FOUND Table, row or property was not found (-1).
     * @member DATA_RESULT_LOCKED A structural operation requires an unlocked store (-3).
     * @member DATA_RESULT_INVALID_ARGUMENT Invalid argument, value or count (-4).
     * @member DATA_RESULT_ALREADY_EXISTS Table type or property name is duplicated (-5).
     * @member DATA_RESULT_BUFFER_TOO_SMALL The output buffer is too small (-6).
     * @member DATA_RESULT_INVALID_FORMAT The serialized blob is malformed or has an unsupported version (-7).
     */
    typedef enum DataResult
    {
        DATA_RESULT_OK = 0,
        DATA_RESULT_END = 1,
        DATA_RESULT_NOT_FOUND = -1,
        DATA_RESULT_LOCKED = -3,
        DATA_RESULT_INVALID_ARGUMENT = -4,
        DATA_RESULT_ALREADY_EXISTS = -5,
        DATA_RESULT_BUFFER_TOO_SMALL = -6,
        DATA_RESULT_INVALID_FORMAT = -7,
    } DataResult;

    /*# Value kinds
     *
     * Supports the six kinds in dmStructDDF::Value, with independent runtime enum values.
     * Structs and lists may nest, including mixed lists and empty containers.
     *
     * @enum
     * @name DataValueType
     * @member DATA_VALUE_TYPE_NUMBER Double-precision number (0).
     * @member DATA_VALUE_TYPE_BOOLEAN Boolean encoded as zero or one (1).
     * @member DATA_VALUE_TYPE_STRING NUL-terminated string (2).
     * @member DATA_VALUE_TYPE_NULL Null value with no payload (3).
     * @member DATA_VALUE_TYPE_STRUCT Named fields (4).
     * @member DATA_VALUE_TYPE_LIST Ordered values (5).
     * @member DATA_VALUE_TYPE_VECTOR3 Three float32 components (6).
     * @member DATA_VALUE_TYPE_VECTOR4 Four float32 components (7).
     * @member DATA_VALUE_TYPE_MATRIX4 Sixteen float32 components in column-major order (8).
     */
    typedef enum DataValueType
    {
        DATA_VALUE_TYPE_NUMBER = 0,
        DATA_VALUE_TYPE_BOOLEAN = 1,
        DATA_VALUE_TYPE_STRING = 2,
        DATA_VALUE_TYPE_NULL = 3,
        DATA_VALUE_TYPE_STRUCT = 4,
        DATA_VALUE_TYPE_LIST = 5,
        DATA_VALUE_TYPE_VECTOR3 = 6,
        DATA_VALUE_TYPE_VECTOR4 = 7,
        DATA_VALUE_TYPE_MATRIX4 = 8,
    } DataValueType;

    /*# Vector3 value
     *
     * Plain C value containing 3 float32 components, also used by the typed field pointer API.
     * Stored-pointer lifetimes follow [ref:DataQueryFindField].
     *
     * @struct
     * @name DataVector3
     * @member m_Values [type:float[3]] X, Y and Z components.
     */
    typedef struct DataVector3
    {
        float m_Values[3];
    } DataVector3;

    /*# Vector4 value
     *
     * Plain C value containing 4 float32 components, also used by the typed field pointer API.
     * Stored-pointer lifetimes follow [ref:DataQueryFindField].
     *
     * @struct
     * @name DataVector4
     * @member m_Values [type:float[4]] X, Y, Z and W components.
     */
    typedef struct DataVector4
    {
        float m_Values[4];
    } DataVector4;

    /*# Matrix4 value
     *
     * Plain C value containing 16 float32 components, also used by the typed field pointer API.
     * Stored-pointer lifetimes follow [ref:DataQueryFindField].
     *
     * @struct
     * @name DataMatrix4
     * @member m_Values [type:float[16]] Column-major components; element at row r, column c is m_Values[c * 4 + r].
     */
    typedef struct DataMatrix4
    {
        float m_Values[16];
    } DataMatrix4;

    /*# Required query property
     *
     * Matches a property path by name and exact declared leaf kind. With an empty
     * m_Path, matches a top-level property. Otherwise each path element names a
     * member of a declared inline STRUCT. The query resolves the complete byte
     * offset once per matching table. Dynamic structs/lists cannot bind member paths.
     * All requested properties must belong to the same table; values are not tested.
     * Zero-initialize before assigning fields. For light.color, set m_Property to
     * the light hash, m_Type to VECTOR3, and m_Path to one color hash. Read/write
     * that field with DataFieldIterGetVector3 / DataFieldIterSetVector3.
     * Nested writes preserve siblings; internal reset of the root property restores
     * all its original members. Paths and other filter arrays need only live through creation.
     *
     * @struct
     * @name DataQueryProperty
     * @member m_Property [type:uint64_t] Required property name hash.
     * @member m_Type [type:DataValueType] Required kind of the final property.
     * @member m_Path [type:const uint64_t*] Member name hashes below m_Property, copied during query creation; NULL for a top-level property.
     * @member m_PathCount [type:uint32_t] Number of nested member hashes; zero for a top-level property, at most 64.
     */
    typedef struct DataQueryProperty
    {
        uint64_t        m_Property;
        DataValueType   m_Type;
        const uint64_t* m_Path;
        uint32_t        m_PathCount;
    } DataQueryProperty;

    /*# Query filter descriptor
     *
     * Copied by [ref:DataCreateQuery]. Empty filters match all owners or tables.
     * All requested tags and properties must be present on a table; a row may match any requested owner.
     * Property bindings are resolved once per matching table. Requested properties form
     * a filter set; their order does not specify field iteration order. A table missing
     * a requested name/path or exact kind does not match.
     * Initialize the complete descriptor to zero before assigning filters.
     *
     * @struct
     * @name DataQueryDesc
     * @member m_OwnerIds [type:const DataOwnerId*] Owner IDs to match; may be NULL when m_OwnerIdCount is zero.
     * @member m_OwnerIdCount [type:uint32_t] Number of owner IDs; zero matches every owner.
     * @member m_AllTags [type:const uint64_t*] Required table tags; may be NULL when m_AllTagCount is zero.
     * @member m_AllTagCount [type:uint32_t] Number of required tags; zero imposes no tag restriction.
     * @member m_Properties [type:const DataQueryProperty*] Required properties to bind; may be NULL when m_PropertyCount is zero.
     * @member m_PropertyCount [type:uint32_t] Number of required properties; zero imposes no property restriction.
     */
    typedef struct DataQueryDesc
    {
        const DataOwnerId*       m_OwnerIds;
        uint32_t                 m_OwnerIdCount;
        const uint64_t*          m_AllTags;
        uint32_t                 m_AllTagCount;
        const DataQueryProperty* m_Properties;
        uint32_t                 m_PropertyCount;
    } DataQueryDesc;

    /*# Query iterator
     *
     * Stack-owned, allocation-free cursor initialized by [ref:DataQueryIter]. The store
     * must remain locked with [ref:DataStoreLock] for the entire traversal. The query
     * and store must outlive every iterator. Do not copy an active iterator.
     * Create rows with [ref:DataIterRows]. Advancing this cursor ends the usable lifetime
     * of its children, including on END. Parents must remain alive at the same address.
     * These lifetime rules are caller obligations; iterator access does not validate them.
     * Owner filters may split a table into contiguous batches. Batch and row order are unspecified.
     * Field order is also unspecified; identify fields by name and kind.
     *
     * @struct
     * @name DataIterator
     */
    typedef struct DataIterator
    {
        // private
        HDataQuery  m_Query;
        uint32_t    m_TableIndex;
        uint32_t    m_NextRow;
        uint32_t    m_StartRow;
        uint32_t    m_Count;
        void*       m_Table;
        void*       m_Rows;
        const void* m_Fields;
        uint8_t*    m_Values;
        uint32_t    m_RowStride;
        uint32_t    m_FieldCount;
    } DataIterator;

    /*# Row iterator
     *
     * Stack-owned, allocation-free cursor over the parent's current batch. Advance with
     * [ref:DataRowIterNext], then inspect its ID/owner or create a [ref:DataRowIterFields]
     * iterator. Do not copy an active iterator. Finish using fields before stepping this
     * cursor, and finish using this cursor before stepping its parent batch, even on END.
     * Keep the store locked and all parents alive at the same address during use.
     * Lifetimes are caller obligations and are not checked by iterator access.
     * Writes and resets preserve iteration. Members below the private marker are internal.
     *
     * @struct
     * @name DataRowIterator
     */
    typedef struct DataRowIterator
    {
        // private
        const DataIterator* m_Parent;
        uint32_t            m_Index;
        uint32_t            m_NextRow;
    } DataRowIterator;

    /*# Field iterator
     *
     * Stack-owned, allocation-free cursor over the parent's current row. Visits requested
     * query properties, including bound inline member paths; otherwise visits all top-level
     * properties. Field order is unspecified. Does not recursively traverse containers.
     * Advance with [ref:DataFieldIterNext], then use typed get/set or inspect metadata with
     * [ref:DataFieldIterGetType] and [ref:DataFieldIterGetNameHash].
     * Do not copy an active iterator. Finish using it before advancing either parent.
     * Keep the store locked and parents alive at the same address. These lifetime rules
     * are not checked at runtime. Writes and resets preserve it; reads observe current values.
     * Capturing the current row index does not extend its lifetime beyond a parent step.
     * Members below the private marker must not be accessed or modified.
     *
     * @struct
     * @name DataFieldIterator
     * @member m_Index [type:uint32_t] Read-only index in the requested fields, or table metadata when none are requested; UINT32_MAX without a current field.
     */
    typedef struct DataFieldIterator
    {
        uint32_t m_Index;

        // private
        uint32_t            m_NextField;
        const DataIterator* m_Batch;
        uint32_t            m_RowIndex;
    } DataFieldIterator;

    /*# Lock store structure
     *
     * Holds table membership, row storage and query bindings stable for iteration.
     * Call before [ref:DataQueryIter] and keep locked until all iterators are finished.
     * Locks nest; every call requires a matching [ref:DataStoreUnlock], including on
     * early loop exits. Value writes and resets remain allowed. Structural engine
     * operations return LOCKED without changing the store while any lock is held.
     * This is a single-threaded structural guard, not a mutex. Callers synchronize threads.
     * Does not allocate or queue operations.
     *
     * @name DataStoreLock
     * @param store [type:HDataStore] Store to lock; must outlive the matching unlock.
     */
    void DataStoreLock(HDataStore store);

    /*# Unlock store structure
     *
     * Releases one [ref:DataStoreLock]. The outermost unlock permits structural changes;
     * all iteration must have finished before that unlock. Does not apply queued work.
     * Engine callers may collect row IDs during traversal and remove them afterward.
     *
     * @name DataStoreUnlock
     * @param store [type:HDataStore] Store with a matching outstanding lock.
     */
    void DataStoreUnlock(HDataStore store);

    /*# Read a Number property
     *
     * Requires the declared kind NUMBER. Reads without allocation; no conversion is performed.
     *
     * @name DataGetPropertyNumber
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param out_value [type:double*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for a kind mismatch.
     */
    DataResult DataGetPropertyNumber(HDataStore store, DataId id, uint64_t property, double* out_value);

    /*# Write a Number property
     *
     * Requires the declared kind NUMBER; does not add properties or change reset defaults.
     * Preserves iterators and reset defaults; scalar/math writes use existing mutable row storage.
     * On error, the stored value is unchanged.
     *
     * @name DataSetPropertyNumber
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param value [type:double] Replacement value.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for an invalid value or kind mismatch.
     */
    DataResult DataSetPropertyNumber(HDataStore store, DataId id, uint64_t property, double value);

    /*# Read a Boolean property
     *
     * Requires the declared kind BOOLEAN. Reads without allocation; no conversion is performed.
     *
     * @name DataGetPropertyBoolean
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param out_value [type:uint8_t*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for a kind mismatch.
     */
    DataResult DataGetPropertyBoolean(HDataStore store, DataId id, uint64_t property, uint8_t* out_value);

    /*# Write a Boolean property
     *
     * Requires the declared kind BOOLEAN; does not add properties or change reset defaults.
     * Preserves iterators and reset defaults; scalar/math writes use existing mutable row storage.
     * On error, the stored value is unchanged.
     * The value must be zero or one.
     *
     * @name DataSetPropertyBoolean
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param value [type:uint8_t] Replacement value.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for an invalid value or kind mismatch.
     */
    DataResult DataSetPropertyBoolean(HDataStore store, DataId id, uint64_t property, uint8_t value);

    /*# Read a String property
     *
     * Requires the declared kind STRING. Reads without allocation; no conversion is performed.
     *
     * @name DataGetPropertyString
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param out_value [type:const char**] Receives the value on success; unchanged on error. Borrowed string; copy before a store write, reset or iterator step.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for a kind mismatch.
     */
    DataResult DataGetPropertyString(HDataStore store, DataId id, uint64_t property, const char** out_value);

    /*# Write a String property
     *
     * Requires the declared kind STRING; does not add properties or change reset defaults.
     * Preserves iterators and reset defaults; scalar/math writes use existing mutable row storage.
     * On error, the stored value is unchanged.
     * Copies the NUL-terminated string into owned payload storage for this registration/table, including self-assignment.
     *
     * @name DataSetPropertyString
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param value [type:const char*] Replacement value to copy; the pointer is not retained.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for an invalid value or kind mismatch.
     */
    DataResult DataSetPropertyString(HDataStore store, DataId id, uint64_t property, const char* value);

    /*# Read a Vector3 property
     *
     * Requires the declared kind VECTOR3. Reads without allocation; no conversion is performed.
     *
     * @name DataGetPropertyVector3
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param out_value [type:DataVector3*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for a kind mismatch.
     */
    DataResult DataGetPropertyVector3(HDataStore store, DataId id, uint64_t property, DataVector3* out_value);

    /*# Write a Vector3 property
     *
     * Requires the declared kind VECTOR3; does not add properties or change reset defaults.
     * Preserves iterators and reset defaults; scalar/math writes use existing mutable row storage.
     * On error, the stored value is unchanged.
     *
     * @name DataSetPropertyVector3
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param value [type:const DataVector3*] Replacement value to copy; the pointer is not retained.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for an invalid value or kind mismatch.
     * @examples
     * ```c
     * #include <dmsdk/data/data.h>
     *
     * DataResult SetLightColor(HDataStore store, DataId id, uint64_t color_property, float r, float g, float b)
     * {
     *     DataVector3 value = { { r, g, b } };
     *     return DataSetPropertyVector3(store, id, color_property, &value);
     * }
     * ```
     * The table metadata must declare color_property as VECTOR3. Existing DDF lists
     * remain lists until the producer explicitly converts them to this representation.
     */
    DataResult DataSetPropertyVector3(HDataStore store, DataId id, uint64_t property, const DataVector3* value);

    /*# Read a Vector4 property
     *
     * Requires the declared kind VECTOR4. Reads without allocation; no conversion is performed.
     *
     * @name DataGetPropertyVector4
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param out_value [type:DataVector4*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for a kind mismatch.
     */
    DataResult DataGetPropertyVector4(HDataStore store, DataId id, uint64_t property, DataVector4* out_value);

    /*# Write a Vector4 property
     *
     * Requires the declared kind VECTOR4; does not add properties or change reset defaults.
     * Preserves iterators and reset defaults; scalar/math writes use existing mutable row storage.
     * On error, the stored value is unchanged.
     *
     * @name DataSetPropertyVector4
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param value [type:const DataVector4*] Replacement value to copy; the pointer is not retained.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for an invalid value or kind mismatch.
     */
    DataResult DataSetPropertyVector4(HDataStore store, DataId id, uint64_t property, const DataVector4* value);

    /*# Read a Matrix4 property
     *
     * Requires the declared kind MATRIX4. Reads without allocation; no conversion is performed.
     *
     * @name DataGetPropertyMatrix4
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param out_value [type:DataMatrix4*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for a kind mismatch.
     */
    DataResult DataGetPropertyMatrix4(HDataStore store, DataId id, uint64_t property, DataMatrix4* out_value);

    /*# Write a Matrix4 property
     *
     * Requires the declared kind MATRIX4; does not add properties or change reset defaults.
     * Preserves iterators and reset defaults; scalar/math writes use existing mutable row storage.
     * On error, the stored value is unchanged.
     *
     * @name DataSetPropertyMatrix4
     * @param store [type:HDataStore] Store handle.
     * @param id [type:DataId] Row ID.
     * @param property [type:uint64_t] Property name hash.
     * @param value [type:const DataMatrix4*] Replacement value to copy; the pointer is not retained.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent row/property, or INVALID_ARGUMENT for an invalid value or kind mismatch.
     */
    DataResult DataSetPropertyMatrix4(HDataStore store, DataId id, uint64_t property, const DataMatrix4* value);

    /*# Create a reusable live query
     *
     * Copies the filters and caches matching tables and property bindings, including later registrations.
     * The caller destroys the query before the engine destroys the store.
     *
     * @name DataCreateQuery
     * @param store [type:HDataStore] Store handle.
     * @param desc [type:const DataQueryDesc*] Owner, required-tag and required-property filters to copy.
     * @param out_query [type:HDataQuery*] Receives the new query on success; unchanged on error.
     * @return result [type:DataResult] OK on success, or INVALID_ARGUMENT for invalid filter arrays/counts or property kinds.
     * @examples
     * ```c
     * #include <dmsdk/data/data.h>
     *
     * DataResult SetOwnerNumber(HDataStore store, DataOwnerId owner, uint64_t property, double value)
     * {
     *     DataQueryProperty field = { property, DATA_VALUE_TYPE_NUMBER };
     *     DataQueryDesc desc = { &owner, 1, 0, 0, &field, 1 };
     *     HDataQuery query;
     *     DataResult result = DataCreateQuery(store, &desc, &query);
     *     if (result != DATA_RESULT_OK)
     *         return result;
     *     DataStoreLock(store);
     *     DataIterator iterator = DataQueryIter(query);
     *     while ((result = DataIterNext(&iterator)) == DATA_RESULT_OK)
     *     {
     *         DataRowIterator rows = DataIterRows(&iterator);
     *         while ((result = DataRowIterNext(&rows)) == DATA_RESULT_OK)
     *         {
     *             DataFieldIterator field = DataRowIterFields(&rows);
     *             result = DataFieldIterNext(&field);
     *             if (result == DATA_RESULT_OK)
     *                 result = DataFieldIterSetNumber(&field, value);
     *             if (result != DATA_RESULT_OK)
     *                 break;
     *         }
     *         if (result != DATA_RESULT_END)
     *             break;
     *     }
     *     DataStoreUnlock(store);
     *     DataDestroyQuery(query);
     *     return result == DATA_RESULT_END ? DATA_RESULT_OK : result;
     * }
     * ```
     * The engine supplies the store. The query matches rows with the requested Number property.
     * An error stops the update; earlier writes remain applied.
     */
    DataResult DataCreateQuery(HDataStore store, const DataQueryDesc* desc, HDataQuery* out_query);

    /*# Destroy a query
     *
     * Frees its copied filters, cached tables and property bindings. Its iterators must no longer be used.
     *
     * @name DataDestroyQuery
     * @param query [type:HDataQuery] Query to destroy; its store must still be alive.
     */
    void DataDestroyQuery(HDataQuery query);

    /*# Start query iteration
     *
     * Creates a fresh iterator over the current store state without allocation.
     * The store must already be locked with [ref:DataStoreLock] and remain locked
     * until traversal finishes, including on early exits. Do not copy active iterators.
     *
     * @name DataQueryIter
     * @param query [type:HDataQuery] Query that must outlive the iterator.
     * @return iterator [type:DataIterator] Iterator with no current batch; advance using DataIterNext.
     */
    DataIterator DataQueryIter(HDataQuery query);

    /*# Advance to the next batch
     *
     * Returns a nonempty contiguous batch from one table. Clears the current batch count
     * on END. Finish using child row and field iterators before every call.
     * No structural or parent-lifetime checks are performed during traversal.
     *
     * @name DataIterNext
     * @param iterator [type:DataIterator*] Iterator to advance; its query and store must still be alive.
     * @return result [type:DataResult] OK for a new batch, or END when exhausted.
     */
    DataResult DataIterNext(DataIterator* iterator);

    /*# Get the current table type
     *
     * @name DataIterGetType
     * @param iterator [type:const DataIterator*] Iterator whose query and store are still alive.
     * @return type [type:uint64_t] Table type hash, or zero without a valid current batch. Zero may also be a valid type hash.
     */
    uint64_t DataIterGetType(const DataIterator* iterator);

    /*# Start row iteration
     *
     * Borrows the current batch without allocation. Any DataIterNext call on the
     * parent invalidates this cursor and its fields. An absent batch yields an empty
     * row iterator. The parent must remain alive at the same address and must not be
     * replaced while children are used. The query/store must also remain alive.
     *
     * @name DataIterRows
     * @param iterator [type:const DataIterator*] Parent query iterator to borrow.
     * @return rows [type:DataRowIterator] Iterator with no current row; advance using DataRowIterNext.
     */
    static inline DataRowIterator DataIterRows(const DataIterator* iterator)
    {
        DataRowIterator rows = { iterator, UINT32_MAX, 0 };
        return rows;
    }

    /*# Advance to the next row
     *
     * Clears the current row on END. Finish using child field iterators before
     * every call. Does not check the store lock or parent lifetime.
     *
     * @name DataRowIterNext
     * @param iterator [type:DataRowIterator*] Row iterator whose parents, query and store are still alive.
     * @return result [type:DataResult] OK for a row, or END when exhausted.
     */
    static inline DataResult DataRowIterNext(DataRowIterator* iterator)
    {
        if (iterator->m_NextRow >= iterator->m_Parent->m_Count)
        {
            iterator->m_Index = UINT32_MAX;
            return DATA_RESULT_END;
        }
        iterator->m_Index = iterator->m_NextRow++;
        return DATA_RESULT_OK;
    }

    /*# Get the current row ID
     *
     * @name DataRowIterGetId
     * @param iterator [type:const DataRowIterator*] Row iterator whose parents, query and store are still alive.
     * @return id [type:DataId] Store-local row ID, or zero without a valid current row.
     */
    DataId DataRowIterGetId(const DataRowIterator* iterator);

    /*# Get the current row owner
     *
     * @name DataRowIterGetOwnerId
     * @param iterator [type:const DataRowIterator*] Row iterator whose parents, query and store are still alive.
     * @return owner [type:DataOwnerId] Current owner, or zero without a valid row. Zero may also be a valid owner.
     */
    DataOwnerId DataRowIterGetOwnerId(const DataRowIterator* iterator);

    /*# Find a query field for direct pointer access
     *
     * Resolves a requested property by its full path and exact kind. Call once after
     * query creation and reuse the handle across batches and subsequent traversals.
     * Matching tables cache their own byte offsets under the same handle. It does not
     * specify a byte offset, descriptor position or field iteration order.
     * The query need not match any tables or rows yet, and no store lock is required
     * for this lookup. Adding/removing tables and rows does not invalidate the handle;
     * it belongs to this query and lasts until DataDestroyQuery.
     * Only Number, Boolean, Vector3, Vector4 and Matrix4 support pointer access.
     * Strings and containers retain their ownership-aware copying accessors.
     * No allocation or value copy occurs. Pointer access requires a little-endian host.
     *
     * Typed pointer getters require a handle of their corresponding kind and a current
     * row from this query, with the store locked. They do not repeat kind, bounds or
     * lifetime validation. Returned pointers are borrowed. Finish using them before
     * advancing the row/batch, unlocking, or calling another mutating accessor or reset
     * for this row/table. Never write through a read pointer. The handle itself remains valid across these operations.
     *
     * @name DataQueryFindField
     * @param query [type:HDataQuery] Query whose requested properties to search; must remain alive.
     * @param property [type:const DataQueryProperty*] Requested property path and kind; borrowed only for this call.
     * @return field [type:uint32_t] Query-local field handle, or UINT32_MAX for an absent property, mismatched kind, invalid path, unsupported kind or host byte order.
     */
    uint32_t DataQueryFindField(HDataQuery query, const DataQueryProperty* property);

    // private
    // Linkage for the public pointer wrappers. Validated field handles and current
    // row indices are caller obligations; no repeated checks occur here.
    const void* DataGetFieldPointerInternal(const DataIterator* batch, uint32_t row, uint32_t field);
    void*       DataGetFieldPointerMutInternal(const DataIterator* batch, uint32_t row, uint32_t field);

    /*# Read a Number through a field pointer
     *
     * Borrows the current mutable row value as read-only, without allocation or copying.
     * Requires a Number handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetNumber
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Number field handle for the parent query.
     * @return value [type:const double*] Borrowed read-only value; non-NULL when the preconditions hold.
     */
    static inline const double* DataFieldGetNumber(const DataRowIterator* iterator, uint32_t field)
    {
        return (const double*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Write a Number through a field pointer
     *
     * Returns the existing instance-owned value without allocation or copying.
     * Writes affect only this field; siblings and the separate reset defaults are preserved.
     * Requires a Number handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetNumberMut
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Number field handle for the parent query.
     * @return value [type:double*] Borrowed writable value; non-NULL when the preconditions hold.
     */
    static inline double* DataFieldGetNumberMut(const DataRowIterator* iterator, uint32_t field)
    {
        return (double*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Read a Boolean through a field pointer
     *
     * Borrows the current mutable row value as read-only, without allocation or copying.
     * Requires a Boolean handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetBoolean
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Boolean field handle for the parent query.
     * @return value [type:const uint8_t*] Borrowed read-only value; non-NULL when the preconditions hold.
     */
    static inline const uint8_t* DataFieldGetBoolean(const DataRowIterator* iterator, uint32_t field)
    {
        return (const uint8_t*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Write a Boolean through a field pointer
     *
     * Returns the existing instance-owned value without allocation or copying.
     * Writes affect only this field; siblings and the separate reset defaults are preserved.
     * Write only zero or one to the returned byte.
     * Requires a Boolean handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetBooleanMut
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Boolean field handle for the parent query.
     * @return value [type:uint8_t*] Borrowed writable value; non-NULL when the preconditions hold.
     */
    static inline uint8_t* DataFieldGetBooleanMut(const DataRowIterator* iterator, uint32_t field)
    {
        return (uint8_t*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Read a Vector3 through a field pointer
     *
     * Borrows the current mutable row value as read-only, without allocation or copying.
     * Requires a Vector3 handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetVector3
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Vector3 field handle for the parent query.
     * @return value [type:const DataVector3*] Borrowed read-only value; non-NULL when the preconditions hold.
     */
    static inline const DataVector3* DataFieldGetVector3(const DataRowIterator* iterator, uint32_t field)
    {
        return (const DataVector3*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Write a Vector3 through a field pointer
     *
     * Returns the existing instance-owned value without allocation or copying.
     * Writes affect only this field; siblings and the separate reset defaults are preserved.
     * Requires a Vector3 handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetVector3Mut
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Vector3 field handle for the parent query.
     * @return value [type:DataVector3*] Borrowed writable value; non-NULL when the preconditions hold.
     */
    static inline DataVector3* DataFieldGetVector3Mut(const DataRowIterator* iterator, uint32_t field)
    {
        return (DataVector3*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Read a Vector4 through a field pointer
     *
     * Borrows the current mutable row value as read-only, without allocation or copying.
     * Requires a Vector4 handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetVector4
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Vector4 field handle for the parent query.
     * @return value [type:const DataVector4*] Borrowed read-only value; non-NULL when the preconditions hold.
     */
    static inline const DataVector4* DataFieldGetVector4(const DataRowIterator* iterator, uint32_t field)
    {
        return (const DataVector4*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Write a Vector4 through a field pointer
     *
     * Returns the existing instance-owned value without allocation or copying.
     * Writes affect only this field; siblings and the separate reset defaults are preserved.
     * Requires a Vector4 handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetVector4Mut
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Vector4 field handle for the parent query.
     * @return value [type:DataVector4*] Borrowed writable value; non-NULL when the preconditions hold.
     */
    static inline DataVector4* DataFieldGetVector4Mut(const DataRowIterator* iterator, uint32_t field)
    {
        return (DataVector4*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Read a Matrix4 through a field pointer
     *
     * Borrows the current mutable row value as read-only, without allocation or copying.
     * Requires a Matrix4 handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetMatrix4
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Matrix4 field handle for the parent query.
     * @return value [type:const DataMatrix4*] Borrowed read-only value; non-NULL when the preconditions hold.
     */
    static inline const DataMatrix4* DataFieldGetMatrix4(const DataRowIterator* iterator, uint32_t field)
    {
        return (const DataMatrix4*)DataGetFieldPointerInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Write a Matrix4 through a field pointer
     *
     * Returns the existing instance-owned value without allocation or copying.
     * Writes affect only this field; siblings and the separate reset defaults are preserved.
     * Requires a Matrix4 handle from [ref:DataQueryFindField] for this query and a current
     * row. Its lock, type and pointer lifetime requirements apply without further checks.
     *
     * @name DataFieldGetMatrix4Mut
     * @param iterator [type:const DataRowIterator*] Row iterator positioned on a row in the locked batch.
     * @param field [type:uint32_t] Successful Matrix4 field handle for the parent query.
     * @return value [type:DataMatrix4*] Borrowed writable value; non-NULL when the preconditions hold.
     */
    static inline DataMatrix4* DataFieldGetMatrix4Mut(const DataRowIterator* iterator, uint32_t field)
    {
        return (DataMatrix4*)DataGetFieldPointerMutInternal(iterator->m_Parent, iterator->m_Index, field);
    }

    /*# Start field iteration
     *
     * Borrows the current row without allocation. Visits requested query fields, or
     * all top-level fields when no properties were requested. Field order is unspecified;
     * identify fields by name/type instead of assuming the next field has a particular name.
     * Any step of the parent row or batch invalidates this cursor. Both parents must
     * remain alive at the same address and must not be replaced while it is used.
     * An absent row yields an empty field iterator. Cursor lifetimes are caller obligations;
     * no store revision or parent-step checks are performed.
     *
     * @name DataRowIterFields
     * @param iterator [type:const DataRowIterator*] Row iterator whose parents, query and store are still alive.
     * @return fields [type:DataFieldIterator] Iterator with no current field; advance using DataFieldIterNext.
     */
    static inline DataFieldIterator DataRowIterFields(const DataRowIterator* iterator)
    {
        DataFieldIterator fields = { UINT32_MAX, iterator->m_Index == UINT32_MAX ? iterator->m_Parent->m_FieldCount : 0, iterator->m_Parent, iterator->m_Index };
        return fields;
    }

    /*# Advance to the next field
     *
     * Advances only the field index; metadata and row bytes are accessed by the getters.
     * Sets m_Index to UINT32_MAX on END, leaving no readable or writable current field.
     *
     * @name DataFieldIterNext
     * @param iterator [type:DataFieldIterator*] Field iterator whose parents, query and store are still alive.
     * @return result [type:DataResult] OK for a field, or END when exhausted.
     */
    static inline DataResult DataFieldIterNext(DataFieldIterator* iterator)
    {
        if (iterator->m_NextField == iterator->m_Batch->m_FieldCount)
        {
            iterator->m_Index = UINT32_MAX;
            return DATA_RESULT_END;
        }
        iterator->m_Index = iterator->m_NextField++;
        return DATA_RESULT_OK;
    }

    /*# Get the current field kind
     *
     * Reads the shared metadata without allocation. Cursor lifetime rules apply.
     *
     * @name DataFieldIterGetType
     * @param iterator [type:const DataFieldIterator*] Field iterator whose parents, query and store are still alive.
     * @return type [type:DataValueType] Declared kind, or NULL without a current field. NULL may also be a declared kind.
     */
    DataValueType DataFieldIterGetType(const DataFieldIterator* iterator);

    /*# Get the current field name hash
     *
     * Reads the shared metadata without allocation. For an inline member path, returns
     * the final member name. Cursor lifetime rules apply.
     *
     * @name DataFieldIterGetNameHash
     * @param iterator [type:const DataFieldIterator*] Field iterator whose parents, query and store are still alive.
     * @return name [type:uint64_t] Name hash, or zero without a current field. Zero may also be a valid name hash.
     */
    uint64_t DataFieldIterGetNameHash(const DataFieldIterator* iterator);

    /*# Read a Vector3 member of a struct query field
     *
     * Uses the current field declared STRUCT, then finds its immediate
     * member by name. Requires the member kind VECTOR3. Reads current mutable
     * bytes without allocation and copies the three floats into the output value.
     * Dynamic member layouts may differ between rows; this does not change query matching.
     * For a declared inline member, bind its path in DataQueryProperty and use
     * DataFieldIterGetVector3 to avoid repeating member lookup for every row.
     *
     * @name DataFieldIterGetStructPropertyVector3
     * @param iterator [type:const DataFieldIterator*] Field iterator whose parents, query and store are still alive.
     * @param property [type:uint64_t] Immediate struct member name hash, e.g. color in the light field.
     * @param out_value [type:DataVector3*] Receives a copy on success; unchanged on error.
     * @return result [type:DataResult] OK, NOT_FOUND for an absent member, or INVALID_ARGUMENT for an absent current field or mismatched kind.
     */
    DataResult DataFieldIterGetStructPropertyVector3(const DataFieldIterator* iterator, uint64_t property, DataVector3* out_value);

    // private
    // Linkage for the inline typed getters below, not standalone SDK entry points.
    // Arguments borrow the locked batch and use its row/requested-field indices.
    // UINT32_MAX denotes no current field; outputs remain unchanged on errors.
    DataResult DataGetFieldNumberInternal(const DataIterator* batch, uint32_t row, uint32_t field, double* out_value);
    DataResult DataGetFieldBooleanInternal(const DataIterator* batch, uint32_t row, uint32_t field, uint8_t* out_value);
    DataResult DataGetFieldStringInternal(const DataIterator* batch, uint32_t row, uint32_t field, const char** out_value);
    DataResult DataGetFieldVector3Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataVector3* out_value);
    DataResult DataGetFieldVector4Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataVector4* out_value);
    DataResult DataGetFieldMatrix4Internal(const DataIterator* batch, uint32_t row, uint32_t field, DataMatrix4* out_value);

    /*# Read a Number field
     *
     * Requires the declared kind NUMBER. Reads without allocation; no conversion is performed.
     *
     * @name DataFieldIterGetNumber
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param out_value [type:double*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field or kind mismatch.
     */
    static inline DataResult DataFieldIterGetNumber(const DataFieldIterator* iterator, double* out_value)
    {
        return DataGetFieldNumberInternal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    /*# Write a Number field
     *
     * Requires the declared kind NUMBER; does not add properties or change reset defaults.
     * Uses the cached query binding. Preserves iterators and reset defaults; fixed-size writes use existing row storage.
     * On error, the stored value is unchanged.
     *
     * @name DataFieldIterSetNumber
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param value [type:double] Replacement value.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field, value or kind mismatch.
     */
    DataResult DataFieldIterSetNumber(const DataFieldIterator* iterator, double value);

    /*# Read a Boolean field
     *
     * Requires the declared kind BOOLEAN. Reads without allocation; no conversion is performed.
     *
     * @name DataFieldIterGetBoolean
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param out_value [type:uint8_t*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field or kind mismatch.
     */
    static inline DataResult DataFieldIterGetBoolean(const DataFieldIterator* iterator, uint8_t* out_value)
    {
        return DataGetFieldBooleanInternal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    /*# Write a Boolean field
     *
     * Requires the declared kind BOOLEAN; does not add properties or change reset defaults.
     * Uses the cached query binding. Preserves iterators and reset defaults; fixed-size writes use existing row storage.
     * On error, the stored value is unchanged.
     * The value must be zero or one.
     *
     * @name DataFieldIterSetBoolean
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param value [type:uint8_t] Replacement value.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field, value or kind mismatch.
     */
    DataResult DataFieldIterSetBoolean(const DataFieldIterator* iterator, uint8_t value);

    /*# Read a String field
     *
     * Requires the declared kind STRING. Reads without allocation; no conversion is performed.
     *
     * @name DataFieldIterGetString
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param out_value [type:const char**] Receives the value on success; unchanged on error. Borrowed string; copy before a store write, reset or iterator step.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field or kind mismatch.
     */
    static inline DataResult DataFieldIterGetString(const DataFieldIterator* iterator, const char** out_value)
    {
        return DataGetFieldStringInternal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    /*# Write a String field
     *
     * Requires the declared kind STRING; does not add properties or change reset defaults.
     * Uses the cached query binding. Preserves iterators and reset defaults; fixed-size writes use existing row storage.
     * On error, the stored value is unchanged.
     * Copies the NUL-terminated string into owned payload storage for this registration/table, including self-assignment.
     *
     * @name DataFieldIterSetString
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param value [type:const char*] Replacement value to copy; the pointer is not retained.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field, value or kind mismatch.
     */
    DataResult DataFieldIterSetString(const DataFieldIterator* iterator, const char* value);

    /*# Read a Vector3 field
     *
     * Requires the declared kind VECTOR3. Reads without allocation; no conversion is performed.
     *
     * @name DataFieldIterGetVector3
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param out_value [type:DataVector3*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field or kind mismatch.
     */
    static inline DataResult DataFieldIterGetVector3(const DataFieldIterator* iterator, DataVector3* out_value)
    {
        return DataGetFieldVector3Internal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    /*# Write a Vector3 field
     *
     * Requires the declared kind VECTOR3; does not add properties or change reset defaults.
     * Uses the cached query binding. Preserves iterators and reset defaults; fixed-size writes use existing row storage.
     * On error, the stored value is unchanged.
     *
     * @name DataFieldIterSetVector3
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param value [type:const DataVector3*] Replacement value to copy; the pointer is not retained.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field, value or kind mismatch.
     */
    DataResult DataFieldIterSetVector3(const DataFieldIterator* iterator, const DataVector3* value);

    /*# Read a Vector4 field
     *
     * Requires the declared kind VECTOR4. Reads without allocation; no conversion is performed.
     *
     * @name DataFieldIterGetVector4
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param out_value [type:DataVector4*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field or kind mismatch.
     */
    static inline DataResult DataFieldIterGetVector4(const DataFieldIterator* iterator, DataVector4* out_value)
    {
        return DataGetFieldVector4Internal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    /*# Write a Vector4 field
     *
     * Requires the declared kind VECTOR4; does not add properties or change reset defaults.
     * Uses the cached query binding. Preserves iterators and reset defaults; fixed-size writes use existing row storage.
     * On error, the stored value is unchanged.
     *
     * @name DataFieldIterSetVector4
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param value [type:const DataVector4*] Replacement value to copy; the pointer is not retained.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field, value or kind mismatch.
     */
    DataResult DataFieldIterSetVector4(const DataFieldIterator* iterator, const DataVector4* value);

    /*# Read a Matrix4 field
     *
     * Requires the declared kind MATRIX4. Reads without allocation; no conversion is performed.
     *
     * @name DataFieldIterGetMatrix4
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param out_value [type:DataMatrix4*] Receives the value on success; unchanged on error.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field or kind mismatch.
     */
    static inline DataResult DataFieldIterGetMatrix4(const DataFieldIterator* iterator, DataMatrix4* out_value)
    {
        return DataGetFieldMatrix4Internal(iterator->m_Batch, iterator->m_RowIndex, iterator->m_Index, out_value);
    }

    /*# Write a Matrix4 field
     *
     * Requires the declared kind MATRIX4; does not add properties or change reset defaults.
     * Uses the cached query binding. Preserves iterators and reset defaults; fixed-size writes use existing row storage.
     * On error, the stored value is unchanged.
     *
     * @name DataFieldIterSetMatrix4
     * @param iterator [type:const DataFieldIterator*] Iterator positioned on a field; its parents, query and store must still be alive.
     * @param value [type:const DataMatrix4*] Replacement value to copy; the pointer is not retained.
     * @return result [type:DataResult] OK, or INVALID_ARGUMENT for an absent current field, value or kind mismatch.
     */
    DataResult DataFieldIterSetMatrix4(const DataFieldIterator* iterator, const DataMatrix4* value);

#ifdef __cplusplus
}
#endif

#endif // DMSDK_DATA_H
