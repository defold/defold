#ifndef DM_GAMESYS_SCRIPT_WINDOW_H
#define DM_GAMESYS_SCRIPT_WINDOW_H

namespace dmGameSystem
{
    struct ScriptLibContext;

    enum SleepMode
    {
        SLEEP_MODE_NORMAL = 0,
        SLEEP_MODE_AWAKE  = 1
    };

    void ScriptWindowRegister(const ScriptLibContext& context);

    void ScriptWindowOnWindowFocus(bool focus);
    void ScriptWindowOnWindowResized(int width, int height);

    void SetSleepMode(SleepMode normal_sleep);
    SleepMode GetSleepMode();
}

#endif // DM_GAMESYS_SCRIPT_WINDOW_H
