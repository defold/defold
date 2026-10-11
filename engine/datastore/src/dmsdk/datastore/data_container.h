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

#ifndef DMSDK_DATA_CONTAINER_H
#define DMSDK_DATA_CONTAINER_H

#include <dmsdk/datastore/data_iter.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DataValueView
    {
        // private
        uint64_t m_Storage[9];
    } DataValueView;

    DataResult    DataFieldGetView(HDataStore store, DataId id, uint64_t field, DataValueView* out_view);
    void          DataRowIterGetView(const DataRowIterator* iterator, uint32_t field, DataValueView* out_view);
    DataValueType DataViewGetType(const DataValueView* view);
    DataResult    DataViewGetCount(const DataValueView* view, uint32_t* out_count);
    DataResult    DataViewGetField(const DataValueView* view, uint64_t field, DataValueView* out_view);
    DataResult    DataViewGetElement(const DataValueView* view, uint32_t index, DataValueView* out_view);
    DataResult    DataViewGetNumber(const DataValueView* view, double* out_value);
    DataResult    DataViewGetFloat(const DataValueView* view, float* out_value);
    DataResult    DataViewGetUInt32(const DataValueView* view, uint32_t* out_value);
    DataResult    DataViewGetBoolean(const DataValueView* view, uint8_t* out_value);
    DataResult    DataViewGetString(const DataValueView* view, const char** out_value);
    DataResult    DataViewGetVector3(const DataValueView* view, DataVector3* out_value);
    DataResult    DataViewGetVector4(const DataValueView* view, DataVector4* out_value);
    DataResult    DataViewGetMatrix4(const DataValueView* view, DataMatrix4* out_value);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Read dynamic containers
 *
 * Read struct members by hash and heterogeneous list elements by index.
 * Views allocate nothing. Use typed row getters for fixed fields in hot loops.
 *
 * @document
 * @name DataContainer
 * @language C
 */

/*# Borrowed value view
 *
 * Created by the view accessors. Copyable; owns no storage. Finish using views
 * and borrowed strings before a store mutation, row/batch advance, unlock or
 * query end. Synchronize ID-based reads externally; query views use the current
 * reservation. The private representation is not a serialized/native row layout.
 *
 * @struct
 * @name DataValueView
 */

/*# Read a field as a view
 *
 * @name DataFieldGetView
 * @param store [type:HDataStore] Store handle.
 * @param id [type:DataId] Row ID.
 * @param field [type:uint64_t] Full field-name hash.
 * @param out_view [type:DataValueView*] Receives a borrowed view on success.
 * @return result [type:DataResult] OK, or NOT_FOUND for an absent row/field.
 */

/*# Read a bound query field as a view
 *
 * Uses the same valid-row and matching-handle contract as typed pointer getters.
 * Containers and strings require read-only access. No name lookup or allocation.
 *
 * @name DataRowIterGetView
 * @param iterator [type:const DataRowIterator*] Current row of a locked or reserved query.
 * @param field [type:uint32_t] Handle returned by DataQueryFindField.
 * @param out_view [type:DataValueView*] Receives a borrowed view.
 */

/*# Read a view's kind
 *
 * @name DataViewGetType
 * @param view [type:const DataValueView*] Valid view.
 * @return type [type:DataValueType] Value kind, including NULL for a null element.
 */

/*# Count container entries
 *
 * @name DataViewGetCount
 * @param view [type:const DataValueView*] Struct or list view.
 * @param out_count [type:uint32_t*] Receives the immediate member/element count.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for a non-container.
 */

/*# Read a struct member
 *
 * Uses a local member hash; repeat for further nesting. No member enumeration.
 *
 * @name DataViewGetField
 * @param view [type:const DataValueView*] Struct view, including an inline struct.
 * @param field [type:uint64_t] Local member-name hash.
 * @param out_view [type:DataValueView*] Receives the member view; may alias view.
 * @return result [type:DataResult] OK, NOT_FOUND, or INVALID_ARGUMENT for a non-struct.
 */

/*# Read a list element
 *
 * @name DataViewGetElement
 * @param view [type:const DataValueView*] List view.
 * @param index [type:uint32_t] Zero-based element index.
 * @param out_view [type:DataValueView*] Receives the element view; may alias view.
 * @return result [type:DataResult] OK, NOT_FOUND outside the list, or INVALID_ARGUMENT for a non-list.
 */

/*# Read a Number view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataViewGetNumber
 * @param view [type:const DataValueView*] Valid view.
 * @param out_value [type:double*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Float view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataViewGetFloat
 * @param view [type:const DataValueView*] Valid view.
 * @param out_value [type:float*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a UInt32 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataViewGetUInt32
 * @param view [type:const DataValueView*] Valid view.
 * @param out_value [type:uint32_t*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Boolean view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataViewGetBoolean
 * @param view [type:const DataValueView*] Valid view.
 * @param out_value [type:uint8_t*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a String view
 *
 * Borrows the NUL-terminated string until the view expires. Errors leave the output unchanged.
 *
 * @name DataViewGetString
 * @param view [type:const DataValueView*] Valid view.
 * @param out_value [type:const char**] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector3 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataViewGetVector3
 * @param view [type:const DataValueView*] Valid view.
 * @param out_value [type:DataVector3*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector4 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataViewGetVector4
 * @param view [type:const DataValueView*] Valid view.
 * @param out_value [type:DataVector4*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Matrix4 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DataViewGetMatrix4
 * @param view [type:const DataValueView*] Valid view.
 * @param out_value [type:DataMatrix4*] Receives the value on success.
 * @return result [type:DataResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

#endif // DMSDK_DATA_CONTAINER_H
