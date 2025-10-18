/*
 * Copyright (C) 2016-2025 Muhammad Tayyab Akram
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

#ifndef _SB_INTERNAL_ALGORITHM_H
#define _SB_INTERNAL_ALGORITHM_H

#include <SheenBidi/SBAlgorithm.h>
#include <SheenBidi/SBBase.h>
#include <SheenBidi/SBBidiType.h>
#include <SheenBidi/SBCodepointSequence.h>
#include <SheenBidi/SBConfig.h>

#include "Object.h"

typedef struct _SBAlgorithm {
    Object _object;
    SBCodepointSequence codepointSequence;
    SBBidiType *fixedTypes;
    SBUInteger retainCount;
} SBAlgorithm;

SB_INTERNAL SBUInteger SBAlgorithmGetSeparatorLength(SBAlgorithmRef algorithm, SBUInteger separatorIndex);

#endif
