#ifndef DDF_SAVE_H
#define DDF_SAVE_H

#include "ddf.h"

namespace dmDDF
{
    Result DoSaveMessage(const void* message, const Descriptor* desc, void* context, SaveFunction save_function);
}

#endif // DDF_SAVE_H
