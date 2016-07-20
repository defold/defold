#ifndef DM_GAMESYS_SCRIPT_WINDOW_H
#define DM_GAMESYS_SCRIPT_WINDOW_H

namespace dmGameSystem
{
    struct ScriptLibContext;

    void ScriptWindowRegister(const ScriptLibContext& context);

    void ScriptWindowOnWindowFocus(bool focus);
    void ScriptWindowOnWindowResized(int width, int height);
}

#endif // DM_GAMESYS_SCRIPT_WINDOW_H
