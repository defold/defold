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

#include "dlib/double_linked_list.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

size_t countListNodes(dmDoubleLinkedList::List* list)
{
    dmDoubleLinkedList::ListNode* node = list->m_Head.m_Next;
    size_t nodes_count = 0;
    while (node != &list->m_Tail)
    {
        nodes_count++;
        node = node->m_Next;
    }
    return nodes_count;
}

TEST(dmDoubleLinkedList, Init)
{
    dmDoubleLinkedList::List list;
    dmDoubleLinkedList::ListInit(&list);

    ASSERT_TRUE(list.m_Head.m_Prev == 0);
    ASSERT_EQ(list.m_Head.m_Next, &list.m_Tail);
    ASSERT_EQ(list.m_Tail.m_Prev, &list.m_Head);
    ASSERT_TRUE(list.m_Tail.m_Next == 0);
}

TEST(dmDoubleLinkedList, Add)
{
    dmDoubleLinkedList::List list;
    dmDoubleLinkedList::ListInit(&list);

    dmDoubleLinkedList::ListNode node_1;
    dmDoubleLinkedList::ListNode node_2;
    dmDoubleLinkedList::ListNode node_3;

    dmDoubleLinkedList::ListAdd(&list, &node_2);
    dmDoubleLinkedList::ListAdd(&list, &node_1);
    dmDoubleLinkedList::ListAdd(&list, &node_3);
    ASSERT_EQ(list.m_Tail.m_Prev, &node_2);
    ASSERT_EQ(list.m_Head.m_Next, &node_3);
    ASSERT_EQ(node_1.m_Prev, &node_3);
    ASSERT_EQ(node_1.m_Next, &node_2);
    ASSERT_TRUE(node_3.m_Prev == &list.m_Head);
    ASSERT_EQ(node_3.m_Next, &node_1);
    ASSERT_EQ(node_2.m_Prev, &node_1);
    ASSERT_TRUE(node_2.m_Next == &list.m_Tail);

    size_t nodes_count = countListNodes(&list);
    ASSERT_EQ(nodes_count, 3);
}

TEST(dmDoubleLinkedList, Remove)
{
    dmDoubleLinkedList::List list;
    dmDoubleLinkedList::ListInit(&list);

    dmDoubleLinkedList::ListNode node_1;
    dmDoubleLinkedList::ListNode node_2;
    dmDoubleLinkedList::ListNode node_3;
    dmDoubleLinkedList::ListNode node_not_in_list;

    dmDoubleLinkedList::ListAdd(&list, &node_2);
    dmDoubleLinkedList::ListAdd(&list, &node_1);
    dmDoubleLinkedList::ListAdd(&list, &node_3);

    dmDoubleLinkedList::ListRemove(&list, &node_not_in_list);
    size_t nodes_count = countListNodes(&list);
    ASSERT_EQ(nodes_count, 3);

    dmDoubleLinkedList::ListRemove(&list, &node_1);
    ASSERT_EQ(list.m_Tail.m_Prev, &node_2);
    ASSERT_EQ(list.m_Head.m_Next, &node_3);
    ASSERT_EQ(node_3.m_Next, &node_2);
    ASSERT_EQ(node_2.m_Prev, &node_3);
    ASSERT_TRUE(node_1.m_Next == 0);
    ASSERT_TRUE(node_1.m_Prev == 0);

    nodes_count = countListNodes(&list);
    ASSERT_EQ(nodes_count, 2);

    dmDoubleLinkedList::ListRemove(&list, &node_2);
    dmDoubleLinkedList::ListRemove(&list, &node_3);
    ASSERT_EQ(&list.m_Head, list.m_Tail.m_Prev);
    nodes_count = countListNodes(&list);
    ASSERT_EQ(nodes_count, 0);
}

TEST(dmDoubleLinkedList, GetLast)
{
    dmDoubleLinkedList::List list;
    dmDoubleLinkedList::ListInit(&list);

    dmDoubleLinkedList::ListNode* tail = dmDoubleLinkedList::ListGetLast(&list);
    ASSERT_TRUE(tail == 0);

    dmDoubleLinkedList::ListNode nodes[2];

    size_t node_size = sizeof(dmDoubleLinkedList::ListNode);
    dmDoubleLinkedList::ListNode* node_1 = &nodes[0];
    dmDoubleLinkedList::ListAdd(&list, node_1);

    dmDoubleLinkedList::ListNode* last_node = dmDoubleLinkedList::ListGetLast(&list);
    ASSERT_EQ(node_1, last_node);

    dmDoubleLinkedList::ListNode* node_2 = &nodes[1];
    dmDoubleLinkedList::ListAdd(&list, node_2);

    last_node = dmDoubleLinkedList::ListGetLast(&list);
    ASSERT_EQ(node_1, last_node);
}

TEST(dmDoubleLinkedList, GetFirst)
{
    dmDoubleLinkedList::List list;
    dmDoubleLinkedList::ListInit(&list);

    dmDoubleLinkedList::ListNode* first = dmDoubleLinkedList::ListGetFirst(&list);
    ASSERT_TRUE(first == 0);

    const uint32_t node_count = 5;
    dmDoubleLinkedList::ListNode nodes[node_count];

    size_t node_size = sizeof(dmDoubleLinkedList::ListNode);
    for (size_t idx = 0; idx < node_count; ++idx)
    {
        dmDoubleLinkedList::ListNode* new_node = &nodes[idx];
        dmDoubleLinkedList::ListAdd(&list, new_node);

        dmDoubleLinkedList::ListNode* first_node = dmDoubleLinkedList::ListGetFirst(&list);
        ASSERT_EQ(new_node, first_node);
    }
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
