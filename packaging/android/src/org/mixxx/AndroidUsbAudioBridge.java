package org.mixxx;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Looper;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public final class AndroidUsbAudioBridge implements AutoCloseable {
    private static final int VENDOR_ID = 0x2b73;
    private static final int PRODUCT_ID = 0x0038;
    private static final int PERMISSION_TIMEOUT_SECONDS = 15;
    private static final String PERMISSION_ACTION =
            "org.mixxx.USB_AUDIO_PERMISSION.";
    private static final AtomicInteger NEXT_REQUEST = new AtomicInteger();

    private static Context applicationContext;
    private static volatile String error = "";

    private UsbDeviceConnection connection;
    private final byte[] descriptors;

    private AndroidUsbAudioBridge(UsbDeviceConnection connection, byte[] descriptors) {
        this.connection = connection;
        this.descriptors = descriptors.clone();
    }

    public static synchronized void initialize(Context context) {
        applicationContext = context == null ? null : context.getApplicationContext();
        error = applicationContext == null ? "Android application context unavailable" : "";
    }

    public static String lastError() {
        return error;
    }

    public static synchronized AndroidUsbAudioBridge openFlx6() {
        error = "";
        try {
            return openFlx6Internal();
        } catch (RuntimeException exception) {
            return fail("Android USB audio bridge failed: " + exception);
        }
    }

    private static AndroidUsbAudioBridge openFlx6Internal() {
        Context context = applicationContext;
        if (context == null) {
            return fail("Android USB audio bridge was not initialized");
        }
        UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        if (manager == null) {
            return fail("Android USB host service unavailable");
        }
        UsbDevice device = null;
        for (UsbDevice candidate : manager.getDeviceList().values()) {
            if (candidate.getVendorId() == VENDOR_ID &&
                    candidate.getProductId() == PRODUCT_ID) {
                if (device != null) {
                    return fail("Multiple DDJ-FLX6 USB devices found");
                }
                device = candidate;
            }
        }
        if (device == null) {
            return fail("DDJ-FLX6 (2b73:0038) is not connected");
        }
        if (!manager.hasPermission(device) &&
                !awaitPermission(context, manager, device)) {
            return null;
        }
        UsbDevice current = manager.getDeviceList().get(device.getDeviceName());
        if (current == null || current.getVendorId() != VENDOR_ID ||
                current.getProductId() != PRODUCT_ID || !manager.hasPermission(current)) {
            return fail("DDJ-FLX6 disconnected or USB permission was lost");
        }
        UsbDeviceConnection opened = null;
        try {
            opened = manager.openDevice(current);
            if (opened == null) {
                return fail("Android could not open DDJ-FLX6 USB device");
            }
            byte[] raw = opened.getRawDescriptors();
            if (raw == null || raw.length == 0 || opened.getFileDescriptor() < 0) {
                return fail("DDJ-FLX6 USB descriptor or file descriptor unavailable");
            }
            AndroidUsbAudioBridge bridge = new AndroidUsbAudioBridge(opened, raw);
            opened = null;
            return bridge;
        } catch (RuntimeException exception) {
            return fail("DDJ-FLX6 USB open failed: " + exception);
        } finally {
            if (opened != null) {
                opened.close();
            }
        }
    }

    private static boolean awaitPermission(
            Context context, UsbManager manager, UsbDevice device) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            error = "USB permission requires a non-UI sound-device thread";
            return false;
        }
        int request = NEXT_REQUEST.incrementAndGet();
        String action = PERMISSION_ACTION + request;
        CountDownLatch completion = new CountDownLatch(1);
        final boolean[] granted = {false};
        BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context receivedContext, Intent intent) {
                if (!action.equals(intent.getAction())) {
                    return;
                }
                UsbDevice result = intent.getParcelableExtra(
                        UsbManager.EXTRA_DEVICE, UsbDevice.class);
                if (result != null && device.getDeviceName().equals(result.getDeviceName())) {
                    granted[0] = intent.getBooleanExtra(
                            UsbManager.EXTRA_PERMISSION_GRANTED, false);
                    completion.countDown();
                }
            }
        };
        PendingIntent permission = null;
        boolean registered = false;
        try {
            context.registerReceiver(receiver, new IntentFilter(action),
                    Context.RECEIVER_NOT_EXPORTED);
            registered = true;
            Intent requestIntent = new Intent(action).setPackage(context.getPackageName());
            permission = PendingIntent.getBroadcast(context, request, requestIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_MUTABLE);
            manager.requestPermission(device, permission);
            boolean received = completion.await(PERMISSION_TIMEOUT_SECONDS, TimeUnit.SECONDS);
            if (!received && !manager.hasPermission(device)) {
                error = "DDJ-FLX6 USB permission request timed out";
                return false;
            }
            if (!granted[0] && !manager.hasPermission(device)) {
                error = "DDJ-FLX6 USB permission denied";
                return false;
            }
            return true;
        } catch (InterruptedException exception) {
            Thread.currentThread().interrupt();
            error = "DDJ-FLX6 USB permission wait interrupted";
            return false;
        } catch (RuntimeException exception) {
            error = "DDJ-FLX6 USB permission request failed: " + exception;
            return false;
        } finally {
            if (registered) {
                try {
                    context.unregisterReceiver(receiver);
                } catch (RuntimeException ignored) {
                }
            }
            if (permission != null) {
                try {
                    permission.cancel();
                } catch (RuntimeException ignored) {
                }
            }
        }
    }

    private static AndroidUsbAudioBridge fail(String reason) {
        error = reason;
        return null;
    }

    public synchronized int getFileDescriptor() {
        return connection == null ? -1 : connection.getFileDescriptor();
    }

    public synchronized byte[] getRawDescriptors() {
        return connection == null ? null : descriptors.clone();
    }

    @Override
    public synchronized void close() {
        if (connection != null) {
            connection.close();
            connection = null;
        }
    }
}
