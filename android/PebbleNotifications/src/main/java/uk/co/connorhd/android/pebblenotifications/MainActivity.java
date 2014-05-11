package uk.co.connorhd.android.pebblenotifications;

import android.app.Activity;
import android.app.FragmentTransaction;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;


public class MainActivity extends Activity {
    static SharedPreferences sharedPrefs;

    boolean showingSetup;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        sharedPrefs = PreferenceManager.getDefaultSharedPreferences(this);

        if (savedInstanceState == null) {
            if (!sharedPrefs.getBoolean("setup", false)) {
                getFragmentManager().beginTransaction().add(R.id.container, new SetupFragment()).commit();
                showingSetup = true;

            } else {
                getFragmentManager().beginTransaction().add(R.id.container, new PrefsFragment()).commit();
                showingSetup = false;
            }
        }
    }

    public void sendTestNotification() {
        NotificationManager nManager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
        Notification.Builder ncomp = new Notification.Builder(this);
        ncomp.setContentTitle("Test notification");
        ncomp.setContentText("Hello, world! :)");
        ncomp.setSmallIcon(R.drawable.ic_launcher);
        ncomp.setAutoCancel(true);
        nManager.notify((int)System.currentTimeMillis(), ncomp.build());
    }

    public void finishedSetup() {
        final FragmentTransaction ft = getFragmentManager().beginTransaction();
        ft.setCustomAnimations(0, R.animator.slide_down);
        ft.remove(getFragmentManager().findFragmentByTag("setup"));
        ft.addToBackStack(null);
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
            case R.id.setup:
                if (!showingSetup) {
                    final FragmentTransaction ft = getFragmentManager().beginTransaction();
                    ft.setCustomAnimations(R.animator.slide_up, 0);
                    ft.add(R.id.container, new SetupFragment(), "setup");
                    ft.addToBackStack(null);
                    ft.commit();

                    showingSetup = true;
                }

                break;

            case R.id.sendTestNotification:
                sendTestNotification();
                break;
        }

        return super.onOptionsItemSelected(item);
    }
}
