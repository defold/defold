package com.defold.push;

import com.dynamo.android.DefoldActivity;

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
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.v4.app.NotificationCompat;
import android.util.Log;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.gcm.GoogleCloudMessaging;

public class Push {

    public static final String TAG = "push";
    public static final String DEFOLD_ACTIVITY = "com.dynamo.android.DefoldActivity";
    public static final String ACTION_FORWARD_PUSH = "com.defold.push.FORWARD";
    public static final String SAVED_PUSH_MESSAGE_NAME = "saved_push_message";
    public static final String SAVED_LOCAL_MESSAGE_NAME = "saved_local_message";
    private static final int PLAY_SERVICES_RESOLUTION_REQUEST = 9000;

    private String senderId = "";

    private static Push instance;
    private static AlarmManager am = null;
    private IPushListener listener = null;

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
                    loadSavedLocalMessages(activity);
                    loadSavedMessages(activity);
                } catch (Throwable e) {
                    Log.e(TAG, "Failed to register", e);
                    sendRegistrationResult(null, e.getLocalizedMessage());
                }
            }
        });
    }

    public void scheduleNotification(final Activity activity, int notificationId, long timestampMillis, String title, String message, String payload, int priority) {

        if (am == null) {
            am = (AlarmManager) activity.getSystemService(activity.ALARM_SERVICE);
        }
        Intent intent = new Intent(activity, LocalNotificationReceiver.class);

        Bundle extras = new Bundle();
        int uid = notificationId;
        extras.putInt("uid", uid);
        extras.putString("title", title);
        extras.putString("message", message);
        extras.putInt("priority", priority);
        extras.putInt("smallIcon", activity.getResources().getIdentifier("push_icon_small", "drawable", activity.getPackageName()));
        extras.putInt("largeIcon", activity.getResources().getIdentifier("push_icon_large", "drawable", activity.getPackageName()));
        extras.putString("payload", payload);
        intent.putExtras(extras);
        intent.setAction("uid" + uid);

        PendingIntent pendingIntent = PendingIntent.getBroadcast(activity, 0, intent, PendingIntent.FLAG_ONE_SHOT);
        am.set(AlarmManager.RTC_WAKEUP, timestampMillis, pendingIntent);

    }

    public void cancelNotification(final Activity activity, int notificationId, String title, String message, String payload, int priority)
    {
        if (am == null) {
            am = (AlarmManager) activity.getSystemService(activity.ALARM_SERVICE);
        }

        if (notificationId < 0) {
            return;
        }

        Intent intent = new Intent(activity, LocalNotificationReceiver.class);

        Bundle extras = new Bundle();
        int uid = notificationId;
        extras.putInt("uid", uid);
        extras.putString("title", title);
        extras.putString("message", message);
        extras.putInt("priority", priority);
        extras.putString("payload", payload);
        intent.putExtras(extras);
        intent.setAction("uid" + uid);
        PendingIntent pendingIntent = PendingIntent.getBroadcast(activity, 0, intent, PendingIntent.FLAG_CANCEL_CURRENT | PendingIntent.FLAG_ONE_SHOT);
        am.cancel(pendingIntent);
    }

    public boolean hasListener() {
        if (this.listener != null) {
            return true;
        }

        return false;
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
            // Read saved remote push notifications
            r = new BufferedReader(new InputStreamReader(
                    context.openFileInput(SAVED_PUSH_MESSAGE_NAME)));
            boolean wasActivated = Boolean.parseBoolean(r.readLine());
            String json = "";
            String line = r.readLine();
            while (line != null) {
                json += line;
                line = r.readLine();
            }
            this.listener.onMessage(json, wasActivated);

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

    private void loadSavedLocalMessages(Context context) {
        BufferedReader r = null;
        try {
            // Read saved local notifications
            r = new BufferedReader(new InputStreamReader(
                    context.openFileInput(SAVED_LOCAL_MESSAGE_NAME)));
            int id = Integer.parseInt(r.readLine());
            boolean wasActivated = Boolean.parseBoolean(r.readLine());
            String json = "";
            String line = r.readLine();
            while (line != null) {
                json += line;
                line = r.readLine();
            }
            this.listener.onLocalMessage(json, id, wasActivated);
        } catch (FileNotFoundException e) {
        } catch (IOException e) {
            Log.e(Push.TAG, "Failed to read local message from disk", e);
        } finally {
            if (r != null) {
                try {
                    r.close();
                } catch (IOException e) {
                }
            }
            context.deleteFile(SAVED_LOCAL_MESSAGE_NAME);
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

        // Note in regards to the 'from' != "google.com/iid" check:
        // - In mid/late 2015 Google updated their push service. During this
        //   change "ghost" messages began popping up in Defold, some even
        //   the first time the apps were run. The reason was Google sending
        //   some of their own messages to the devices with "commands", for
        //   example telling the GCM-library to request a new device token.
        //   These commands where not handled in our GCM-library version,
        //   thus we simply forwarded them below as normal pushes.
        //   More info: JIRA issue DEF-1354
        //              http://stackoverflow.com/questions/30479424/
        if (!extras.isEmpty() &&
            !extras.getString("from").equals("google.com/iid")) {

            if (GoogleCloudMessaging.MESSAGE_TYPE_MESSAGE.equals(messageType)) {
                JSONObject o = toJson(extras);
                String msg = o.toString();
                boolean wasActivated = !DefoldActivity.isActivityVisible();
                Log.d(TAG, "message received: " + msg);
                if (listener != null) {
                    Log.d(TAG, "forwarding message to application");
                    listener.onMessage(msg, wasActivated);
                }

                Log.d(TAG, "creating notification for message");
                sendNotification(context, extras, wasActivated);
            } else {
                Log.i(TAG, String.format("unhandled message type: %s",
                        messageType));
            }
        }
    }

    void onLocalPush(String msg, int id, boolean wasActivated) {
        if (listener != null) {
            this.listener.onLocalMessage(msg, id, wasActivated);
        }

    }

    private void sendNotification(Context context, Bundle extras, boolean wasActivated) {

        NotificationManager nm = (NotificationManager) context
                .getSystemService(Context.NOTIFICATION_SERVICE);

        Intent intent = new Intent(context, PushDispatchActivity.class)
                .setAction(ACTION_FORWARD_PUSH);
        intent.putExtra("wasActivated", (byte) (wasActivated ? 1 : 0));

        if (DefoldActivity.isActivityVisible()) {
            // Send the push notification directly to the dispatch
            // activity if app is visible.
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
        } else {
            for (String key : extras.keySet()) {
                intent.putExtra(key, extras.getString(key));
            }

            int id = (int) (System.currentTimeMillis() % Integer.MAX_VALUE);
            PendingIntent contentIntent = PendingIntent.getActivity(context, id,
                    intent, 0);

            String fieldTitle = null;
            String fieldText = null;

            // Try to find field names from manifest file
            PackageManager pm = context.getPackageManager();
            try {
                ComponentName cn = new ComponentName(context, DEFOLD_ACTIVITY);
                ActivityInfo activityInfo = pm.getActivityInfo(cn, PackageManager.GET_META_DATA);

                Bundle bundle = activityInfo.metaData;
                if (bundle != null) {
                    fieldTitle = bundle.getString("com.defold.push.field_title", null);
                    fieldText = bundle.getString("com.defold.push.field_text", "alert");
                } else {
                    Log.w(TAG, "Bundle was null, could not get meta data from manifest.");
                }
            } catch (PackageManager.NameNotFoundException e) {
                Log.w(TAG, "Could not get activity info, needed to get push field conversion.");
            }

            ApplicationInfo info = context.getApplicationInfo();
            String title = info.loadLabel(pm).toString();
            if (fieldTitle != null && extras.getString(fieldTitle) != null) {
                title = extras.getString(fieldTitle);
            }

            String text = extras.getString(fieldText);
            if (text == null) {
                Log.w(TAG, "Missing text field in push message");
                text = "New message";
            }

            NotificationCompat.Builder builder = new NotificationCompat.Builder(context)
                    .setContentTitle(title)
                    .setStyle(new NotificationCompat.BigTextStyle().bigText(text))
                    .setContentText(text);

            // Find icons if they were supplied, fallback to app icon
            int smallIconId = context.getResources().getIdentifier("push_icon_small", "drawable", context.getPackageName());
            int largeIconId = context.getResources().getIdentifier("push_icon_large", "drawable", context.getPackageName());
            if (smallIconId == 0) {
                smallIconId = info.icon;
            }
            if (largeIconId == 0) {
                largeIconId = info.icon;
            }

            // Get bitmap for large icon resource
            try {
                Resources resources = pm.getResourcesForApplication(info);
                Bitmap largeIconBitmap = BitmapFactory.decodeResource(resources, largeIconId);
                builder.setLargeIcon(largeIconBitmap);
            } catch (PackageManager.NameNotFoundException e) {
                Log.w(TAG, "Could not get application resources.");
            }

            builder.setSmallIcon(smallIconId);

            builder.setContentIntent(contentIntent);
            Notification notification = builder.build();
            notification.flags |= Notification.FLAG_AUTO_CANCEL;
            nm.notify(id, notification);
        }
    }

}
