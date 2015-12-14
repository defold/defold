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
    private Message onAbortedMessage;

    // Must match values from facebook.h
    private enum State {
        STATE_CREATED              (1),
        STATE_CREATED_TOKEN_LOADED (2),
        STATE_CREATED_OPENING      (3),
        STATE_OPEN                 (4),
        STATE_OPEN_TOKEN_EXTENDED  (5),
        STATE_CLOSED_LOGIN_FAILED  (6),
        STATE_CLOSED               (7);

        private final int value;
        private State(int value) { this.value = value; };
        public int getValue() { return this.value; };

    }

    /*
     * Functions to convert Lua enums to corresponding Facebook enums.
     * Switch values are from enums defined in facebook_android.cpp.
     */
    private DefaultAudience convertDefaultAudience(int fromLuaInt) {
        switch (fromLuaInt) {
            case 2:
                return DefaultAudience.ONLY_ME;
            case 3:
                return DefaultAudience.FRIENDS;
            case 4:
                return DefaultAudience.EVERYONE;
            case 1:
            default:
                return DefaultAudience.NONE;
        }
    }

    private GameRequestContent.ActionType convertGameRequestAction(int fromLuaInt) {
        switch (fromLuaInt) {
            case 3:
                return GameRequestContent.ActionType.ASKFOR;
            case 4:
                return GameRequestContent.ActionType.TURN;
            case 2:
            default:
                return GameRequestContent.ActionType.SEND;
        }
    }

    private GameRequestContent.Filters convertGameRequestFilters(int fromLuaInt) {
        switch (fromLuaInt) {
            case 3:
                return GameRequestContent.Filters.APP_NON_USERS;
            case 2:
            default:
                return GameRequestContent.Filters.APP_USERS;
        }
    }

    private class DefaultDialogCallback<T> implements FacebookCallback<T> {

        @Override
        public void onSuccess(T result) { /* needs to be implemented for each dialog */ }

        @Override
        public void onCancel() {
            Bundle data = new Bundle();
            data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
            data.putString(Facebook.MSG_KEY_ERROR, "Dialog canceled");
            respond(Facebook.ACTION_SHOW_DIALOG, data);
        }

        @Override
        public void onError(FacebookException error) {
            Bundle data = new Bundle();
            data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
            data.putString(Facebook.MSG_KEY_ERROR, error.toString());
            respond(Facebook.ACTION_SHOW_DIALOG, data);
        }

    }

    private void respond(final String action, final Bundle data) {
        onAbortedMessage = null; // only kept for when respond() never happens.
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
    private void UpdateUserData() {
        GraphRequest request = GraphRequest.newMeRequest(
            AccessToken.getCurrentAccessToken(),
             new GraphRequest.GraphJSONObjectCallback() {
                @Override
                public void onCompleted(JSONObject object, GraphResponse response) {
                   Bundle data = new Bundle();
                   if (response.getError() == null) {
                       data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                       data.putString(Facebook.MSG_KEY_USER, object.toString());
                       data.putString(Facebook.MSG_KEY_ACCESS_TOKEN, AccessToken.getCurrentAccessToken().getToken());
                       data.putInt(Facebook.MSG_KEY_STATE, State.STATE_OPEN.getValue());
                   } else {
                       data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                       data.putInt(Facebook.MSG_KEY_STATE, State.STATE_CLOSED_LOGIN_FAILED.getValue());
                       data.putString(Facebook.MSG_KEY_ERROR, response.getError().getErrorMessage());
                   }
                   respond(Facebook.ACTION_LOGIN, data);
                }
            });
        Bundle parameters = new Bundle();
        parameters.putString("fields", "last_name,link,id,gender,email,locale,name,first_name,updated_time");
        request.setParameters(parameters);
        request.executeAsync();
    }

    private void actionLogin() {

        if (AccessToken.getCurrentAccessToken() != null) {
            UpdateUserData();
        } else {
            LoginManager.getInstance().registerCallback(callbackManager, new FacebookCallback<LoginResult>() {

                @Override
                public void onSuccess(LoginResult result) {
                    UpdateUserData();
                }

                @Override
                public void onCancel() {
                    Bundle data = new Bundle();
                    data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                    data.putInt(Facebook.MSG_KEY_STATE, State.STATE_CLOSED_LOGIN_FAILED.getValue());
                    data.putString(Facebook.MSG_KEY_ERROR, "Login canceled");
                    respond(Facebook.ACTION_LOGIN, data);
                }

                @Override
                public void onError(FacebookException error) {
                    Bundle data = new Bundle();
                    data.putBoolean(Facebook.MSG_KEY_SUCCESS, false);
                    data.putInt(Facebook.MSG_KEY_STATE, State.STATE_CLOSED_LOGIN_FAILED.getValue());
                    data.putString(Facebook.MSG_KEY_ERROR, error.toString());
                    respond(Facebook.ACTION_LOGIN, data);
                }

            });

            LoginManager.getInstance().logInWithReadPermissions(parent, Arrays.asList("public_profile", "email", "user_friends"));
        }

    }

    private void actionRequestPermissions( final String action, final Bundle extras ) {

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
                    data.putString(Facebook.MSG_KEY_ERROR, "Login canceled");
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
                int defaultAudienceInt = extras.getInt(Facebook.INTENT_EXTRA_AUDIENCE);
                LoginManager.getInstance().setDefaultAudience(convertDefaultAudience(defaultAudienceInt));
                LoginManager.getInstance().logInWithPublishPermissions(parent, Arrays.asList(permissions));
            }

        } else {
            Bundle data = new Bundle();
            data.putString(Facebook.MSG_KEY_ERROR, "User is not logged in");
            respond(action, data);
        }
    }

    private void actionShowDialog( final Bundle extras ) {

        String dialogType = extras.getString(Facebook.INTENT_EXTRA_DIALOGTYPE);
        Bundle dialogParams = extras.getBundle(Facebook.INTENT_EXTRA_DIALOGPARAMS);

        if (dialogType.equals("appinvite")) {

            AppInviteDialog appInviteDialog = new AppInviteDialog(parent);
            AppInviteContent content = new AppInviteContent.Builder()
                .setApplinkUrl(dialogParams.getString("url", ""))
                .setPreviewImageUrl(dialogParams.getString("preview_image_url", ""))
                .build();

            appInviteDialog.registerCallback(callbackManager, new DefaultDialogCallback<AppInviteDialog.Result>() {

                @Override
                public void onSuccess(AppInviteDialog.Result result) {
                    Bundle data = new Bundle();
                    data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                    data.putBundle(Facebook.MSG_KEY_DIALOG_RESULT, result.getData());
                    respond(Facebook.ACTION_SHOW_DIALOG, data);
                }

            });

            // canShow false when browser/app not available.
            if (appInviteDialog.canShow()) {
                appInviteDialog.show(parent, content);
            } else {
                Bundle data = new Bundle();
                data.putString(Facebook.MSG_KEY_ERROR, "Dialog not available");
                respond(Facebook.ACTION_SHOW_DIALOG, data);
            }

        } else {

            // All dialogs that require login
            if (AccessToken.getCurrentAccessToken() == null) {

                Bundle data = new Bundle();
                data.putString(Facebook.MSG_KEY_ERROR, "User is not logged in");
                respond(Facebook.ACTION_SHOW_DIALOG, data);

            } else {

                if (dialogType.equals("feed")) {

                    ShareDialog shareDialog = new ShareDialog(parent);
                    ShareLinkContent.Builder content = new ShareLinkContent.Builder()
                        .setContentUrl(Uri.parse(dialogParams.getString("link", "")))
                        .setContentTitle(dialogParams.getString("caption", ""))
                        .setImageUrl(Uri.parse(dialogParams.getString("picture", "")))
                        .setContentDescription(dialogParams.getString("description", ""))
                        .setPlaceId(dialogParams.getString("place_id", ""))
                        .setRef(dialogParams.getString("ref", ""));

                    String peopleIdsString = dialogParams.getString("people_ids", null);
                    if (peopleIdsString != null) {
                        content.setPeopleIds(Arrays.asList(peopleIdsString.split(",")));
                    }

                    shareDialog.registerCallback(callbackManager, new DefaultDialogCallback<Sharer.Result>() {

                        @Override
                        public void onSuccess(Sharer.Result result) {
                            Bundle data = new Bundle();
                            Bundle subdata = new Bundle();
                            subdata.putString("post_id", result.getPostId());
                            data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                            data.putBundle(Facebook.MSG_KEY_DIALOG_RESULT, subdata);
                            respond(Facebook.ACTION_SHOW_DIALOG, data);
                        }

                    });

                    shareDialog.show(parent, content.build());

                } else if (dialogType.equals("apprequests") || dialogType.equals("apprequest")) {

                    GameRequestDialog appInviteDialog = new GameRequestDialog(parent);

                    ArrayList<String> suggestionsArray = new ArrayList<String>();
                    String suggestionsString = dialogParams.getString("suggestions", null);
                    if (suggestionsString != null) {
                        String [] suggestions = suggestionsString.split(",");
                        suggestionsArray.addAll(Arrays.asList(suggestions));
                    }

                    // comply with JS way of specifying recipients/to
                    String toString = null;

                    // Recipients field does not exist in FB SDK 4.3 for Android.
                    // For now we fill the "to" field with the comma separated user ids,
                    // but when we upgrade to a newer version with the recipients field
                    // we should fill the correct recipients field instead.
                    if (dialogParams.getString("to", null) != null) {
                        toString = dialogParams.getString("to");
                    }

                    if (dialogParams.getString("recipients", null) != null) {
                        toString = dialogParams.getString("recipients");
                    }

                    GameRequestContent.Builder content = new GameRequestContent.Builder()
                        .setTitle(dialogParams.getString("title", ""))
                        .setMessage(dialogParams.getString("message", ""))
                        .setData(dialogParams.getString("data"));

                    if (dialogParams.getString("object_id") != null) {
                        content.setObjectId(dialogParams.getString("object_id"));
                    }

                    if (dialogParams.getString("action_type") != null) {
                        int actionInt = Integer.parseInt(dialogParams.getString("action_type", "0"));
                        content.setActionType(convertGameRequestAction(actionInt));
                    }

                    // recipients, filters and suggestions are mutually exclusive
                    int filters = Integer.parseInt(dialogParams.getString("filters", "-1"));
                    if (toString != null) {
                        content.setTo(toString);
                    } else if (filters != -1) {
                        content.setFilters(convertGameRequestFilters(filters));
                    } else {
                        content.setSuggestions(suggestionsArray);
                    }

                    appInviteDialog.registerCallback(callbackManager, new DefaultDialogCallback<GameRequestDialog.Result>() {

                        @Override
                        public void onSuccess(GameRequestDialog.Result result) {
                            final String[] recipients = result.getRequestRecipients().toArray(new String[result.getRequestRecipients().size()]);
                            Bundle data = new Bundle();
                            Bundle subdata = new Bundle();
                            subdata.putString("request_id", result.getRequestId());
                            subdata.putStringArray("to", recipients);
                            data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                            data.putBundle(Facebook.MSG_KEY_DIALOG_RESULT, subdata);
                            respond(Facebook.ACTION_SHOW_DIALOG, data);
                        }

                    });

                    appInviteDialog.show(parent, content.build());

                } else {
                    Bundle data = new Bundle();
                    data.putString(Facebook.MSG_KEY_ERROR, "Unknown dialog type");
                    respond(Facebook.ACTION_SHOW_DIALOG, data);
                }
            }
        }

    }

    @Override
    public void onCreate(Bundle savedInstanceState, Activity parent) {
        this.parent = parent;
        Intent intent = parent.getIntent();
        Bundle extras = intent.getExtras();

        callbackManager = CallbackManager.Factory.create();

        final String action = intent.getAction();
        this.messenger = (Messenger) extras.getParcelable(Facebook.INTENT_EXTRA_MESSENGER);

        // Prepare a response to send in case we finish without having sent anything
        // (activity shut down etc). This is cleared by respond()
        Bundle abortedData = new Bundle();
        abortedData.putString(Facebook.MSG_KEY_ACTION, action);
        abortedData.putString(Facebook.MSG_KEY_ERROR, "Aborted");
        onAbortedMessage = new Message();
        onAbortedMessage.setData(abortedData);

        try {
            if (action.equals(Facebook.ACTION_LOGIN)) {
                actionLogin();
            } else if (action.equals(Facebook.ACTION_REQ_READ_PERMS) ||
                       action.equals(Facebook.ACTION_REQ_PUB_PERMS)) {
                actionRequestPermissions( action, extras );
            } else if (action.equals(Facebook.ACTION_SHOW_DIALOG)) {
                actionShowDialog( extras );
            }
        } catch (Exception e) {
            Bundle data = new Bundle();
            data.putString(Facebook.MSG_KEY_ERROR, e.getMessage());
            respond(action, data);
        }
    }

    @Override public void onDestroy() {
        if (onAbortedMessage != null) {
            try {
                messenger.send(onAbortedMessage);
            } catch (RemoteException e) {
                Log.wtf(TAG, e);
            }
        }
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        callbackManager.onActivityResult(requestCode, resultCode, data);
    }

}
