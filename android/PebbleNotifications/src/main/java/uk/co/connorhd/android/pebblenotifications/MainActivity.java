package uk.co.connorhd.android.pebblenotifications;

import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentTransaction;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.Toast;

import com.getpebble.android.kit.PebbleKit;

public class MainActivity extends Activity {

    public boolean showingSetup;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        SharedPreferences sharedPrefs = PreferenceManager.getDefaultSharedPreferences(this);

        if (savedInstanceState == null) {
            if (!sharedPrefs.getBoolean("setup", false)) {
                getFragmentManager().beginTransaction().add(R.id.container, new SetupFragment()).commit();
                showingSetup = true;

            } else {
                getFragmentManager().beginTransaction().add(R.id.container, new PrefsFragment()).commit();
                showingSetup = false;

                if (!Global.notificationAccess(this)) {
                    Toast.makeText(this, "No notification access. Please setup.", Toast.LENGTH_LONG).show();
                }
            }
        }
    }

    public void sendTestNotification() {
        NotificationManager nManager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
        Notification.Builder ncomp =
                new Notification.Builder(this)
                .setSmallIcon(R.drawable.ic_launcher)
                .setContentTitle("Test notification")
                .setContentText("Hello, world! :)")
                .setAutoCancel(true);

        nManager.notify((int)System.currentTimeMillis(), ncomp.build());
    }

    public void finishedSetup() {
        final FragmentTransaction ft = getFragmentManager().beginTransaction();
        ft.setCustomAnimations(0, R.animator.slide_down);

        Fragment setupFragment = getFragmentManager().findFragmentByTag("setup-menu");

        if (setupFragment != null) {
            // If setup added via menu button, remove it
            ft.remove(setupFragment);
        } else {
            // If added normally at start, just replace it with a new preference fragment
            ft.replace(R.id.container, new PrefsFragment());
        }

        ft.commit();

        showingSetup = false;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch(item.getItemId()) {

            case R.id.openApp:
                PebbleKit.startAppOnPebble(getApplicationContext(), Global.appUUID);
                break;

            case R.id.sendTestNotification:
                sendTestNotification();
                break;

            case R.id.setup:
                if (!showingSetup) {
                    final FragmentTransaction ft = getFragmentManager().beginTransaction();
                    ft.setCustomAnimations(R.animator.slide_up, 0);
                    ft.add(R.id.container, new SetupFragment(), "setup-menu");
                    ft.addToBackStack(null);
                    ft.commit();

                    showingSetup = true;
                }

                break;
        }

        return super.onOptionsItemSelected(item);
    }
}
