#ifndef DM_FACEBOOK_H
#define DM_FACEBOOK_H

namespace dmFacebook {

    enum State {
        StateCreated                   = 0,
        StateCreatedTokenLoaded        = 1,
        StateCreatedOpening            = 2,
        StateOpen                      = 3,
        StateOpenTokenExtended         = 4,
        ClosedLoginFailed              = 5,
        Closed                         = 6,
    };

}



#endif // #ifndef DM_FACEBOOK_H
