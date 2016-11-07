#ifndef DM_GAMESYS_SCRIPT_LABEL_H
#define DM_GAMESYS_SCRIPT_LABEL_H

namespace dmGameSystem
{
    struct ScriptLibContext;

    void ScriptLabelRegister(const ScriptLibContext& context);
    void ScriptLabelFinalize(const ScriptLibContext& context);
}

#endif // DM_GAMESYS_SCRIPT_LABEL_H
