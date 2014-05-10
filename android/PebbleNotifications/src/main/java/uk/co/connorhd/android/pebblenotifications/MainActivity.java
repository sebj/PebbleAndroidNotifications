package uk.co.connorhd.android.pebblenotifications;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Button;
import android.widget.Toast;

import com.getpebble.android.kit.PebbleKit;


public class MainActivity extends Activity {

    static final int NOTIFICATION_PERMISSION = 1;

    static Button permissionButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        permissionButton = (Button)findViewById(R.id.btnAddPermission);
        permissionButton.setEnabled(!notificationAccess());
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == NOTIFICATION_PERMISSION) {
            // Update button state to reflect user actions (or inactions)
            permissionButton.setEnabled(!notificationAccess());
        }
    }

    public void buttonClicked(View v) {
        if (v.getId() == R.id.btnInstallApp) {
            // Install Pebble App..
            if (PebbleKit.isWatchConnected(getApplicationContext())) {
                // ..if watch connected..
                if (PebbleKit.getWatchFWVersion(getApplicationContext()).getMajor() >= 2) {
                    // ..and firmware 2.0+
                    Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://connorhd.co.uk/pebble/notifications/watchapp.pbw"));
                    startActivity(intent);
                } else {
                    Toast.makeText(getApplicationContext(), "Firmware 2.0+ required", Toast.LENGTH_LONG).show();
                }
            } else {
                Toast.makeText(getApplicationContext(), "Please connect Pebble", Toast.LENGTH_LONG).show();
            }

        } else if (v.getId() == R.id.btnAddPermission) {
            // Add security permission
            if (!notificationAccess()) {
                Intent intent = new Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS");
                startActivityForResult(intent, NOTIFICATION_PERMISSION);
            }

        } else if (v.getId() == R.id.btnCreateNotify) {
            // Send test notification
            NotificationManager nManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            Notification.Builder ncomp = new Notification.Builder(this);
            ncomp.setContentText("If you're reading this on your Pebble, you're good to go!");
            ncomp.setSmallIcon(R.drawable.ic_launcher);
            ncomp.setAutoCancel(true);
            nManager.notify((int)System.currentTimeMillis(), ncomp.build());

        }
    }

    // Based on http://stackoverflow.com/a/21392852
    // Returns true or false based on whether app has access to notifications
    public boolean notificationAccess() {
        String enabledNotificationListeners = Settings.Secure.getString(getContentResolver(), "enabled_notification_listeners");
        String packageName = getPackageName();

        // Check to see if the enabledNotificationListeners String exists and contains our package name
        return !(enabledNotificationListeners == null || !enabledNotificationListeners.contains(packageName));
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.main, menu);
        return false;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.settings) {

            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
