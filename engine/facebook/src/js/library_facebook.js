var LibraryFacebook = {
        $FBinner: {

        },

        dmFacebookInitialize: function(app_id, version) {
            // We assume that the Facebook javascript SDK is loaded by now.
            // This should be done via a script tag (synchronously) in the html page:
            // <script type="text/javascript" src="//connect.facebook.net/en_US/sdk.js"></script>
            // This script tag MUST be located before the engine (game) js script tag.
            try {
                FB.init({
                    appId      : Pointer_stringify(app_id),
                    status     : false,
                    xfbml      : false,
                    version    : Pointer_stringify(version),
                });

                window._dmFacebookUpdateMe = function(callback) {
                    try {
                        FB.api('/me', 'get', { fields: 'id,name,email,first_name,last_name,link,gender,updated_time' }, function (response) {
                            var e = (response && response.error ? response.error.message : 0);
                            if(e == 0) {
                                var me_data = JSON.stringify(response);
                                callback(0, me_data);
                            } else {
                                callback(e, 0);
                            }
                        });
                    } catch (e){
                        console.error("Facebook me failed " + e);
                    }
                };

                window._dmFacebookUpdatePermissions = function(callback) {
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
                                callback(0, permissions_data);
                            } else {
                                callback(e, 0);
                            }
                        });
                    } catch (e){
                        console.error("Facebook permissions failed " + e);
                    }
                };

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
                        var res_data = JSON.stringify(response);
                        var res_buf = allocate(intArrayFromString(res_data), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viii', callback, [lua_state, res_buf, e]);
                    } else {
                        var error = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viii', callback, [lua_state, 0, error]);
                    }
                });
            } catch (e) {
                console.error("Facebook show dialog failed " + e);
            }
        },

        // https://developers.facebook.com/docs/reference/javascript/FB.AppEvents.LogEvent
        dmFacebookPostEvent: function(event, value_to_sum, keys, values) {
            var params = {};
            try {
                event = Pointer_stringify(event);
                keys = JSON.parse(Pointer_stringify(keys));
                values = JSON.parse(Pointer_stringify(values));
                for (var i = 0; i < keys.length; ++i) {
                    params[keys[i]] = values[i];
                }
            } catch (e) {
                console.error("Unable to parse data from Defold: " + e);
            }

            try {
                FB.AppEvents.logEvent(event, value_to_sum, params);
            } catch (e) {
                console.error("Unable to post event to Facebook Analytics: " + e);
            }
        },

        // https://developers.facebook.com/docs/javascript/reference/v2.0
        dmFacebookEnableEventUsage: function() {
            console.error("Limiting Facebook Analytics is not supported for Canvas");
        },

        // https://developers.facebook.com/docs/javascript/reference/v2.0
        dmFacebookDisableEventUsage: function() {
            console.error("Limiting Facebook Analytics is not supported for Canvas");
        },

        // https://developers.facebook.com/docs/reference/javascript/FB.login/v2.0
        dmFacebookDoLogin: function(state_open, state_closed, state_failed, callback, lua_state) {

            var chainedUpdateMeAndPermissions = function() {
                // request user and permissions data, need to store this on the C side
                window._dmFacebookUpdateMe(function(e, me_data) {
                    if (e == 0) {
                        window._dmFacebookUpdatePermissions(function(e, permissions_data) {
                            if (e == 0) {
                                var me_buf = allocate(intArrayFromString(me_data), 'i8', ALLOC_STACK);
                                var permissions_buf = allocate(intArrayFromString(permissions_data), 'i8', ALLOC_STACK);
                                Runtime.dynCall('viiiii', callback, [lua_state, state_open, 0, me_buf, permissions_buf]);
                            } else {
                                var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                                Runtime.dynCall('viiiii', callback, [lua_state, state_failed, err_buf, 0, 0]);
                            }
                        });
                    } else {
                        var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viiiii', callback, [lua_state, state_failed, err_buf, 0, 0]);
                    }
                });
            }

            try {

                FB.getLoginStatus(function(response) {

                    // The user might login with JS outside Defold,
                    // then we only need to update the me-table and
                    // permissions list.
                    if (response.status === 'connected') {
                        chainedUpdateMeAndPermissions();
                    } else {

                        FB.login(function(response) {
                            var e = (response && response.error ? response.error.message : 0);
                            if (e == 0 && response.authResponse) {

                                chainedUpdateMeAndPermissions();

                            } else if (e != 0) {
                                var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                                Runtime.dynCall('viiiii', callback, [lua_state, state_closed, err_buf, 0, 0]);
                            } else {
                                // No authResponse. Below text is from facebook's own example of this case.
                                e = 'User cancelled login or did not fully authorize.';
                                var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                                Runtime.dynCall('viiiii', callback, [lua_state, state_failed, err_buf, 0, 0]);
                            }
                        }, {scope: 'public_profile,user_friends'});

                    }
                });
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

                        // update internal permission state
                        window._dmFacebookUpdatePermissions(function(e, permissions_data) {
                            if (e == 0) {
                                var permissions_buf = allocate(intArrayFromString(permissions_data), 'i8', ALLOC_STACK);
                                Runtime.dynCall('viii', callback, [lua_state, 0, permissions_buf]);
                            } else {
                                var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                                Runtime.dynCall('viii', callback, [lua_state, err_buf, 0]);
                            }
                        });

                    } else if (e != 0) {
                        var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viii', callback, [lua_state, err_buf, 0]);
                    } else {
                        // No authResponse. Below text is from facebook's own example of this case.
                        e = 'User cancelled login or did not fully authorize.';
                        var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viii', callback, [lua_state, err_buf, 0]);
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
                        // update internal permission state
                        window._dmFacebookUpdatePermissions(function(e, permissions_data) {
                            if (e == 0) {
                                var permissions_buf = allocate(intArrayFromString(permissions_data), 'i8', ALLOC_STACK);
                                Runtime.dynCall('viii', callback, [lua_state, 0, permissions_buf]);
                            } else {
                                var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                                Runtime.dynCall('viii', callback, [lua_state, err_buf, 0]);
                            }
                        });

                    } else if (e != 0) {
                        var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viii', callback, [lua_state, err_buf, 0]);
                    } else {
                        // No authResponse. Below text is from facebook's own example of this case.
                        e = 'User cancelled login or did not fully authorize.';
                        var err_buf = allocate(intArrayFromString(e), 'i8', ALLOC_STACK);
                        Runtime.dynCall('viii', callback, [lua_state, err_buf, 0]);
                    }
                }, {scope: Pointer_stringify(permissions)});
            } catch (e){
                console.error("Facebook request publish permissions failed " + e);
            }
        }
}

autoAddDeps(LibraryFacebook, '$FBinner');
mergeInto(LibraryManager.library, LibraryFacebook);
