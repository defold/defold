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

#ifndef DMSDK_DOUBLE_LINKED_LIST_H
#define DMSDK_DOUBLE_LINKED_LIST_H

/*#
 * Double linked list structs and functions
 *
 * @document
 * @name Double linked list structs and functions
 * @namespace dmDoubleLinkedList
 * @path engine/dlib/src/dmsdk/dlib/double_linked_list.h
 * @language C++
 */
namespace dmDoubleLinkedList
{
    /*
     * Double linked list node
     * @struct
     * @name dmDoubleLinkedList::ListNode
     * @member m_Prev [type:dmDoubleLinkedList::ListNode*] pointer to the previous element in the linked list
     * @member m_Next [type:dmDobuleLinkedList::ListNode*] pointer to the next element in the linked list
     */
    struct ListNode
    {
        ListNode()
            : m_Prev(0)
            , m_Next(0)
        { }
        struct ListNode* m_Prev;
        struct ListNode* m_Next;
    };

    /*
     * Double linked list
     * @struct
     * @name dmDoubleLinkedList::List
     * @member m_Head [type:dmDoubleLinkedList::ListNode] the head of the linked list
     * @member m_Tail [type:dmDoubleLinkedList::ListNode] the tail of the linked list
     */
    struct List
    {
        ListNode m_Head;
        ListNode m_Tail;
    };

    /*#
     * Initialize empty double linked list
     * @name dmDoubleLinkedList::ListInit
     * @param list [type:dmDoubleLinkedList::List*] the list
     */
    void ListInit(List* list);

    /*#
     * Remove item from the list. Doesn't deallocate memory allocated for the item.
     * If element is not in the list function doesn't affect linked list.
     * @name dmDoubleLinkedList::ListRemove
     * @param list [type:dmDoubleLinkedList::List*] the list
     * @param item [type:dmDoubleLinkedList::ListNode*] removed item
     */
    void ListRemove(List* list, ListNode* item);

    /*#
     * Added new list element as a head of the list
     * @name dmDoubleLinkedList::ListAdd
     * @param list [type:dmDoubleLinkedList::List*] the list
     * @param item [type:dmDoubleLinkedList::ListNode*] new list element
     */
    void ListAdd(List* list, ListNode* item);

    /*#
     * Get the last element of the list. If the list is empty (tail == head) returns 0.
     * @name dmDoubleLinkedList::ListGetLast
     * @param list [type:dmDoubleLinkedList::List*] the list
     * @return last_element [type:dmDoubleLinkedList::ListNode*] the last element of the list. If the list is empty (tail == head) returns 0
     */
    ListNode* ListGetLast(List* list);

    /*#
     * Get the first element of the list. If the list is empty (tail == head) returns 0.
     * @name dmDoubleLinkedList::ListGetFirst
     * @param list [type:dmDoubleLinkedList::List*] the list
     * @return first_element [type:dmDoubleLinkedList::ListNode*] the first element of the list.  If the list is empty (tail == head) returns 0
     */
    ListNode* ListGetFirst(List* list);
}

#endif
