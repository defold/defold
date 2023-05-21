// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2023 Mathias Westerdahl

#pragma once

#include <atlaspacker/atlaspacker.h>

#pragma pack(1)

typedef enum
{
    AP_BP_MODE_DEFAULT = 0, // Default is whichever enum is also set as 0
    AP_BP_SKYLINE_BL = 0
} apBinPackMode;

typedef struct
{
    apBinPackMode   mode;
    int             no_rotate;
} apBinPackerOptions;

#pragma options align=reset

void      apBinPackerSetDefaultOptions(apBinPackerOptions* options);
apPacker* apBinPackerCreate(apBinPackerOptions* options);
void      apBinPackerDestroy(apPacker* packer);
