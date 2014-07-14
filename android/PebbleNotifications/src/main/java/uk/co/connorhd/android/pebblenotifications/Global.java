package uk.co.connorhd.android.pebblenotifications;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.provider.Settings;

import java.util.UUID;

public class Global {
    public static UUID appUUID = UUID.fromString("f0d3403d-9cec-4101-8502-2a801fe24761");

    //Pebble message codes
    public static final int MSG_NOTIFICATION_DETAILS = 100;
    public static final int MSG_NOTIFICATION_TITLE = 200;
    public static final int MSG_LOAD_NOTIFICATION_ID = 300;
    public static final int MSG_NOTIFICATIONS_CHANGED = 500;
    public static final int MSG_NO_NOTIFICATIONS = 700;
    public static final int MSG_PHONE_BATTERY = 800;

    //Battery
    public static int getBatteryLevel(Context cxt) {
        Intent batteryIntent = cxt.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));

        if (batteryIntent != null) {
            int level = batteryIntent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
            int scale = batteryIntent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);

            if (level == -1 || scale == -1) {
                return 50;
            }

            return (int)(((float)level / (float)scale) * 100.0f);
        }

        return 50;
    }

    // Based on http://stackoverflow.com/a/21392852
    // Returns true or false based on whether app has access to notifications
    public static boolean notificationAccess(Activity activity) {
        if (activity != null) {
            String enabledNotificationListeners = Settings.Secure.getString(activity.getContentResolver(), "enabled_notification_listeners");

            // Check to see if the enabledNotificationListeners String exists and contains our package name
            return !(enabledNotificationListeners == null || !enabledNotificationListeners.contains(activity.getPackageName()));
        }
        return false;
    }


    //Singleton stuff
    private static Global mInstance = null;
    private Global() {}
    public static Global getInstance() {
        if (mInstance == null) {
            mInstance = new Global();
        }
        return mInstance;
    }
}