package com.defold.push;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;

import java.util.List;
import java.util.ArrayList;

import org.json.JSONException;
import org.json.JSONObject;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.AlarmManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.v4.app.NotificationCompat;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.gcm.GoogleCloudMessaging;

public class Push {

    public static final String TAG = "push";
    public static final String ACTION_FORWARD_PUSH = "com.defold.push.FORWARD";
    public static final String SAVED_PUSH_MESSAGE_NAME = "saved_push_message";
    private static final int PLAY_SERVICES_RESOLUTION_REQUEST = 9000;

    private String senderId = "";

    private static Push instance;
    private static AlarmManager am = null;
    private static List<Intent> scheduledNotifications;
    private IPushListener listener;

    private GoogleCloudMessaging gcm;

    public void start(Activity activity, IPushListener listener, String senderId) {
        Log.d(TAG, String.format("Push started (%s %s)", listener, senderId));
        this.listener = listener;
        this.senderId = senderId;
    }

    public void stop() {
        Log.d(TAG, "Push stopped");
        this.listener = null;
    }

    public void register(final Activity activity) {
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    startGooglePlay(activity);
                    loadSavedMessages(activity);
                } catch (Throwable e) {
                    Log.e(TAG, "Failed to register", e);
                    sendRegistrationResult(null, e.getLocalizedMessage());
                }
            }
        });
    }

    public void scheduleNotification(final Activity activity, int notificationId, int timeOffsetSec, String title, String message, String userdata, String group, int priority) {

        if (am == null) {
            am = (AlarmManager) activity.getSystemService(activity.ALARM_SERVICE);
            scheduledNotifications = new ArrayList<Intent>();
        }
        Intent intent = new Intent(activity, LocalNotificationReceiver.class);

        Bundle extras = new Bundle();
        int uid = (int)System.currentTimeMillis();
        extras.putInt("uid", uid);
        extras.putString("title", title);
        extras.putString("message", message);
        extras.putInt("priority", priority);
        extras.putString("group", group);
        extras.putString("userdata", userdata);
        intent.putExtras(extras);
        intent.setAction("uid" + uid);

        PendingIntent pendingIntent = PendingIntent.getBroadcast(activity, 0, intent, PendingIntent.FLAG_ONE_SHOT);
        scheduledNotifications.add(intent);
        am.set(AlarmManager.RTC_WAKEUP, System.currentTimeMillis() + timeOffsetSec * 1000, pendingIntent);

    }

    public void cancelNotification(final Activity activity, int notificationId)
    {
        if (am == null) {
            am = (AlarmManager) activity.getSystemService(activity.ALARM_SERVICE);
            scheduledNotifications = new ArrayList<Intent>();
        }

        if (notificationId < 0 || notificationId >= scheduledNotifications.size())
        {
            return;
        }

        Intent intent = scheduledNotifications.get(notificationId);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(activity, 0, intent, PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_ONE_SHOT);
        am.cancel(pendingIntent);
    }

    public static Push getInstance() {
        if (instance == null) {
            instance = new Push();
        }
        return instance;
    }

    private void startGooglePlay(Activity activity) {
        if (checkPlayServices(activity)) {
            gcm = GoogleCloudMessaging.getInstance(activity);
            registerInBackground(activity);
        } else {
            Log.w(TAG, "No valid Google Play Services APK found.");
        }
    }

    private boolean checkPlayServices(Activity activity) {
        int resultCode = GooglePlayServicesUtil
                .isGooglePlayServicesAvailable(activity);
        if (resultCode != ConnectionResult.SUCCESS) {
            if (GooglePlayServicesUtil.isUserRecoverableError(resultCode)) {
                GooglePlayServicesUtil.getErrorDialog(resultCode, activity,
                        PLAY_SERVICES_RESOLUTION_REQUEST).show();
            } else {
                Log.w(TAG, "This device is not supported.");
            }
            return false;
        }
        return true;
    }

    private void sendRegistrationResult(String regid, String errorMessage) {
        if (listener != null) {
            listener.onRegistration(regid, errorMessage);
        } else {
            Log.e(TAG, "No listener callback set");
        }
    }

    private void registerInBackground(final Context context) {
        new AsyncTask<Void, Void, Void>() {
            @Override
            protected Void doInBackground(Void... params) {
                try {
                    if (gcm == null) {
                        gcm = GoogleCloudMessaging.getInstance(context);
                    }
                    String regid = gcm.register(senderId);
                    sendRegistrationResult(regid, null);
                } catch (IOException e) {
                    Log.e(TAG, "Failed to register", e);
                    sendRegistrationResult(null, e.getLocalizedMessage());
                } catch (Throwable e) {
                    Log.e(TAG, "Failed to register", e);
                    sendRegistrationResult(null, e.getLocalizedMessage());
                }
                return null;
            }

        }.execute(null, null, null);
    }

    private void loadSavedMessages(Context context) {
        BufferedReader r = null;
        try {
            r = new BufferedReader(new InputStreamReader(
                    context.openFileInput(SAVED_PUSH_MESSAGE_NAME)));
            String json = "";
            String line = r.readLine();
            while (line != null) {
                json += line;
                line = r.readLine();
            }
            this.listener.onMessage(json);

        } catch (FileNotFoundException e) {
        } catch (IOException e) {
            Log.e(Push.TAG, "Failed to read push message from disk", e);
        } finally {
            if (r != null) {
                try {
                    r.close();
                } catch (IOException e) {
                }
            }
            context.deleteFile(SAVED_PUSH_MESSAGE_NAME);
        }
    }

    static JSONObject toJson(Bundle bundle) {
        JSONObject o = new JSONObject();
        for (String k : bundle.keySet()) {
            try {
                o.put(k, bundle.getString(k));
            } catch (JSONException e) {
                Log.e(TAG, "failed to create json-object", e);
            }
        }

        return o;
    }

    void onPush(Context context, Intent intent) {
        Bundle extras = intent.getExtras();
        GoogleCloudMessaging gcm = GoogleCloudMessaging.getInstance(context);
        String messageType = gcm.getMessageType(intent);

        if (!extras.isEmpty()) {
            if (GoogleCloudMessaging.MESSAGE_TYPE_MESSAGE.equals(messageType)) {
                JSONObject o = toJson(extras);
                String msg = o.toString();
                Log.d(TAG, "message received: " + msg);
                if (listener != null) {
                    Log.d(TAG, "forwarding message to application");
                    listener.onMessage(msg);
                }
                Log.d(TAG, "creating notification for message");
                sendNotification(context, extras);
            } else {
                Log.i(TAG, String.format("unhandled message type: %s",
                        messageType));
            }
        }
    }

    void onLocalPush(String msg) {
        if (listener != null) {
            Log.d(TAG, "forwarding local message to application");
            this.listener.onMessage(msg);
        }

    }

    private void sendNotification(Context context, Bundle extras) {

        NotificationManager nm = (NotificationManager) context
                .getSystemService(Context.NOTIFICATION_SERVICE);

        Intent intent = new Intent(context, PushDispatchActivity.class)
                .setAction(ACTION_FORWARD_PUSH);

        for (String key : extras.keySet()) {
            intent.putExtra(key, extras.getString(key));
        }

        int id = (int) (System.currentTimeMillis() % Integer.MAX_VALUE);
        PendingIntent contentIntent = PendingIntent.getActivity(context, id,
                intent, 0);

        String alert = extras.getString("alert");
        if (alert == null) {
            Log.w(TAG, "Missing alert field in push message");
            alert = "New message";
        }

        ApplicationInfo info = context.getApplicationInfo();
        NotificationCompat.Builder builder = new NotificationCompat.Builder(
                context).setSmallIcon(info.icon)
                .setContentTitle(info.loadLabel(context.getPackageManager()))
                .setStyle(new NotificationCompat.BigTextStyle().bigText(alert))
                .setContentText(alert);

        builder.setContentIntent(contentIntent);
        Notification notification = builder.build();
        notification.flags |= Notification.FLAG_AUTO_CANCEL;
        nm.notify(id, notification);
    }

}
