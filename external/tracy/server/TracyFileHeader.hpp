#ifndef __TRACYFILEHEADER_HPP__
#define __TRACYFILEHEADER_HPP__

#include <stdint.h>

#include "../public/common/TracyForceInline.hpp"

namespace tracy
{

static const uint8_t TracyHeader[4] = { 't', 'r', 253, 'P' };
static const uint8_t Lz4Header[4]  = { 't', 'l', 'Z', 4 };
static const uint8_t ZstdHeader[4] = { 't', 'Z', 's', 't' };

static constexpr tracy_force_inline int FileVersion( uint8_t h5, uint8_t h6, uint8_t h7 )
{
    return ( h5 << 16 ) | ( h6 << 8 ) | h7;
}

}

#endif
