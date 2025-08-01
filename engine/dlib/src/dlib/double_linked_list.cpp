// Copyright 2020-2025 The Defold Foundation
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

#include <dlib/double_linked_list.h>

namespace dmDoubleLinkedList
{
    void ListInit(List* list)
    {
        list->m_Head.m_Prev = 0;
        list->m_Tail.m_Next = 0;
        list->m_Head.m_Next = &list->m_Tail;
        list->m_Tail.m_Prev = &list->m_Head;
    }

    void ListRemove(List* list, ListNode* item)
    {
        ListNode* prev = item->m_Prev;
        ListNode* next = item->m_Next;
        item->m_Prev = 0;
        item->m_Next = 0;
        if (prev != 0 && next != 0)
        {
            prev->m_Next = next;
            next->m_Prev = prev;
        }
    }

    void ListAdd(List* list, ListNode* item)
    {
        ListNode* prev = &list->m_Head;
        ListNode* next = prev->m_Next;
        item->m_Prev = prev;
        item->m_Next = next;
        prev->m_Next = item;
        next->m_Prev = item;
    }

    ListNode* ListGetLast(List* list)
    {
        ListNode* last = list->m_Tail.m_Prev;
        return last == &list->m_Head ? 0 : last;
    }

    ListNode* ListGetFirst(List* list)
    {
        ListNode* first = list->m_Head.m_Next;
        return first == &list->m_Tail ? 0 : first;
    }

}