namespace dLib
{
    bool g_DebugMode = true;
    void SetDebugMode(bool debug_mode)
    {
        g_DebugMode = debug_mode;
    }

    bool IsDebugMode()
    {
        return g_DebugMode;
    }
}
