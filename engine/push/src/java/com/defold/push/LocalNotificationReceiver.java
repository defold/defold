package com.defold.push;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Notification;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.v4.app.NotificationCompat;
import android.support.v4.content.WakefulBroadcastReceiver;
import android.os.Process;
import android.os.Bundle;
import android.util.Log;
import android.R;

public class LocalNotificationReceiver extends WakefulBroadcastReceiver {

    NotificationManager nm;

    @Override
    public void onReceive(Context context, Intent intent) {

        nm = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);

        Bundle extras = intent.getExtras();
        int id = extras.getInt("uid");

        Intent new_intent = new Intent(context, LocalPushDispatchActivity.class).setAction("com.defold.push.FORWARD");
        new_intent.putExtras(extras);
        new_intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);

        try {
            PendingIntent contentIntent = PendingIntent.getActivity(context, id, new_intent, PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_ONE_SHOT);

            ApplicationInfo info = context.getApplicationInfo();

            NotificationCompat.Builder builder = new NotificationCompat.Builder(context)
                .setContentTitle(extras.getString("title"))
                .setContentText(extras.getString("message"))
                .setWhen(System.currentTimeMillis())
                .setContentIntent(contentIntent)
                .setPriority(extras.getInt("priority"));

            // Find icons, if they were supplied
            int smallIconId = extras.getInt("smallIcon");
            int largeIconId = extras.getInt("largeIcon");
            if (smallIconId == 0) {
                smallIconId = info.icon;
            }
            if (largeIconId == 0) {
                largeIconId = info.icon;
            }

            // Get bitmap for large icon resource
            PackageManager pm = context.getPackageManager();
            Resources resources = pm.getResourcesForApplication(info);
            Bitmap largeIconBitmap = BitmapFactory.decodeResource(resources, largeIconId);

            builder.setSmallIcon(smallIconId);
            builder.setLargeIcon(largeIconBitmap);

            Notification notification = builder.build();
            notification.defaults = Notification.DEFAULT_ALL;
            notification.flags |= Notification.FLAG_AUTO_CANCEL;
            nm.notify(id, notification);

        } catch (PackageManager.NameNotFoundException e) {
            Log.e("LocalNotificationReceiver", "PackageManager.NameNotFoundException!");
        }

    }

}
