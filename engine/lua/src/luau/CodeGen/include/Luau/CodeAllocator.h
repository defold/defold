// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/CodeAllocationData.h"
#include "Luau/CodeGenOptions.h"

#include <vector>

#include <stddef.h>
#include <stdint.h>

namespace Luau
{
namespace CodeGen
{

constexpr uint32_t kCodeAlignment = 32;

struct CodeAllocator
{
    CodeAllocator(size_t blockSize, size_t maxTotalSize);
    CodeAllocator(size_t blockSize, size_t maxTotalSize, AllocationCallback* allocationCallback, void* allocationCallbackContext);
    ~CodeAllocator();

    // Places data and code into the executable page area
    // To allow allocation while previously allocated code is already running, allocation has page granularity
    // It's important to group functions together so that page alignment won't result in a lot of wasted space
    bool allocate_DEPRECATED(
        const uint8_t* data,
        size_t dataSize,
        const uint8_t* code,
        size_t codeSize,
        uint8_t*& result,
        size_t& resultSize,
        uint8_t*& resultCodeStart
    );

    // Places data and code into the executable page area
    // To allow allocation while previously allocated code is already running, allocation has page granularity
    // It's important to group functions together so that page alignment won't result in a lot of wasted space
    CodeAllocationData allocate(const uint8_t* data, size_t dataSize, const uint8_t* code, size_t codeSize);

    // Marks executable page area as no longer executable
    // Freed allocation area can be reused for future allocations
    void deallocate(CodeAllocationData codeAllocationData);

    // Provided to unwind info callbacks
    void* context = nullptr;

    // Called when new block is created to create and setup the unwinding information for all the code in the block
    // 'startOffset' reserves space for data at the beginning of the page
    void* (*createBlockUnwindInfo)(void* context, uint8_t* block, size_t blockSize, size_t& startOffset) = nullptr;

    // Called to destroy unwinding information returned by 'createBlockUnwindInfo'
    void (*destroyBlockUnwindInfo)(void* context, void* unwindData) = nullptr;

    // Rounds 'size' up to the nearest OS page boundary
    static size_t alignToPageSize(size_t size);

private:
    // Unwind information can be placed inside the block with some implementation-specific reservations at the beginning
    // But to simplify block space checks, we limit the max size of all that data
    static const size_t kMaxReservedDataSize = 256;

    bool allocateNewBlock(size_t& unwindInfoSize);

    uint8_t* allocatePages(size_t size) const;
    void freePages(uint8_t* mem, size_t size) const;

    // Current block we use for allocations
    uint8_t* blockPos = nullptr;
    uint8_t* blockEnd = nullptr;

    // All allocated blocks
    std::vector<uint8_t*> blocks;
    std::vector<void*> unwindInfos;

    size_t blockSize = 0;
    size_t maxTotalSize = 0;
    size_t liveAllocations = 0;

    AllocationCallback* allocationCallback = nullptr;
    void* allocationCallbackContext = nullptr;
};

} // namespace CodeGen
} // namespace Luau
