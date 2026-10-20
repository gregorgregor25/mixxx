package org.mixxx;

import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.WindowManager;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import org.qtproject.qt.android.QtActivityBase;
import java.io.File;
import java.io.IOException;

public class MainActivity extends QtActivityBase {
    private static final String PREFS_NAME = "mixxxLaunch";
    private static final String SETTINGS_PATH_KEY = "settingsPath";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        applyRememberedSettingsPath();
        AndroidUsbAudioBridge.initialize(getApplicationContext());
        super.onCreate(savedInstanceState);
        HardwareDiagnostics.log(this);

        // Disable drawing over cutout - isn't working
        WindowManager.LayoutParams lp = this.getWindow().getAttributes();
        lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;

        // Disable system and navigation bar to prevent accidental back or app switch
        WindowInsetsControllerCompat windowInsetsController =
            WindowCompat.getInsetsController(getWindow(), getWindow().getDecorView());
        windowInsetsController.setSystemBarsBehavior(
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        windowInsetsController.hide(WindowInsetsCompat.Type.navigationBars());
    }

    private void applyRememberedSettingsPath() {
        Intent intent = getIntent();
        if (intent == null) {
            return;
        }
        try {
            SharedPreferences preferences = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
            String requested = intent.getStringExtra("mixxxSettingsPath");
            if (requested != null) {
                String validRequested = validSettingsPath(requested);
                if (validRequested != null) {
                    preferences.edit().putString(SETTINGS_PATH_KEY, validRequested).commit();
                }
            }
            String arguments = intent.getStringExtra("applicationArguments");
            if (arguments != null && !arguments.isEmpty()) {
                return;
            }
            String stored = preferences.getString(SETTINGS_PATH_KEY, null);
            if (stored != null) {
                String validStored = validSettingsPath(stored);
                if (validStored != null) {
                    intent.putExtra("applicationArguments", "--settings-path=" + validStored);
                } else {
                    preferences.edit().remove(SETTINGS_PATH_KEY).commit();
                }
            }
        } catch (RuntimeException ignored) {
        }
    }

    private String validSettingsPath(String requested) {
        if (requested.isEmpty()) {
            return null;
        }
        File externalFiles = getExternalFilesDir(null);
        if (externalFiles == null) {
            return null;
        }
        try {
            File root = externalFiles.getCanonicalFile();
            File directory = new File(requested).getCanonicalFile();
            String prefix = root.getPath() + File.separator;
            if (directory.isDirectory() && directory.getPath().startsWith(prefix)) {
                return directory.getPath();
            }
        } catch (IOException | SecurityException ignored) {
        }
        return null;
    }
}
