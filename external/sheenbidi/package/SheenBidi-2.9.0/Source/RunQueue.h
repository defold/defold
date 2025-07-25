/*
 * Copyright (C) 2014-2025 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SB_INTERNAL_RUN_QUEUE_H
#define _SB_INTERNAL_RUN_QUEUE_H

#include <SheenBidi/SBBase.h>
#include <SheenBidi/SBConfig.h>

#include "LevelRun.h"
#include "Object.h"

#define RunQueueList_Length         8
#define RunQueueList_MaxIndex       (RunQueueList_Length - 1)

typedef struct _RunQueueList {
    LevelRun elements[RunQueueList_Length];

    struct _RunQueueList *previous; /**< Reference to the previous list of queue elements */
    struct _RunQueueList *next;     /**< Reference to the next list of queue elements */
} RunQueueList, *RunQueueListRef;

typedef struct _RunQueue {
    Object _object;
    RunQueueList _firstList;        /**< First list of elements, which is part of the queue */
    RunQueueListRef _frontList;     /**< The list containing front element of the queue */
    RunQueueListRef _rearList;      /**< The list containing rear element of the queue */
    RunQueueListRef _partialList;   /**< The list containing latest partial isolating run */
    SBInteger _frontTop;            /**< Index of front element in front list */
    SBInteger _rearTop;             /**< Index of rear element in rear list */
    SBInteger _partialTop;          /**< Index of partial run in partial list */
    SBUInteger count;               /**< Number of elements the queue contains */
    SBBoolean shouldDequeue;
} RunQueue, *RunQueueRef;

SB_INTERNAL void RunQueueInitialize(RunQueueRef queue);

SB_INTERNAL SBBoolean RunQueueEnqueue(RunQueueRef queue, const LevelRunRef levelRun);
SB_INTERNAL void RunQueueDequeue(RunQueueRef queue);

SB_INTERNAL LevelRunRef RunQueueGetFront(RunQueueRef queue);

SB_INTERNAL void RunQueueFinalize(RunQueueRef queue);

#endif
