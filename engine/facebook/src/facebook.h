#ifndef DM_FACEBOOK_H
#define DM_FACEBOOK_H

namespace dmFacebook {

    enum State {
        STATE_CREATED              = 1,
        STATE_CREATED_TOKEN_LOADED = 2,
        STATE_CREATED_OPENING      = 3,
        STATE_OPEN                 = 4,
        STATE_OPEN_TOKEN_EXTENDED  = 5,
        STATE_CLOSED_LOGIN_FAILED  = 6,
        STATE_CLOSED               = 7,
    };

    enum GameRequestAction {
        GAMEREQUEST_ACTIONTYPE_NONE   = 1,
        GAMEREQUEST_ACTIONTYPE_SEND   = 2,
        GAMEREQUEST_ACTIONTYPE_ASKFOR = 3,
        GAMEREQUEST_ACTIONTYPE_TURN   = 4,
    };

    enum GameRequestFilters {
        GAMEREQUEST_FILTER_NONE        = 1,
        GAMEREQUEST_FILTER_APPUSERS    = 2,
        GAMEREQUEST_FILTER_APPNONUSERS = 3,
    };

    enum Audience {
        AUDIENCE_NONE     = 1,
        AUDIENCE_ONLYME   = 2,
        AUDIENCE_FRIENDS  = 3,
        AUDIENCE_EVERYONE = 4,
    };


/*
    Notes on facebook.show_dialog in regards to FB SDK 4.

    Dialog parameters have been updated to align with field names in the
    current SDK version. Names have been chosen with a preference of iOS
    field names (imageURL vs imageUrl for example).

    Some fields that have changed names in the SDK still exist, but are
    considered deprecated (to -> recipients).

    facebook.show_dialog( dialog_type, param_table, callback_func ):

    dialog_type == "apprequests" or "gamerequest":
                           arg     type                    iOS / Android
        -                title : string [                title / setTitle       ]
        -              message : string [              message / setMessage     ]
        -           actionType :    int [           actionType / setActionType  ]
        -              filters :    int [              filters / setFilters     ]
        -                 data : string [                 data / setData        ]
        -             objectID : string [             objectID / setObjectId    ]
        - recipientSuggestions :  array [ recipientSuggestions / setSuggestions ]
        -           recipients :  array [           recipients / setTo          ]

        Deprecated fields:
        -          suggestions : string, use recipientSuggestions instead
        -                   to : string, use recipients instead

    dialog_type == "feed" or "link":
                           arg     type                  iOS / Android
        -         contentTitle : string [       contentTitle / setContentTitle       ]
        -   contentDescription : string [ contentDescription / setContentDescription ]
        -             imageURL : string [           imageURL / setImageUrl           ]
        -           contentURL : string [         contentURL / setContentUrl         ]
        -            peopleIDs :  array [          peopleIDs / setPeopleIds          ]
        -              placeID : string [            placeID / setPlaceId            ]
        -                  ref : string [                ref / setRef                ]

        Deprecated fields:
        -                title : string, use contentTitle instead
        -          description : string, use contentDescription instead
        -                 link : string, use contentURL instead

    dialog_type == "appinvite":
                               arg     type                        iOS / Android
        -               appLinkURL : string [               appLinkURL / setApplinkUrl      ]
        - appInvitePreviewImageURL : string [ appInvitePreviewImageURL / setPreviewImageUrl ]

*/

}

#endif // #ifndef DM_FACEBOOK_H
