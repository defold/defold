#ifndef DM_GAMESYS_SCRIPT_WINDOW_H
#define DM_GAMESYS_SCRIPT_WINDOW_H

namespace dmGameSystem
{
    struct ScriptLibContext;

    enum DimMode
    {
        DIMMING_UNKNOWN = 0,
        DIMMING_ON      = 1,
        DIMMING_OFF     = 2
    };

    void ScriptWindowRegister(const ScriptLibContext& context);

    void ScriptWindowOnWindowFocus(bool focus);
    void ScriptWindowOnWindowResized(int width, int height);

    void SetDimMode(DimMode mode);
    DimMode GetDimMode();
}

#endif // DM_GAMESYS_SCRIPT_WINDOW_H
