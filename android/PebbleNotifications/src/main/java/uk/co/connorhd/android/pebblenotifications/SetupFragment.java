package uk.co.connorhd.android.pebblenotifications;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.app.Fragment;
import android.preference.PreferenceManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Toast;

import com.getpebble.android.kit.PebbleKit;

public class SetupFragment extends Fragment implements View.OnClickListener {

    static final int INSTALL_PEBBLE_APP = 1;
    static final int NOTIFICATION_PERMISSION = 2;

    SharedPreferences sharedPrefs;
    Button permissionButton;

    boolean installedApp = false;

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
        permissionButton.setEnabled(!Global.notificationAccess(getActivity()));

        mView.findViewById(R.id.btnInstallApp).setOnClickListener(this);
        permissionButton.setOnClickListener(this);
        mView.findViewById(R.id.btnCreateNotify).setOnClickListener(this);
        mView.findViewById(R.id.btnFinish).setOnClickListener(this);

        return mView;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        //Perhaps not the best method

        MainActivity activity = (MainActivity)getActivity();
        if (activity != null) {
            activity.showingSetup = false;
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == INSTALL_PEBBLE_APP) {
            if (PebbleKit.isWatchConnected(getActivity().getApplicationContext()) && PebbleKit.getWatchFWVersion(getActivity().getApplicationContext()).getMajor() >= 2) {
                //Assume app installed
                installedApp = true;
            }
        } else if (requestCode == NOTIFICATION_PERMISSION) {
            // Update button state to reflect user actions (or in-actions)
            permissionButton.setEnabled(!Global.notificationAccess(getActivity()));

            if (Global.notificationAccess(getActivity()) && installedApp) {
                SharedPreferences.Editor editor = sharedPrefs.edit();
                editor.putBoolean("setup", true);
                editor.apply();
            }
        }
    }

    public void onClick(View v) {
        switch(v.getId()) {
            case R.id.btnInstallApp:
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
                break;

            case R.id.btnAddPermission:
                // Add security permission
                if (!Global.notificationAccess(getActivity())) {
                    Intent intent = new Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS");
                    startActivityForResult(intent, NOTIFICATION_PERMISSION);
                }
                break;

            case R.id.btnCreateNotify:
                // Send test notification
                ((MainActivity)getActivity()).sendTestNotification();
                break;

            case R.id.btnFinish:
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
                                    editor.apply();

                                    ((MainActivity)getActivity()).finishedSetup();
                                }
                            })
                            .setNegativeButton(android.R.string.no, new DialogInterface.OnClickListener() {
                                public void onClick(DialogInterface dialog, int which) {
                                    // Do nothing - stay on current page and allow user to install Pebble app
                                }
                            })
                            .setIconAttribute(android.R.attr.alertDialogIcon)
                            .show();
                }
        }
    }
}
