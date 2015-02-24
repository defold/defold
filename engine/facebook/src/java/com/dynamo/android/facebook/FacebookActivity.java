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

import com.dynamo.android.DispatcherActivity.PseudoActivity;
import com.facebook.FacebookOperationCanceledException;
import com.facebook.Request;
import com.facebook.Request.GraphUserCallback;
import com.facebook.Response;
import com.facebook.Session;
import com.facebook.Session.NewPermissionsRequest;
import com.facebook.Session.OpenRequest;
import com.facebook.SessionDefaultAudience;
import com.facebook.SessionState;
import com.facebook.Settings;
import com.facebook.model.GraphUser;

public class FacebookActivity implements PseudoActivity {
    private static final String TAG = "defold.facebook";

    private Messenger messenger;
    private Session session;
    private Activity parent;

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

    private boolean handleException(String action, Exception exception) {
        return handleException(action, exception, null);
    }

    private boolean handleException(String action, Exception exception, Bundle data) {
        if (exception != null) {
            if (data == null) {
                data = new Bundle();
            }
            if (!(exception instanceof FacebookOperationCanceledException)) {
                data.putString(Facebook.MSG_KEY_ERROR, exception.getMessage());
            }
            respond(action, data);
            return true;
        } else {
            return false;
        }
    }

    private boolean verifyPermissions(Session session, String[] permissions) {
        List<String> remaining = new ArrayList<String>(Arrays.asList(permissions));
        remaining.removeAll(session.getPermissions());
        return remaining.isEmpty();
    }

    @Override
    public void onCreate(Bundle savedInstanceState, Activity parent) {
        this.parent = parent;
        Intent intent = parent.getIntent();
        Bundle extras = intent.getExtras();
        this.messenger = (Messenger) extras.getParcelable(Facebook.INTENT_EXTRA_MESSENGER);
        final String action = intent.getAction();
        if (action.equals(Facebook.ACTION_LOGIN)) {
            // Disabling auto publish installs is a requirement by King
            Settings.setShouldAutoPublishInstall(false);
            this.session = new Session.Builder(parent).setApplicationId(extras.getString(Facebook.INTENT_EXTRA_APP_ID))
                    .build();
            Session.setActiveSession(session);
            final GraphUserCallback userCallback = new GraphUserCallback() {
                @Override
                public void onCompleted(GraphUser user, Response response) {
                    Bundle data = new Bundle();
                    if (user != null) {
                        data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                        data.putString(Facebook.MSG_KEY_USER, user.getInnerJSONObject()
                                .toString());
                        data.putString(Facebook.MSG_KEY_ACCESS_TOKEN, Session
                                .getActiveSession().getAccessToken());
                        data.putInt(Facebook.MSG_KEY_STATE, SessionState.OPENED.ordinal());
                    }
                    respond(action, data);
                }
            };
            Session.StatusCallback statusCallback = new Session.StatusCallback() {
                @Override
                public void call(Session session, final SessionState state, Exception exception) {
                    if (!state.equals(SessionState.OPENING)) {
                        try {
                            Bundle data = new Bundle();
                            data.putInt(Facebook.MSG_KEY_STATE, state.ordinal());
                            if (!handleException(action, exception, data)) {
                                if (state.isOpened()) {
                                    Request.executeMeRequestAsync(session, userCallback);
                                }
                            }
                        } finally {
                            session.removeCallback(this);
                        }
                    }
                }
            };
            session.openForRead(new OpenRequest(parent).setPermissions(Arrays.asList("public_profile", "email", "user_friends"))
                    .setCallback(statusCallback));
        } else if (action.equals(Facebook.ACTION_REQ_READ_PERMS)) {
            this.session = Session.getActiveSession();
            if (this.session != null && this.session.getState().isOpened()) {
                final String[] permissions = extras.getStringArray(Facebook.INTENT_EXTRA_PERMISSIONS);
                NewPermissionsRequest request = new NewPermissionsRequest(parent, Arrays.asList(permissions));
                // FB SDK is broken, add the callback to the session instead of request
                session.addCallback(
                        new Session.StatusCallback() {
                    @Override
                    public void call(Session session, SessionState state, Exception exception) {
                        try {
                            if (!handleException(action, exception)) {
                                if (state.isOpened() && verifyPermissions(session, permissions)) {
                                    Bundle data = new Bundle();
                                    data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                                    respond(action, data);
                                }
                            }
                        } finally {
                            session.removeCallback(this);
                        }
                    }
                });
                session.requestNewReadPermissions(request);
            } else {
                Bundle data = new Bundle();
                data.putString(Facebook.MSG_KEY_ERROR, "User is not logged in.");
                respond(action, data);
            }
        } else if (action.equals(Facebook.ACTION_REQ_PUB_PERMS)) {
            this.session = Session.getActiveSession();
            if (this.session != null && this.session.getState().isOpened()) {
                SessionDefaultAudience defaultAudience = SessionDefaultAudience.values()[extras
                        .getInt(Facebook.INTENT_EXTRA_AUDIENCE)];
                final String[] permissions = extras.getStringArray(Facebook.INTENT_EXTRA_PERMISSIONS);
                NewPermissionsRequest request = new NewPermissionsRequest(parent, Arrays.asList(permissions));
                request.setDefaultAudience(defaultAudience);
                // FB SDK is broken, add the callback to the session instead of request
                session.addCallback(
                        new Session.StatusCallback() {
                    @Override
                    public void call(Session session, SessionState state, Exception exception) {
                        try {
                            if (!handleException(action, exception)) {
                                if (state.isOpened() && verifyPermissions(session, permissions)) {
                                    Bundle data = new Bundle();
                                    data.putBoolean(Facebook.MSG_KEY_SUCCESS, true);
                                    respond(action, data);
                                }
                            }
                        } finally {
                            session.removeCallback(this);
                        }
                    }
                });
                session.requestNewPublishPermissions(request);
            } else {
                Bundle data = new Bundle();
                data.putString(Facebook.MSG_KEY_ERROR, "User is not logged in.");
                respond(action, data);
            }
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        this.session.onActivityResult(this.parent, requestCode, resultCode, data);
    }

}