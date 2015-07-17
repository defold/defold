package com.dynamo.android.facebook;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;
import android.net.Uri;

import android.support.v4.app.FragmentActivity;
import com.dynamo.android.DispatcherActivity.PseudoActivity;
import android.content.Context;

import com.facebook.FacebookSdk;
import com.facebook.AccessToken;
import com.facebook.CallbackManager;
import com.facebook.FacebookCallback;
import com.facebook.FacebookException;
import com.facebook.login.LoginManager;
import com.facebook.login.LoginResult;
import com.facebook.login.DefaultAudience;
import com.facebook.share.Sharer;
import com.facebook.share.model.ShareLinkContent;
import com.facebook.share.model.AppInviteContent;
import com.facebook.share.model.GameRequestContent;
import com.facebook.share.widget.ShareDialog;
import com.facebook.share.widget.AppInviteDialog;
import com.facebook.share.widget.GameRequestDialog;
import com.facebook.GraphRequest;
import com.facebook.GraphResponse;
import com.facebook.HttpMethod;

import org.json.JSONObject;

public class FacebookActivity implements PseudoActivity {
    private static final String TAG = "defold.facebook";

    private Messenger messenger;
    private Activity parent;
    private CallbackManager callbackManager;

    /*
     * Functions to convert Lua enums to corresponding Facebook enums.
     * Switch values are from enums defined in facebook_android.cpp.
     */
    private DefaultAudience convertDefaultAudience(int fromLuaInt) {
        switch (fromLuaInt) {
            case 0:
                return DefaultAudience.ONLY_ME;
            case 1:
                return DefaultAudience.FRIENDS;
            case 2:
                return DefaultAudience.EVERYONE;
            case -1:
            default:
                return DefaultAudience.NONE;
        }
    }

    private GameRequestContent.ActionType convertGameRequestAction(int fromLuaInt) {
        switch (fromLuaInt) {
            case 1:
                return GameRequestContent.ActionType.ASKFOR;
            case 2:
                return GameRequestContent.ActionType.TURN;
            case 0:
            default:
                return GameRequestContent.ActionType.SEND;
        }
    }

    private GameRequestContent.Filters convertGameRequestFilters(int fromLuaInt) {
        switch (fromLuaInt) {
            case 1:
                return GameRequestContent.Filters.APP_NON_USERS;
            case 0:
            default:
                return GameRequestContent.Filters.APP_USERS;
        }
    }

    private void respond(final String action, final Bundle data) {

        this.parent.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                data.putString(Facebook.MSG_KEY_ACTION, action);
                Message message = new Message();
                message.setData(data);
                try {
                    messenger.send(message);
                } catch (RemoteException e) {
                    Log.wtf(TAG, e);
                }
                parent.finish();
            }
        });
    }

    // Do a graph request to get latest user data (i.e. "me" table)
    private void UpdateUserData( final String action )
    {
        GraphRequest request = GraphRequest.newMeRequest(
            AccessToken.getCurrentAccessToken(),
            new GraphRequest.GraphJSONObjectCallback() {
                @Override
                public void onCompleted(
                       JSONObject object,
                       GraphResponse response) {

                   Bundle data = new Bundle();
                   data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                   data.putString(Facebook.MSG_KEY_USER, object.toString());
                   data.putString(Facebook.MSG_KEY_ACCESS_TOKEN, AccessToken.getCurrentAccessToken().getToken());
                   data.putInt(Facebook.MSG_KEY_STATE, 1);

                   respond(action, data);
                }
            });
        Bundle parameters = new Bundle();
        parameters.putString("fields", "last_name,link,id,gender,email,locale,name,first_name,updated_time");
        request.setParameters(parameters);
        request.executeAsync();
    }

    @Override
    public void onCreate(Bundle savedInstanceState, Activity parent) {
        this.parent = parent;
        Intent intent = parent.getIntent();
        Bundle extras = intent.getExtras();

        callbackManager = CallbackManager.Factory.create();

        final String action = intent.getAction();
        this.messenger = (Messenger) extras.getParcelable(Facebook.INTENT_EXTRA_MESSENGER);

        if (action.equals(Facebook.ACTION_LOGIN)) {

            if (AccessToken.getCurrentAccessToken() != null) {
                UpdateUserData(action);
            } else {
                LoginManager.getInstance().registerCallback(callbackManager, new FacebookCallback<LoginResult>() {

                    @Override
                    public void onSuccess(LoginResult result) {
                        UpdateUserData(action);
                    }

                    @Override
                    public void onCancel() {
                        Bundle data = new Bundle();
                        data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                        data.putInt(Facebook.MSG_KEY_STATE, 3);
                        data.putString(Facebook.MSG_KEY_ERROR, "Login canceled.");
                        respond(action, data);
                    }

                    @Override
                    public void onError(FacebookException error) {
                        Bundle data = new Bundle();
                        data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                        data.putInt(Facebook.MSG_KEY_STATE, 0);
                        data.putString(Facebook.MSG_KEY_ERROR, error.toString());
                        respond(action, data);
                    }

                });

                LoginManager.getInstance().logInWithReadPermissions(parent, Arrays.asList("public_profile", "email", "user_friends"));
            }

        } else if (action.equals(Facebook.ACTION_REQ_READ_PERMS) ||
                   action.equals(Facebook.ACTION_REQ_PUB_PERMS)) {

            if (AccessToken.getCurrentAccessToken() != null) {

                final String[] permissions = extras.getStringArray(Facebook.INTENT_EXTRA_PERMISSIONS);
                LoginManager.getInstance().registerCallback(callbackManager, new FacebookCallback<LoginResult>() {

                    @Override
                    public void onSuccess(LoginResult result) {
                        Bundle data = new Bundle();
                        data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                        respond(action, data);
                    }

                    @Override
                    public void onCancel() {
                        Bundle data = new Bundle();
                        data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                        data.putString(Facebook.MSG_KEY_ERROR, "Login canceled.");
                        respond(action, data);
                    }

                    @Override
                    public void onError(FacebookException error) {
                        Bundle data = new Bundle();
                        data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                        data.putString(Facebook.MSG_KEY_ERROR, error.toString());
                        respond(action, data);
                    }

                });

                if (action.equals(Facebook.ACTION_REQ_READ_PERMS)) {
                    LoginManager.getInstance().logInWithReadPermissions(parent, Arrays.asList(permissions));
                } else {
                    int defaultAudienceInt = Integer.parseInt(extras.getString(Facebook.INTENT_EXTRA_AUDIENCE));
                    LoginManager.getInstance().setDefaultAudience(convertDefaultAudience(defaultAudienceInt));
                    LoginManager.getInstance().logInWithPublishPermissions(parent, Arrays.asList(permissions));
                }

            } else {
                Bundle data = new Bundle();
                data.putString(Facebook.MSG_KEY_ERROR, "User is not logged in.");
                respond(action, data);
            }

        } else if (action.equals(Facebook.ACTION_SHOW_DIALOG)) {

            if (AccessToken.getCurrentAccessToken() != null) {

                String dialogType = extras.getString(Facebook.INTENT_EXTRA_DIALOGTYPE);
                Bundle dialogParams = extras.getBundle(Facebook.INTENT_EXTRA_DIALOGPARAMS);

                if (dialogType.equals("feed") || dialogType.equals("link")) {

                    ShareDialog shareDialog = new ShareDialog(parent);
                    ShareLinkContent content = new ShareLinkContent.Builder()
                        .setContentUrl(Uri.parse(dialogParams.getString("contentURL", "")))
                        .setContentTitle(dialogParams.getString("contentTitle", ""))
                        .setImageUrl(Uri.parse(dialogParams.getString("imageURL", "")))
                        .setContentDescription(dialogParams.getString("contentDescription", ""))
                        .build();

                        shareDialog.registerCallback(callbackManager, new FacebookCallback<Sharer.Result>() {

                            @Override
                            public void onSuccess(Sharer.Result result) {
                                Bundle data = new Bundle();
                                data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                                data.putString(Facebook.MSG_KEY_POST_ID, result.getPostId());
                                respond(action, data);
                            }

                            @Override
                            public void onCancel() {
                                Bundle data = new Bundle();
                                data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                                data.putString(Facebook.MSG_KEY_ERROR, "Dialog canceled.");
                                respond(action, data);
                            }

                            @Override
                            public void onError(FacebookException error) {
                                Bundle data = new Bundle();
                                data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                                data.putString(Facebook.MSG_KEY_ERROR, error.toString());
                                respond(action, data);
                            }

                        });
                        shareDialog.show(parent, content);

                } else if (dialogType.equals("appinvite")) {

                    AppInviteDialog appInviteDialog = new AppInviteDialog(parent);
                    AppInviteContent content = new AppInviteContent.Builder()
                        .setApplinkUrl(dialogParams.getString("appLinkURL", ""))
                        .setPreviewImageUrl(dialogParams.getString("appInvitePreviewImageURL", ""))
                        .build();

                        appInviteDialog.registerCallback(callbackManager, new FacebookCallback<AppInviteDialog.Result>() {

                            @Override
                            public void onSuccess(AppInviteDialog.Result result) {
                                Bundle data = new Bundle();
                                data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                                data.putString(Facebook.MSG_KEY_POST_ID, result.getData().toString());
                                respond(action, data);
                            }

                            @Override
                            public void onCancel() {
                                Bundle data = new Bundle();
                                data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                                data.putString(Facebook.MSG_KEY_ERROR, "Dialog canceled.");
                                respond(action, data);
                            }

                            @Override
                            public void onError(FacebookException error) {
                                Bundle data = new Bundle();
                                data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                                data.putString(Facebook.MSG_KEY_ERROR, error.toString());
                                respond(action, data);
                            }

                        });
                        appInviteDialog.show(parent, content);

                } else if (dialogType.equals("gamerequest") || dialogType.equals("apprequest")) {

                    GameRequestDialog appInviteDialog = new GameRequestDialog(parent);

                    ArrayList<String> suggestions = new ArrayList<String>();
                    final String[] recipients = dialogParams.getStringArray("recipients");
                    if (recipients != null) {
                        suggestions.addAll(Arrays.asList(recipients));
                    }

                    GameRequestContent.Builder content = new GameRequestContent.Builder()
                        .setTitle(dialogParams.getString("title", ""))
                        .setMessage(dialogParams.getString("message", ""))
                        .setData(dialogParams.getString("data"))
                        .setObjectId(dialogParams.getString("objectID"));

                        int actionInt = Integer.parseInt(dialogParams.getString("actionType", "0"));
                        content.setActionType(convertGameRequestAction(actionInt));

                        // filters and suggestions are mutually exclusive
                        int filters = Integer.parseInt(dialogParams.getString("filters", "-1"));
                        if (filters != -1) {
                            content.setFilters(convertGameRequestFilters(filters));
                        } else {
                            content.setSuggestions(suggestions);
                        }

                        System.out.println("waog:: " + content.toString());

                        appInviteDialog.registerCallback(callbackManager, new FacebookCallback<GameRequestDialog.Result>() {

                            @Override
                            public void onSuccess(GameRequestDialog.Result result) {
                                Bundle data = new Bundle();
                                data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                                data.putString(Facebook.MSG_KEY_POST_ID, result.getRequestId());
                                respond(action, data);
                            }

                            @Override
                            public void onCancel() {
                                Bundle data = new Bundle();
                                data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                                data.putString(Facebook.MSG_KEY_ERROR, "Dialog canceled.");
                                respond(action, data);
                            }

                            @Override
                            public void onError(FacebookException error) {
                                Bundle data = new Bundle();
                                data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                                data.putString(Facebook.MSG_KEY_ERROR, error.toString());
                                respond(action, data);
                            }

                        });
                        appInviteDialog.show(parent, content.build());

                } else {
                    Bundle data = new Bundle();
                    data.putString(Facebook.MSG_KEY_ERROR, "Unknown dialog type.");
                    respond(action, data);
                }

            } else {
                Bundle data = new Bundle();
                data.putString(Facebook.MSG_KEY_ERROR, "User is not logged in.");
                respond(action, data);
            }

        }

    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        callbackManager.onActivityResult(requestCode, resultCode, data);
    }

}
