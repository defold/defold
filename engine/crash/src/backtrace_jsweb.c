#include "crash.h"
#include "crash_private.h"
#include <dlib/log.h>
#include <dlib/json.h>

namespace dmCrash
{
    void WriteExtra(int fd)
    {
        // WriteExtra is void for js-web, JSWriteExtra is used instead.
    }

    void WriteDump()
    {
        // WriteDump is void for js-web, JSWriteDump is used instead.
    }

    void InstallHandler()
    {
        // Crash handler is installed directly via JavaScript.
        // window.onerror = crash_handler;
    }
}

extern "C" void JSWriteDump(char* message, char* json_stacktrace) {
    dmLogInfo("JSWriteDump");
    dmLogInfo("Message    : %s", message);
    dmLogInfo("Stacktrace : %s", json_stacktrace);
    dmLogInfo("Message    # %d", strlen(message));
    dmLogInfo("Stacktrace # %d", strlen(json_stacktrace));

    dmJson::Document doc = { 0 };
    dmJson::Result result = dmJson::Parse(json_stacktrace, &doc);
    if (result == dmJson::RESULT_OK)
    {
        if (doc.m_NodeCount > 0 && doc.m_Nodes[0].m_Type == dmJson::TYPE_ARRAY)
        {
            for (uint32_t i = 1; i < doc.m_NodeCount; ++i)
            {
                dmJson::Node& curr = doc.m_Nodes[i];
                if (curr.m_Type == dmJson::TYPE_STRING)
                {
                    dmLogInfo("Found string between %d and %d", curr.m_Start, curr.m_End);
                }
                else
                {
                    dmLogError("Unexpected stracktrace entry #%d format (%d)", i, curr.m_Type);
                }
            }
        }
        else if (doc.m_NodeCount > 0 && doc.m_Nodes[0].m_Type == dmJson::TYPE_STRING)
        {
            dmLogInfo("stacktrace is a string!");
        }
        else
        {
            dmLogError("Unexpected stacktrace format (%d)", (int) doc.m_Nodes[0].m_Type);
        }

        dmJson::Free(&doc);
    }
    else
    {
        dmLogError("Unable to parse stacktrace!");
    }

    dmCrash::g_AppState.m_Signum = 0xDEAD;
    dmCrash::g_AppState.m_PtrCount = 0;
}

extern "C" void JSWriteExtra(int fd) {
    dmLogInfo("JSWriteExtra");

}