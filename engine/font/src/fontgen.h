#pragma once

#include <dmsdk/dlib/hash.h>
#include <dmsdk/extension/extension.h>

namespace dmFontGen
{
    bool Initialize(dmExtension::Params* params);
    void Finalize(dmExtension::Params* params);
    void Update(dmExtension::Params* params);

    // Scripting

    bool LoadFont(const char* fontc_path, const char* ttf_path);
    bool UnloadFont(dmhash_t fontc_path_hash);

    typedef void (*FGlyphCallback)(void* cbk_ctx, int result, const char* errmsg);

    bool AddGlyphs(dmhash_t fontc_path_hash, const char* text, FGlyphCallback cbk, void* cbk_ctx);
    bool RemoveGlyphs(dmhash_t fontc_path_hash, const char* text);
}
