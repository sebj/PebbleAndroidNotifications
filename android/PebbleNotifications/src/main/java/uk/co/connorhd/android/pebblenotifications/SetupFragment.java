package uk.co.connorhd.android.pebblenotifications;


import android.app.AlertDialog;
import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.app.Fragment;
import android.preference.PreferenceManager;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import com.getpebble.android.kit.PebbleKit;


/**
 * A simple {@link Fragment} subclass.
 * Use the {@link SetupFragment#newInstance} factory method to
 * create an instance of this fragment.
 *
 */
public class SetupFragment extends Fragment implements View.OnClickListener {

    static final int INSTALL_PEBBLE_APP = 1;
    static final int NOTIFICATION_PERMISSION = 2;

    static SharedPreferences sharedPrefs;
    static Button permissionButton;

    static boolean installedApp = false;


    public static SetupFragment newInstance() {
        return new SetupFragment();
    }
    public SetupFragment() {
        // Required empty public constructor
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        sharedPrefs = PreferenceManager.getDefaultSharedPreferences(getActivity());
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        View mView = inflater.inflate(R.layout.fragment_setup, container, false);
        mView.setBackgroundColor(Color.WHITE);

        permissionButton = (Button)mView.findViewById(R.id.btnAddPermission);
        permissionButton.setEnabled(!notificationAccess());

        mView.findViewById(R.id.btnInstallApp).setOnClickListener(this);
        permissionButton.setOnClickListener(this);
        mView.findViewById(R.id.btnCreateNotify).setOnClickListener(this);
        mView.findViewById(R.id.btnFinish).setOnClickListener(this);

        return mView;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == INSTALL_PEBBLE_APP) {
            if (PebbleKit.isWatchConnected(getActivity().getApplicationContext()) && PebbleKit.getWatchFWVersion(getActivity().getApplicationContext()).getMajor() >= 2) {
                //Assume app installed
                installedApp = true;
            }
        } else if (requestCode == NOTIFICATION_PERMISSION) {
            // Update button state to reflect user actions (or inactions)
            permissionButton.setEnabled(!notificationAccess());

            if (notificationAccess() && installedApp) {
                SharedPreferences.Editor editor = sharedPrefs.edit();
                editor.putBoolean("setup", true);
                editor.commit();
            }
        }
    }

    public void onClick(View v) {
        if (v.getId() == R.id.btnInstallApp) {
            // Install Pebble App..
            if (PebbleKit.isWatchConnected(getActivity().getApplicationContext())) {
                // ..if watch connected..
                if (PebbleKit.getWatchFWVersion(getActivity().getApplicationContext()).getMajor() >= 2) {
                    // ..and firmware 2.0+
                    Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://connorhd.co.uk/pebble/notifications/watchapp.pbw"));
                    startActivityForResult(intent, INSTALL_PEBBLE_APP);
                } else {
                    Toast.makeText(getActivity().getApplicationContext(), "Firmware 2.0+ required", Toast.LENGTH_LONG).show();
                }
            } else {
                Toast.makeText(getActivity().getApplicationContext(), "Please connect Pebble", Toast.LENGTH_LONG).show();
            }

        } else if (v.getId() == R.id.btnAddPermission) {
            // Add security permission
            if (!notificationAccess()) {
                Intent intent = new Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS");
                startActivityForResult(intent, NOTIFICATION_PERMISSION);
            }

        } else if (v.getId() == R.id.btnCreateNotify) {
            // Send test notification
            ((MainActivity) getActivity()).sendTestNotification();

        } else if (v.getId() == R.id.btnFinish) {
            // Done!
            if (sharedPrefs.getBoolean("setup", false)) {
                ((MainActivity)getActivity()).finishedSetup();
            } else {
                new AlertDialog.Builder(getActivity())
                        .setTitle("Pebble app not installed")
                        .setMessage("Are you sure you want to continue anyway?")
                        .setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int which) {

                                SharedPreferences.Editor editor = sharedPrefs.edit();
                                editor.putBoolean("setup", true);
                                editor.commit();

                                ((MainActivity) getActivity()).finishedSetup();
                            }
                        })
                        .setNegativeButton(android.R.string.no, new DialogInterface.OnClickListener() {
                            public void onClick(DialogInterface dialog, int which) {
                                // Do nothing
                            }
                        })
                        .setIconAttribute(android.R.attr.alertDialogIcon)
                        .show();
            }
        }
    }

    // Based on http://stackoverflow.com/a/21392852
    // Returns true or false based on whether app has access to notifications
    public boolean notificationAccess() {
        String enabledNotificationListeners = Settings.Secure.getString(getActivity().getContentResolver(), "enabled_notification_listeners");
        String packageName = getActivity().getPackageName();

        // Check to see if the enabledNotificationListeners String exists and contains our package name
        return !(enabledNotificationListeners == null || !enabledNotificationListeners.contains(packageName));
    }
}
