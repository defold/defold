#ifndef DDF_SAVE_H
#define DDF_SAVE_H

#include "ddf.h"

DDFError DDFDoSaveMessage(const void* message, const SDDFDescriptor* desc, void* context, DDFSaveFunction save_function);

#endif // DDF_SAVE_H
