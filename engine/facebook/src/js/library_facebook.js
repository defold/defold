var LibraryFacebook = {
        $FBinner: {

        },

        dmFacebookInitialize: function(app_id) {
            // We assume that the Facebook javascript SDK is loaded by now.
            // This should be done via a script tag (synchronously) in the html page:
            // <script type="text/javascript" src="//connect/facebook.net/en_US/sdk.js"></script>
            // This script tag MUST be located before the engine (game) js script tag.
            try {
                FB.init({
                    appId      : Pointer_stringify(app_id),
                    status     : false,
                    xfbml      : false,
                    version    : 'v2.0',
                });
            } catch (e){
                console.error("Facebook initialize failed " + e);
            }
        },

        // https://developers.facebook.com/docs/reference/javascript/FB.getAuthResponse/
        dmFacebookAccessToken: function(callback, lua_state) {
            try {
                var response = FB.getAuthResponse(); // Cached??
                var access_token = (response && response.accessToken ? response.accessToken : 0);

                if(access_token != 0) {
                    var buf = allocate(intArrayFromString(access_token), 'i8', ALLOC_STACK);
                    Runtime.dynCall('vii', callback, [lua_state, buf]);
                } else {
                    Runtime.dynCall('vii', callback, [lua_state, 0]);
                }
            } catch (e){
                console.error("Facebook access token failed " + e);
            }
        },

        // https://developers.facebook.com/docs/graph-api/reference/v2.0/user/permissions
        dmFacebookPermissions: function(callback, lua_state) {
            try {
                FB.api('/me/permissions', function (response) {
                    var e = (response && response.error ? response.error.message : 0);
                    if(e == 0 && response.data) {
                        var permissions = [];
                        for (var i=0; i<response.data.length; i++) {
                            if(response.data[i].permission && response.data[i].status) {
                                if(response.data[i].status === 'granted') {
                                    permissions.push(response.data[i].permission);
                                } else if(response.data[i].status === 'declined') {
                                    // TODO: Handle declined permissions?
                                }
                            }
                        }
                        // Just make json of the acutal permissions (array)
                        var permissions_data = JSON.stringify(permissions);
                        var buf = allocate(intArrayFromString(permissions_data), 'i8', ALLOC_STACK);
                        Runtime.dynCall('vii', callback, [lua_state, buf]);
                    } else {
                        Runtime.dynCall('vii', callback, [lua_state, 0]);
                    }
                });
            } catch (e){
                console.error("Facebook permissions failed " + e);
            }
        },

        // https://developers.facebook.com/docs/graph-api/reference/v2.0/user/
        dmFacebookMe: function(callback, lua_state){
            try {
                FB.api('/me', function (response) {
                    var e = (response && response.error ? response.error.message : 0);
                    if(e == 0) {
                        var me_data = JSON.stringify(response);
                        var buf = allocate(intArrayFromString(me_data), 'i8', ALLOC_STACK);
                        Runtime.dynCall('vii', callback, [lua_state, buf]);
                    } else {
                        // This follows the iOS implementation...
                        Runtime.dynCall('vii', callback, [lua_state, 0]);
                    }
                });
            } catch (e){
                console.error("Facebook me failed " + e);
            }
        },

        // https://developers.facebook.com/docs/javascript/reference/FB.ui
        dmFacebookShowDialog: function(params, mth, callback, lua_state) {
            var par = JSON.parse(Pointer_stringify(params));
            par.method = Pointer_stringify(mth);

            try {
                FB.ui(par, function(response) {
                    // https://developers.facebook.com/docs/graph-api/using-graph-api/v2.0
                    //   (Section 'Handling Errors')
                    var e = (response && response.error ? response.error.message : 0);
                    if(e == 0) {
                        // TODO: UTF8?
                        // Matches iOS
                        var result = 'fbconnect://success?';
                        for (var key in response) {
                            if(response.hasOwnProperty(key)) {
                                result += key + '=' + encodeURIComponent(response[key]) + '&';
                            }
                        }
                        if(result[result.length-1] === '&') {
                            result.slice(0, -1);
                        }
                        var url = allocate(intArrayFromString(result), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viii', callback, [lua_state, url, e]);
                    } else {
                        var error = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        var url = 0;
                        Runtime.dynCall('viii', callback, [lua_state, url, error]);
                    }
                });
            } catch (e){
                console.error("Facebook show dialog failed " + e);
            }
        },

        // https://developers.facebook.com/docs/reference/javascript/FB.login/v2.0
        dmFacebookDoLogin: function(state_open, state_closed, state_failed, callback, lua_state) {
            try {
                FB.login(function(response) {
                    var e = (response && response.error ? response.error.message : 0);
                    if (e == 0 && response.authResponse) {
                        Runtime.dynCall('viii', callback, [lua_state, state_open, 0]);
                    } else if (e != 0) {
                        var buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viii', callback, [lua_state, state_closed, buf]);
                    } else {
                        // No authResponse. Below text is from facebook's own example of this case.
                        e = 'User cancelled login or did not fully authorize.';
                        var buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viii', callback, [lua_state, state_failed, buf]);
                    }
                }, {scope: 'public_profile,user_friends'});
            } catch (e) {
                console.error("Facebook login failed " + e);
            }
        },

        dmFacebookDoLogout: function() {
            try {
                FB.logout(function(response) {
                    // user is now logged out
                });
            } catch (e){
                console.error("Facebook logout failed " + e);
            }
        },

        // https://developers.facebook.com/docs/reference/javascript/FB.login/v2.0
        // https://developers.facebook.com/docs/facebook-login/permissions/v2.0
        dmFacebookRequestReadPermissions: function(permissions, callback, lua_state) {
            try {
                FB.login(function(response) {
                    var e = (response && response.error ? response.error.message : 0);
                    if (e == 0 && response.authResponse) {
                        Runtime.dynCall('vii', callback, [lua_state, 0]);
                    } else if (e != 0) {
                        var buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('vii', callback, [lua_state, buf]);
                    } else {
                        // No authResponse. Below text is from facebook's own example of this case.
                        e = 'User cancelled login or did not fully authorize.';
                        var buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('vii', callback, [lua_state, buf]);
                    }
                }, {scope: Pointer_stringify(permissions)});
            } catch (e){
                console.error("Facebook request read permissions failed " + e);
            }

        },

        // https://developers.facebook.com/docs/reference/javascript/FB.login/v2.0
        // https://developers.facebook.com/docs/facebook-login/permissions/v2.0
        dmFacebookRequestPublishPermissions: function(permissions, audience, callback, lua_state) {
            try {
                FB.login(function(response) {
                    var e = (response && response.error ? response.error.message : 0);
                    if (e == 0 && response.authResponse) {
                        Runtime.dynCall('vii', callback, [lua_state, 0]);
                    } else if (e != 0) {
                        var buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('vii', callback, [lua_state, buf]);
                    } else {
                        // No authResponse. Below text is from facebook's own example of this case.
                        e = 'User cancelled login or did not fully authorize.';
                        var buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('vii', callback, [lua_state, buf]);
                    }
                }, {scope: Pointer_stringify(permissions)});
            } catch (e){
                console.error("Facebook request publish permissions failed " + e);
            }
        }
}

autoAddDeps(LibraryFacebook, '$FBinner');
mergeInto(LibraryManager.library, LibraryFacebook);