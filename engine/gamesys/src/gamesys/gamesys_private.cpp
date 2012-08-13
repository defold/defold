#include <stdarg.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include "gamesys_private.h"

namespace dmGameSystem
{
    void LogMessageError(dmMessage::Message* message, const char* format, ...)
    {
        va_list lst;
        va_start(lst, format);

        char buf[512];

        int n = vsnprintf(buf, sizeof(buf), format, lst);

        if (n < (int) sizeof(buf))
        {
            const char* id_str = (const char*) dmHashReverse64(message->m_Id, 0);

            const dmMessage::URL* sender = &message->m_Sender;
            const char* socket_name_sender = dmMessage::GetSocketName(sender->m_Socket);
            const char* path_name_sender = (const char*) dmHashReverse64(sender->m_Path, 0);
            const char* fragment_name_sender = (const char*) dmHashReverse64(sender->m_Fragment, 0);

            const dmMessage::URL* receiver = &message->m_Receiver;
            const char* socket_name_receiver = dmMessage::GetSocketName(receiver->m_Socket);
            const char* path_name_receiver = (const char*) dmHashReverse64(receiver->m_Path, 0);
            const char* fragment_name_receiver = (const char*) dmHashReverse64(receiver->m_Fragment, 0);

            n+= DM_SNPRINTF(buf + n, sizeof(buf) - n, " Message '%s' sent from %s:%s#%s to %s:%s#%s.",
                            id_str,
                            socket_name_sender, path_name_sender, fragment_name_sender,
                            socket_name_receiver, path_name_receiver, fragment_name_receiver);
        }

        if (n >= (int) sizeof(buf) - 1) {
            dmLogError("Buffer underflow when formatting message-error (LogMessageError)");
        }

        dmLogError("%s", buf);

        va_end(lst);

    }
}
