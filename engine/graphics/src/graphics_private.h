#ifndef DM_GRAPHICS_PRIVATE_H
#define DM_GRAPHICS_PRIVATE_H

namespace dmGraphics
{
    uint64_t GetDrawCount();
    void SetForceFragmentReloadFail(bool should_fail);
    void SetForceVertexReloadFail(bool should_fail);
}

#endif // #ifndef DM_GRAPHICS_PRIVATE_H
