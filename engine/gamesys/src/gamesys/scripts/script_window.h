#ifndef DM_GAMESYS_SCRIPT_WINDOW_H
#define DM_GAMESYS_SCRIPT_WINDOW_H

namespace dmGameSystem
{
    struct ScriptLibContext;

    enum DimMode
    {
        DIMMING_ON = 0,
        DIMMING_OFF  = 1
    };

    void ScriptWindowRegister(const ScriptLibContext& context);

    void ScriptWindowOnWindowFocus(bool focus);
    void ScriptWindowOnWindowResized(int width, int height);

    void SetDimMode(DimMode mode);
    DimMode GetDimMode();
}

#endif // DM_GAMESYS_SCRIPT_WINDOW_H
