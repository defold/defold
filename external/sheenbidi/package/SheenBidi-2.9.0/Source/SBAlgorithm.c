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

#include <stddef.h>

#include <SheenBidi/SBConfig.h>

#include "BidiTypeLookup.h"
#include "Object.h"
#include "SBBase.h"
#include "SBCodepoint.h"
#include "SBCodepointSequence.h"
#include "SBLog.h"
#include "SBParagraph.h"
#include "SBAlgorithm.h"

#define ALGORITHM  0
#define BIDI_TYPES 1
#define COUNT      2

static SBAlgorithmRef AllocateAlgorithm(SBUInteger stringLength)
{
    void *pointers[COUNT] = { NULL };
    SBUInteger sizes[COUNT];

    sizes[ALGORITHM]  = sizeof(SBAlgorithm);
    sizes[BIDI_TYPES] = sizeof(SBBidiType) * stringLength;

    if (ObjectCreate(sizes, COUNT, pointers)) {
        SBAlgorithmRef algorithm = pointers[ALGORITHM];
        SBBidiType *fixedTypes = pointers[BIDI_TYPES];

        algorithm->fixedTypes = fixedTypes;
    }

    return pointers[ALGORITHM];
}

#undef ALGORITHM
#undef BIDI_TYPES
#undef COUNT

static void DetermineBidiTypes(const SBCodepointSequence *sequence, SBBidiType *types)
{
    SBUInteger stringIndex = 0;
    SBUInteger firstIndex = 0;
    SBCodepoint codepoint;

    while ((codepoint = SBCodepointSequenceGetCodepointAt(sequence, &stringIndex)) != SBCodepointInvalid) {
        types[firstIndex] = LookupBidiType(codepoint);

        /* Subsequent code units get 'BN' type. */
        while (++firstIndex < stringIndex) {
            types[firstIndex] = SBBidiTypeBN;
        }
    }
}

static SBAlgorithmRef CreateAlgorithm(const SBCodepointSequence *codepointSequence)
{
    SBUInteger stringLength = codepointSequence->stringLength;
    SBAlgorithmRef algorithm;

    SB_LOG_BLOCK_OPENER("Algorithm Input");
    SB_LOG_STATEMENT("Codepoints", 1, SB_LOG_CODEPOINT_SEQUENCE(codepointSequence));
    SB_LOG_BLOCK_CLOSER();

    algorithm = AllocateAlgorithm(stringLength);

    if (algorithm) {
        algorithm->codepointSequence = *codepointSequence;
        algorithm->retainCount = 1;

        DetermineBidiTypes(codepointSequence, algorithm->fixedTypes);

        SB_LOG_BLOCK_OPENER("Determined Types");
        SB_LOG_STATEMENT("Types",  1, SB_LOG_BIDI_TYPES_ARRAY(algorithm->fixedTypes, stringLength));
        SB_LOG_BLOCK_CLOSER();

        SB_LOG_BREAKER();
    }

    return algorithm;
}

SBAlgorithmRef SBAlgorithmCreate(const SBCodepointSequence *codepointSequence)
{
    if (SBCodepointSequenceIsValid(codepointSequence)) {
        return CreateAlgorithm(codepointSequence);
    }

    return NULL;
}

const SBBidiType *SBAlgorithmGetBidiTypesPtr(SBAlgorithmRef algorithm)
{
    return algorithm->fixedTypes;
}

SB_INTERNAL SBUInteger SBAlgorithmGetSeparatorLength(SBAlgorithmRef algorithm, SBUInteger separatorIndex)
{
    const SBCodepointSequence *codepointSequence = &algorithm->codepointSequence;
    SBUInteger stringIndex = separatorIndex;
    SBCodepoint codepoint;
    SBUInteger separatorLength;

    codepoint = SBCodepointSequenceGetCodepointAt(codepointSequence, &stringIndex);
    separatorLength = stringIndex - separatorIndex;

    if (codepoint == '\r') {
        /* Don't break in between 'CR' and 'LF'. */
        if (stringIndex < codepointSequence->stringLength) {
            codepoint = SBCodepointSequenceGetCodepointAt(codepointSequence, &stringIndex);

            if (codepoint == '\n') {
                separatorLength = stringIndex - separatorIndex;
            }
        }
    }

    return separatorLength;
}

void SBAlgorithmGetParagraphBoundary(SBAlgorithmRef algorithm,
    SBUInteger paragraphOffset, SBUInteger suggestedLength,
    SBUInteger *acutalLength, SBUInteger *separatorLength)
{
    const SBCodepointSequence *codepointSequence = &algorithm->codepointSequence;
    SBBidiType *bidiTypes = algorithm->fixedTypes;
    SBUInteger limitIndex;
    SBUInteger startIndex;

    if (separatorLength) {
        *separatorLength = 0;
    }

    SBUIntegerNormalizeRange(codepointSequence->stringLength, &paragraphOffset, &suggestedLength);
    limitIndex = paragraphOffset + suggestedLength;

    for (startIndex = paragraphOffset; startIndex < limitIndex; startIndex++) {
        SBBidiType currentType = bidiTypes[startIndex];

        if (currentType == SBBidiTypeB) {
            SBUInteger codeUnitCount = SBAlgorithmGetSeparatorLength(algorithm, startIndex);

            startIndex += codeUnitCount;

            if (separatorLength) {
                *separatorLength = codeUnitCount;
            }
            break;
        }
    }

    if (acutalLength) {
        *acutalLength = startIndex - paragraphOffset;
    }
}

SBParagraphRef SBAlgorithmCreateParagraph(SBAlgorithmRef algorithm,
    SBUInteger paragraphOffset, SBUInteger suggestedLength, SBLevel baseLevel)
{
    const SBCodepointSequence *codepointSequence = &algorithm->codepointSequence;
    SBUInteger stringLength = codepointSequence->stringLength;

    SBUIntegerNormalizeRange(stringLength, &paragraphOffset, &suggestedLength);

    if (suggestedLength > 0) {
        return SBParagraphCreate(algorithm, paragraphOffset, suggestedLength, baseLevel);
    }

    return NULL;
}

SBAlgorithmRef SBAlgorithmRetain(SBAlgorithmRef algorithm)
{
    if (algorithm) {
        algorithm->retainCount += 1;
    }

    return algorithm;
}

void SBAlgorithmRelease(SBAlgorithmRef algorithm)
{
    if (algorithm && --algorithm->retainCount == 0) {
        ObjectDispose(&algorithm->_object);
    }
}
