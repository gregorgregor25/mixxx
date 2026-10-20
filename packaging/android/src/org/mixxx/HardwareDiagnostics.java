package org.mixxx;

import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.Locale;
import java.util.Map;

final class HardwareDiagnostics {
    private static final String TAG = "MixxxHardware";

    private HardwareDiagnostics() {
    }

    static void log(Context context) {
        StringBuilder report = new StringBuilder();
        try {
            record(report, "Timestamp: "
                    + new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSSXXX", Locale.US).format(new Date()));
            try {
                record(report, "Android SDK: " + Build.VERSION.SDK_INT);
                PackageManager packageManager = context.getPackageManager();
                record(report, "USB host feature: " + packageManager.hasSystemFeature(PackageManager.FEATURE_USB_HOST));
                record(report, "MIDI feature: " + packageManager.hasSystemFeature(PackageManager.FEATURE_MIDI));
            } catch (RuntimeException e) {
                warn(report, "Unable to read Android feature flags", e);
            }

            logAudioOutputs(context, report);
            logMidiDevices(context, report);
            logUsbDevices(context, report);
        } catch (RuntimeException e) {
            Log.w(TAG, "Hardware diagnostics stopped early", e);
            report.append("Hardware diagnostics stopped early: ").append(e).append('\n');
        } finally {
            writeReport(context, report);
        }
    }

    private static void logAudioOutputs(Context context, StringBuilder report) {
        try {
            AudioManager manager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
            if (manager == null) {
                record(report, "Audio output devices: unavailable (AudioManager missing)");
                return;
            }
            AudioDeviceInfo[] devices = manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
            if (devices == null || devices.length == 0) {
                record(report, "Audio output devices: unknown (none reported)");
                return;
            }
            record(report, "Audio output devices reported: " + devices.length);
            for (AudioDeviceInfo device : devices) {
                try {
                    record(report, "Audio output name=" + device.getProductName()
                            + " id=" + device.getId()
                            + " type=" + device.getType()
                            + " channelCounts=" + valuesOrUnknown(device.getChannelCounts())
                            + " sampleRates=" + valuesOrUnknown(device.getSampleRates()));
                } catch (RuntimeException e) {
                    warn(report, "Unable to inspect an audio output", e);
                }
            }
        } catch (RuntimeException e) {
            warn(report, "Unable to enumerate audio outputs", e);
        }
    }

    private static void logMidiDevices(Context context, StringBuilder report) {
        try {
            MidiManager manager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
            if (manager == null) {
                record(report, "MIDI devices: unavailable (MidiManager missing)");
                return;
            }
            MidiDeviceInfo[] devices = manager.getDevices();
            if (devices == null || devices.length == 0) {
                record(report, "MIDI devices: unknown (none reported)");
                return;
            }
            record(report, "MIDI devices reported: " + devices.length);
            for (MidiDeviceInfo device : devices) {
                try {
                    Bundle properties = device.getProperties();
                    record(report, "MIDI device name=" + properties.getString(MidiDeviceInfo.PROPERTY_NAME)
                            + " manufacturer=" + properties.getString(MidiDeviceInfo.PROPERTY_MANUFACTURER)
                            + " product=" + properties.getString(MidiDeviceInfo.PROPERTY_PRODUCT)
                            + " type=" + device.getType()
                            + " usb=" + (device.getType() == MidiDeviceInfo.TYPE_USB)
                            + " inputPorts=" + device.getInputPortCount()
                            + " outputPorts=" + device.getOutputPortCount());
                } catch (RuntimeException e) {
                    warn(report, "Unable to inspect a MIDI device", e);
                }
            }
        } catch (RuntimeException e) {
            warn(report, "Unable to enumerate MIDI devices", e);
        }
    }

    private static void logUsbDevices(Context context, StringBuilder report) {
        try {
            UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
            if (manager == null) {
                record(report, "USB devices: unavailable (UsbManager missing)");
                return;
            }
            Map<String, UsbDevice> devices = manager.getDeviceList();
            if (devices == null || devices.isEmpty()) {
                record(report, "USB devices: unknown (none reported)");
                return;
            }
            record(report, "USB devices reported: " + devices.size());
            for (UsbDevice device : devices.values()) {
                try {
                    record(report, "USB device vendorId=" + device.getVendorId()
                            + " productId=" + device.getProductId()
                            + " interfaces=" + device.getInterfaceCount());
                    for (int index = 0; index < device.getInterfaceCount(); index++) {
                        UsbInterface usbInterface = device.getInterface(index);
                        record(report, "USB interface=" + index
                                + " class=" + usbInterface.getInterfaceClass()
                                + " subclass=" + usbInterface.getInterfaceSubclass()
                                + " protocol=" + usbInterface.getInterfaceProtocol());
                    }
                } catch (RuntimeException e) {
                    warn(report, "Unable to inspect a USB device", e);
                }
            }
        } catch (RuntimeException e) {
            warn(report, "Unable to enumerate USB devices", e);
        }
    }

    private static void record(StringBuilder report, String message) {
        Log.i(TAG, message);
        report.append(message).append('\n');
    }

    private static void warn(StringBuilder report, String message, RuntimeException e) {
        Log.w(TAG, message, e);
        report.append(message).append(": ").append(e).append('\n');
    }

    private static void writeReport(Context context, StringBuilder report) {
        try {
            File directory = context.getExternalFilesDir(null);
            if (directory == null) {
                Log.w(TAG, "Unable to save hardware report: app external files directory unavailable");
                return;
            }
            File output = new File(directory, "hardware-diagnostics.txt");
            try (FileOutputStream stream = new FileOutputStream(output)) {
                stream.write(report.toString().getBytes(StandardCharsets.UTF_8));
            }
            Log.i(TAG, "Saved hardware report to " + output.getAbsolutePath());
        } catch (IOException | RuntimeException e) {
            Log.w(TAG, "Unable to save hardware report", e);
        }
    }

    private static String valuesOrUnknown(int[] values) {
        return values == null || values.length == 0 ? "unknown" : Arrays.toString(values);
    }
}
