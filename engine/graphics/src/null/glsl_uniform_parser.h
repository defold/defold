#ifndef DMGRAPHICS_GLSL_UNIFORM_PARSER_H
#define DMGRAPHICS_GLSL_UNIFORM_PARSER_H

#include <stdint.h>
#include "../graphics.h"

namespace dmGraphics
{
    typedef void (*UniformCallback)(const char* name, uint32_t name_length, Type type, uintptr_t userdata);

    bool GLSLUniformParse(const char* buffer, UniformCallback cb, uintptr_t userdata);
}

#endif // DMGRAPHICS_GLSL_UNIFORM_PARSER_H
