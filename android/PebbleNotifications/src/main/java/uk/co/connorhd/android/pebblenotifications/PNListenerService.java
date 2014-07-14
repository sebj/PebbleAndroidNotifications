package uk.co.connorhd.android.pebblenotifications;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.PowerManager;
import android.preference.PreferenceManager;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.text.SpannableString;
import android.util.Log;
import android.util.SparseArray;
import android.widget.RemoteViews;

import com.getpebble.android.kit.Constants;
import com.getpebble.android.kit.PebbleKit;
import com.getpebble.android.kit.PebbleKit.PebbleAckReceiver;
import com.getpebble.android.kit.PebbleKit.PebbleDataReceiver;
import com.getpebble.android.kit.PebbleKit.PebbleNackReceiver;
import com.getpebble.android.kit.util.PebbleDictionary;

public class PNListenerService extends NotificationListenerService {
    private static SharedPreferences sharedPrefs;

    private LinkedList<PebbleDictionary> outgoing = new LinkedList<PebbleDictionary>();
    private boolean flushing = false;
    private SparseArray<StatusBarNotification> sentNotifications = new SparseArray<StatusBarNotification>();

    private PebbleAckReceiver pAck;
    private PebbleNackReceiver pNack;
    private PebbleDataReceiver pData;

    public class BatteryReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            sendAndroidBattery();
        }
    }

    private void flushOutgoing() {
        if (!PebbleKit.isWatchConnected(getApplicationContext())) {
            outgoing.remove();
        }

        if (outgoing.size() > 0 && !flushing) {
            flushing = true;
            PebbleKit.sendDataToPebbleWithTransactionId(getApplicationContext(), Global.appUUID, outgoing.get(0), 174);
        }
    }

    @Override
    public void onCreate() {
        sharedPrefs = PreferenceManager.getDefaultSharedPreferences(this);

        IntentFilter batteryFilter = new IntentFilter();
        batteryFilter.addAction(Intent.ACTION_BATTERY_LOW);
        batteryFilter.addAction(Intent.ACTION_BATTERY_OKAY);
        batteryFilter.addAction(Intent.ACTION_POWER_CONNECTED);
        batteryFilter.addAction(Intent.ACTION_POWER_DISCONNECTED);
        registerReceiver(new BatteryReceiver(), batteryFilter);

        //Pebble ack
        pAck = new PebbleAckReceiver(Global.appUUID) {
            @Override
            public void receiveAck(Context context, int transactionId) {
                if (transactionId == 174) {
                    outgoing.remove();
                    flushing = false;
                    flushOutgoing();
                }
            }
        };
        registerReceiver(pAck, new IntentFilter(Constants.INTENT_APP_RECEIVE_ACK));

        //Pebble nack
        pNack = new PebbleNackReceiver(Global.appUUID) {
            @Override
            public void receiveNack(Context context, int transactionId) {
                if (transactionId == 174) {
                    // Give up sending data - TODO: something better?
                    outgoing.clear();
                    flushing = false;
                }

            }
        };
        registerReceiver(pNack, new IntentFilter(Constants.INTENT_APP_RECEIVE_NACK));

        //Pebble data
        pData = new PebbleDataReceiver(Global.appUUID) {
            @Override
            public void receiveData(Context context, int transactionId, PebbleDictionary data) {
                PebbleKit.sendAckToPebble(context, transactionId);
                if (data.contains(0)) {
                    // Watch wants data
                    sendNotifications();

                } else if (data.contains(1)) {
                    // Dismiss notification
                    int notifId = data.getInteger(1).intValue();
                    StatusBarNotification notif = sentNotifications.get(notifId);
                    if (notif != null) {
                        cancelNotification(notif.getPackageName(), notif.getTag(), notif.getId());
                    }

                } else if (data.contains(2)) {
                    // Try to open notification/perform notification action
                    int notifId = data.getInteger(2).intValue();
                    StatusBarNotification notif = sentNotifications.get(notifId);
                    if (notif != null) {
                        PendingIntent intent = notif.getNotification().contentIntent;
                        if (intent != null) {
                            try {
                                notif.getNotification().contentIntent.send();
                            } catch (PendingIntent.CanceledException e) {
                                //
                            }
                        }
                    }
                } else if (data.contains(4)) {
                    //Battery status
                    sendAndroidBattery();
                }
            }
        };
        registerReceiver(pData, new IntentFilter(Constants.INTENT_APP_RECEIVE));
    }

    private void sendAndroidBattery() {
        PebbleDictionary dict = new PebbleDictionary();
        dict.addInt8(Global.MSG_PHONE_BATTERY, (byte)Global.getBatteryLevel(getApplicationContext()));
        outgoing.add(dict);
        flushOutgoing();
    }

    private void sendNotifications() {
        int notifId = 0;
        List<StatusBarNotification> sbns = Arrays.asList(getActiveNotifications());
        Collections.reverse(sbns);

        for (StatusBarNotification sbn : sbns) {
            if (notifId == 25) {
                // Can't send that many notifications
                return;
            }
            if (!shouldSendNotification(sbn)) {
                // Skip notification, move to next
                continue;
            }

            // Get title
            final PackageManager pm = getApplicationContext().getPackageManager();
            ApplicationInfo ai;
            try {
                ai = pm.getApplicationInfo(sbn.getPackageName(), 0);
            } catch (final NameNotFoundException e) {
                ai = null;
            }
            final String title = (String) (ai != null ? pm.getApplicationLabel(ai) : "(unknown)");

            // Get details
            String details = "Failed to find additional notification text.";

            if (sbn.getNotification().tickerText != null) {
                details = sbn.getNotification().tickerText.toString();
            } else if (sbn.getNotification().tickerView != null) {
                RemoteViews rv = sbn.getNotification().tickerView;

                try {
                    details = "";
                    for (Field field : rv.getClass().getDeclaredFields()) {
                        field.setAccessible(true);

                        if (field.getName().equals("mActions")) {
                            ArrayList<Object> things = (ArrayList<Object>) field.get(rv);
                            //Log.w(getClass().toString(), things.toString());
                            for (Object object : things) {
                                for (Field innerField : object.getClass().getDeclaredFields()) {
                                    innerField.setAccessible(true);
                                    if (innerField.getName().equals("value")) {
                                        Object innerObj = innerField.get(object);
                                        if (innerObj instanceof String || innerObj instanceof SpannableString) {
                                            if (details.length() > 0) {
                                                details += " - ";
                                            }
                                            try {
                                                details += innerField.get(object).toString();
                                            } catch (Exception e) {
                                                //
                                            }
                                        }
                                    }

                                }
                            }
                        }
                    }
                } catch (Exception e) {
                    details = "Failed to find additional notification text.";
                }
            }

            PebbleDictionary dict = new PebbleDictionary();
            dict.addString(Global.MSG_NOTIFICATION_TITLE, title.substring(0, Math.min(title.length(), 19)));
            dict.addString(Global.MSG_NOTIFICATION_DETAILS, details.substring(0, Math.min(details.length(), 59)));
            sentNotifications.put(notifId, sbn);
            dict.addInt8(Global.MSG_LOAD_NOTIFICATION_ID, (byte)notifId++);
            outgoing.add(dict);

            // Get icon
            int iconId = sbn.getNotification().icon;
            Resources res = null;
            try {
                res = getPackageManager().getResourcesForApplication(sbn.getPackageName());
            } catch (NameNotFoundException e) {
                //
            }
            if (res != null) {
                try {
                    Bitmap icon = BitmapFactory.decodeResource(res, iconId);
                    icon = Bitmap.createScaledBitmap(icon, 48, 48, false);

                    byte[] bitmap = new byte[116];

                    int atByte = 0;
                    int atKey = 0;
                    StringBuilder bin = new StringBuilder();

                    boolean grayscale = true;
                    for (int y = 0; y < 48; y++) {
                        for (int x = 0; x < 48; x++) {
                            int c = icon.getPixel(x, y);
                            if ((c & 0x000000FF) != (c >> 8 & 0x000000FF) || (c >> 8 & 0x000000FF) != (c >> 16 & 0x000000FF)) {
                                grayscale = false;
                            }
                        }
                    }

                    for (int y = 0; y < 48; y++) {
                        for (int x = 0; x < 48; x++) {
                            int c = icon.getPixel(x, y);
                            int averageColor = ((c & 0x000000FF) + (c >> 8 & 0x000000FF) + (c >> 16 & 0x000000FF)) / 3;
                            int opacity = ((c >> 24) & 0x000000FF);

                            // Less than 50% opacity or (if a colour icon)
                            // very light colours are "white"
                            if (opacity < 127 || (!grayscale && averageColor > 225)) {
                                bin.append("0");
                            } else {
                                bin.append("1");
                            }
                            if (bin.length() == 8) {
                                bin.reverse();
                                bitmap[atByte++] = (byte)Integer.parseInt(bin.toString(), 2);
                                bin = new StringBuilder();
                            }
                            if (atByte == 116) {
                                dict = new PebbleDictionary();
                                dict.addBytes(atKey++, bitmap);
                                outgoing.add(dict);
                                atByte = 0;
                                bitmap = new byte[116];
                            }
                        }

                    }
                    dict = new PebbleDictionary();
                    dict.addBytes(atKey, bitmap);
                    outgoing.add(dict);
                } catch (Exception e) {
                    //
                }
            }
        }

        if (notifId == 0) {
            // No notifications
            PebbleDictionary dict = new PebbleDictionary();
            dict.addInt8(Global.MSG_NO_NOTIFICATIONS, (byte)1);
            outgoing.add(dict);
        }
        flushOutgoing();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        unregisterReceiver(pAck);
        unregisterReceiver(pNack);
        unregisterReceiver(pData);
    }

    @Override
    public void onNotificationPosted(StatusBarNotification sbn) {
        if (shouldSendNotification(sbn)) {
            // Start if not running
            PebbleKit.startAppOnPebble(getApplicationContext(), Global.appUUID);

            // If running notify that new notifications exist
            PebbleDictionary dict = new PebbleDictionary();
            dict.addInt8(Global.MSG_NOTIFICATIONS_CHANGED, (byte)0);
            outgoing.add(dict);
            flushOutgoing();
        }
    }

    @Override
    public void onNotificationRemoved(StatusBarNotification sbn) {
        // If running notify that new notifications exist
        PebbleDictionary dict = new PebbleDictionary();
        dict.addInt8(Global.MSG_NOTIFICATIONS_CHANGED, (byte)0);
        outgoing.add(dict);
        flushOutgoing();
    }

    public boolean shouldSendNotification(StatusBarNotification sbn) {
        boolean alwaysNotify = sharedPrefs.getBoolean("pref_always_send_notifications", true);
        boolean sendOngoing = sharedPrefs.getBoolean("pref_send_ongoing", false);

        PowerManager powerManager = (PowerManager)getSystemService(POWER_SERVICE);

        // Only send to Pebble if (send ongoing is true (ie. doesn't matter if notification is or not)
        // OR send ongoing is false but notification is not ongoing),
        // priority is default or higher, has master notifications switch enabled
        // AND [always send notifications is on, OR it's off but the screen is off]
        return (sendOngoing || !sendOngoing && !sbn.isOngoing()) && sbn.getNotification().priority >= Notification.PRIORITY_DEFAULT && sbn.getNotification().icon != 0
                && sharedPrefs.getBoolean("pref_master_switch", true) && (alwaysNotify || (!alwaysNotify && !powerManager.isScreenOn()));
    }
}