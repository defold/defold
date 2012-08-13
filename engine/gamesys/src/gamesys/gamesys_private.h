#ifndef DM_GAMESYS_PRIVER_H
#define DM_GAMESYS_PRIVER_H

#include <dlib/message.h>

namespace dmGameSystem
{
    /**
     * Log message error. The function will send a formatted printf-style string to dmLogError
     * and append message sender/receiver information on the following format:
     * Message <MESSAGE-ID> sent from <SENDER> to <RECEIVER>. For format-string should be a complete sentence including
     * a terminating period character but without trailing space.
     * @param message message
     * @param format printf-style format string
     */
    void LogMessageError(dmMessage::Message* message, const char* format, ...);

}

#endif // DM_GAMESYS_PRIVER_H
