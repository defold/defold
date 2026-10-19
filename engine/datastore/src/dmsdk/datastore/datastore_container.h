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

#ifndef DMSDK_DATASTORE_CONTAINER_H
#define DMSDK_DATASTORE_CONTAINER_H

#include <dmsdk/datastore/datastore_iter.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct DSValueView
    {
        // private
        uint64_t m_Storage[9];
    } DSValueView;

    DSResult    DSFieldGetView(HDSContext store, DSId id, uint64_t field, DSValueView* out_view);
    void        DSRowIterGetView(const DSRowIterator* iterator, uint32_t field, DSValueView* out_view);
    DSValueType DSViewGetType(const DSValueView* view);
    DSResult    DSViewGetCount(const DSValueView* view, uint32_t* out_count);
    DSResult    DSViewGetField(const DSValueView* view, uint64_t field, DSValueView* out_view);
    DSResult    DSViewGetElement(const DSValueView* view, uint32_t index, DSValueView* out_view);
    DSResult    DSViewGetNumber(const DSValueView* view, double* out_value);
    DSResult    DSViewGetFloat(const DSValueView* view, float* out_value);
    DSResult    DSViewGetUInt32(const DSValueView* view, uint32_t* out_value);
    DSResult    DSViewGetBoolean(const DSValueView* view, uint8_t* out_value);
    DSResult    DSViewGetString(const DSValueView* view, const char** out_value);
    DSResult    DSViewGetVector3(const DSValueView* view, DSVector3* out_value);
    DSResult    DSViewGetVector4(const DSValueView* view, DSVector4* out_value);
    DSResult    DSViewGetMatrix4(const DSValueView* view, DSMatrix4* out_value);

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
 * @name DSContainer
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
 * @name DSValueView
 */

/*# Read a field as a view
 *
 * @name DSFieldGetView
 * @param store [type:HDSContext] Store handle.
 * @param id [type:DSId] Row ID.
 * @param field [type:uint64_t] Full field-name hash.
 * @param out_view [type:DSValueView*] Receives a borrowed view on success.
 * @return result [type:DSResult] OK, or NOT_FOUND for an absent row/field.
 */

/*# Read a bound query field as a view
 *
 * Uses the same valid-row and matching-handle contract as typed pointer getters.
 * Containers and strings require read-only access. No name lookup or allocation.
 *
 * @name DSRowIterGetView
 * @param iterator [type:const DSRowIterator*] Current row of a locked or reserved query.
 * @param field [type:uint32_t] Handle returned by DSQueryFindField.
 * @param out_view [type:DSValueView*] Receives a borrowed view.
 */

/*# Read a view's kind
 *
 * @name DSViewGetType
 * @param view [type:const DSValueView*] Valid view.
 * @return type [type:DSValueType] Value kind, including NULL for a null element.
 */

/*# Count container entries
 *
 * @name DSViewGetCount
 * @param view [type:const DSValueView*] Struct or list view.
 * @param out_count [type:uint32_t*] Receives the immediate member/element count.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a non-container.
 */

/*# Read a struct member
 *
 * Uses a local member hash; repeat for further nesting. No member enumeration.
 *
 * @name DSViewGetField
 * @param view [type:const DSValueView*] Struct view, including an inline struct.
 * @param field [type:uint64_t] Local member-name hash.
 * @param out_view [type:DSValueView*] Receives the member view; may alias view.
 * @return result [type:DSResult] OK, NOT_FOUND, or INVALID_ARGUMENT for a non-struct.
 */

/*# Read a list element
 *
 * @name DSViewGetElement
 * @param view [type:const DSValueView*] List view.
 * @param index [type:uint32_t] Zero-based element index.
 * @param out_view [type:DSValueView*] Receives the element view; may alias view.
 * @return result [type:DSResult] OK, NOT_FOUND outside the list, or INVALID_ARGUMENT for a non-list.
 */

/*# Read a Number view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetNumber
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:double*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Float view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetFloat
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:float*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a UInt32 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetUInt32
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:uint32_t*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Boolean view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetBoolean
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:uint8_t*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a String view
 *
 * Borrows the NUL-terminated string until the view expires. Errors leave the output unchanged.
 *
 * @name DSViewGetString
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:const char**] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector3 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetVector3
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:DSVector3*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Vector4 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetVector4
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:DSVector4*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

/*# Read a Matrix4 view
 *
 * Copies the value without conversion. Errors leave the output unchanged.
 *
 * @name DSViewGetMatrix4
 * @param view [type:const DSValueView*] Valid view.
 * @param out_value [type:DSMatrix4*] Receives the value on success.
 * @return result [type:DSResult] OK, or INVALID_ARGUMENT for a kind mismatch.
 */

#endif // DMSDK_DATASTORE_CONTAINER_H
