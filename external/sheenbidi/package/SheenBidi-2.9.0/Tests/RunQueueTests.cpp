/*
 * Copyright (C) 2025 Muhammad Tayyab Akram
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

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include <SheenBidi/SBBase.h>

extern "C" {
#include <Source/BidiChain.h>
#include <Source/LevelRun.h>
#include <Source/RunQueue.h>
}

#include "RunQueueTests.h"

using namespace std;
using namespace SheenBidi;

static void initialize(LevelRunRef levelRun, BidiLink link) {
    levelRun->next = levelRun;
    levelRun->firstLink = link;
    levelRun->lastLink = link;
    levelRun->subsequentLink = link;
    levelRun->extrema = 0;
    levelRun->kind = 0;
    levelRun->level = SBLevelInvalid;
}

static bool areEqual(LevelRunRef firstRun, LevelRunRef secondRun) {
    if (firstRun == secondRun) {
        return true;
    }

    return firstRun->next == secondRun->next
        && firstRun->firstLink == secondRun->firstLink
        && firstRun->lastLink == secondRun->lastLink
        && firstRun->subsequentLink == secondRun->subsequentLink
        && firstRun->extrema == secondRun->extrema
        && firstRun->kind == secondRun->kind
        && firstRun->level == secondRun->level;
}

RunQueueTests::RunQueueTests()
{
}

void RunQueueTests::run() {
    testInitialize();
    testBulkInsertion();
}

void RunQueueTests::testInitialize() {
    RunQueue queue;
    RunQueueInitialize(&queue);

    assert(queue._firstList.previous == nullptr);
    assert(queue._firstList.next == nullptr);

    assert(queue._frontList == &queue._firstList);
    assert(queue._rearList == &queue._firstList);
    assert(queue._partialList == nullptr);

    assert(queue._frontTop == 0);
    assert(queue._rearTop == -1);
    assert(queue._partialTop == -1);

    assert(queue.count == 0);
    assert(queue.shouldDequeue == false);

    RunQueueFinalize(&queue);
}

void RunQueueTests::testBulkInsertion() {
    RunQueue queue;
    RunQueueInitialize(&queue);

    array<LevelRun, 32> levelRuns{};
    uint32_t runCount = 0;

    for (uint32_t index = 0; index < levelRuns.size(); index++) {
        LevelRunRef run = &levelRuns[index];
        initialize(run, index);

        RunQueueEnqueue(&queue, run);
        runCount += 1;

        assert(queue.count == runCount);
        assert(areEqual(&queue._rearList->elements[queue._rearTop], run));
    }

    for (auto &run : levelRuns) {
        LevelRunRef front = RunQueueGetFront(&queue);

        assert(queue.count == runCount);
        assert(front == &queue._frontList->elements[queue._frontTop]);
        assert(areEqual(front, &run));

        RunQueueDequeue(&queue);
        runCount -= 1;
    }

    RunQueueFinalize(&queue);
}

#ifdef STANDALONE_TESTING

int main(int argc, const char *argv[]) {
    RunQueueTests runQueueTests;
    runQueueTests.run();

    return 0;
}

#endif
