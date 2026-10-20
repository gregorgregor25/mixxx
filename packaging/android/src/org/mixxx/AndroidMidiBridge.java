package org.mixxx;

import android.content.Context;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

import java.io.IOException;
import java.util.ArrayDeque;
import java.util.Arrays;

public final class AndroidMidiBridge {
    private static final String TAG = "MixxxAndroidMidi";
    private static final int MAX_PACKET_BYTES = 16384;
    private static final int MAX_QUEUED_BYTES = 131072;
    private static final int MAX_QUEUED_PACKETS = 512;

    public static final class Packet {
        private final byte[] data;
        private final long timestampNanos;
        private final long epoch;

        Packet(byte[] data, long timestampNanos, long epoch) {
            this.data = data;
            this.timestampNanos = timestampNanos;
            this.epoch = epoch;
        }

        public byte[] getData() {
            return data;
        }

        public long getTimestampNanos() {
            return timestampNanos;
        }

        public long getEpoch() {
            return epoch;
        }
    }

    public static MidiDeviceInfo[] enumerate(Context context) {
        MidiManager manager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
        if (manager == null) {
            return new MidiDeviceInfo[0];
        }
        return manager.getDevices();
    }

    public static String property(MidiDeviceInfo info, String key) {
        String value = info.getProperties().getString(key);
        return value == null ? "" : value;
    }

    public static String portName(MidiDeviceInfo info, int type, int number) {
        for (MidiDeviceInfo.PortInfo port : info.getPorts()) {
            if (port.getType() == type && port.getPortNumber() == number) {
                String name = port.getName();
                return name == null ? "" : name;
            }
        }
        return "";
    }

    private final MidiManager manager;
    private final MidiDeviceInfo info;
    private final int inputPortNumber;
    private final int outputPortNumber;
    private final Handler callbackHandler = new Handler(Looper.getMainLooper());
    private final Object lock = new Object();
    private final ArrayDeque<Packet> packets = new ArrayDeque<>();
    private int queuedBytes;
    private long droppedPackets;
    private long epoch;
    private int generation;
    private boolean opening;
    private boolean open;
    private boolean removed;
    private boolean callbackRegistered;
    private MidiDevice device;
    private MidiInputPort inputPort;
    private MidiOutputPort outputPort;

    private final MidiManager.DeviceCallback deviceCallback = new MidiManager.DeviceCallback() {
        @Override
        public void onDeviceRemoved(MidiDeviceInfo removedInfo) {
            if (removedInfo.getId() == info.getId()) {
                synchronized (lock) {
                    removed = true;
                }
                close();
            }
        }
    };

    public AndroidMidiBridge(Context context, MidiDeviceInfo info,
            int inputPortNumber, int outputPortNumber) {
        manager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
        this.info = info;
        this.inputPortNumber = inputPortNumber;
        this.outputPortNumber = outputPortNumber;
    }

    public boolean open(int timeoutMillis) {
        if (manager == null) {
            return false;
        }
        final int requestGeneration;
        synchronized (lock) {
            if (open) {
                return true;
            }
            if (opening || removed) {
                return false;
            }
            opening = true;
            requestGeneration = ++generation;
            packets.clear();
            queuedBytes = 0;
            epoch++;
        }
        try {
            synchronized (lock) {
                if (!opening || generation != requestGeneration) {
                    return false;
                }
                manager.registerDeviceCallback(deviceCallback, callbackHandler);
                callbackRegistered = true;
            }
            manager.openDevice(info, openedDevice -> finishOpen(openedDevice,
                    requestGeneration), callbackHandler);
        } catch (RuntimeException error) {
            Log.w(TAG, "Unable to request MIDI device", error);
            close();
            return false;
        }

        long deadline = SystemClock.elapsedRealtime() + timeoutMillis;
        synchronized (lock) {
            while (opening) {
                long remaining = deadline - SystemClock.elapsedRealtime();
                if (remaining <= 0) {
                    break;
                }
                try {
                    lock.wait(remaining);
                } catch (InterruptedException error) {
                    Thread.currentThread().interrupt();
                    break;
                }
            }
            if (open) {
                return true;
            }
        }
        close();
        return false;
    }

    private void finishOpen(MidiDevice openedDevice, int requestGeneration) {
        if (openedDevice == null) {
            synchronized (lock) {
                if (generation == requestGeneration) {
                    opening = false;
                    lock.notifyAll();
                }
            }
            return;
        }

        MidiInputPort newInput = null;
        MidiOutputPort newOutput = null;
        try {
            if (inputPortNumber >= 0) {
                newInput = openedDevice.openInputPort(inputPortNumber);
                if (newInput == null) {
                    throw new IOException("MIDI input port unavailable");
                }
            }
            if (outputPortNumber >= 0) {
                newOutput = openedDevice.openOutputPort(outputPortNumber);
                if (newOutput == null) {
                    throw new IOException("MIDI output port unavailable");
                }
                final int receiverGeneration = requestGeneration;
                newOutput.connect(new MidiReceiver() {
                    @Override
                    public void onSend(byte[] data, int offset, int count,
                            long timestamp) {
                        enqueue(data, offset, count, timestamp, receiverGeneration);
                    }
                });
            }
        } catch (IOException | RuntimeException error) {
            Log.w(TAG, "Unable to open MIDI port", error);
            closeQuietly(newOutput);
            closeQuietly(newInput);
            closeQuietly(openedDevice);
            synchronized (lock) {
                if (generation == requestGeneration) {
                    opening = false;
                    lock.notifyAll();
                }
            }
            return;
        }

        boolean accepted;
        synchronized (lock) {
            accepted = opening && !removed && generation == requestGeneration;
            if (accepted) {
                device = openedDevice;
                inputPort = newInput;
                outputPort = newOutput;
                open = true;
                opening = false;
                lock.notifyAll();
            }
        }
        if (!accepted) {
            closeQuietly(newOutput);
            closeQuietly(newInput);
            closeQuietly(openedDevice);
        }
    }

    private void enqueue(byte[] data, int offset, int count, long timestamp,
            int receiverGeneration) {
        if (count <= 0) {
            return;
        }
        synchronized (lock) {
            if (generation != receiverGeneration || (!open && !opening)) {
                return;
            }
            if (count > MAX_PACKET_BYTES) {
                droppedPackets++;
                packets.clear();
                queuedBytes = 0;
                epoch++;
                return;
            }
            if (packets.size() >= MAX_QUEUED_PACKETS ||
                    queuedBytes + count > MAX_QUEUED_BYTES) {
                droppedPackets += packets.size();
                packets.clear();
                queuedBytes = 0;
                epoch++;
            }
            packets.addLast(new Packet(Arrays.copyOfRange(data, offset, offset + count),
                    timestamp, epoch));
            queuedBytes += count;
        }
    }

    public Packet pollPacket() {
        synchronized (lock) {
            Packet packet = packets.pollFirst();
            if (packet != null) {
                queuedBytes -= packet.data.length;
            }
            return packet;
        }
    }

    public long getDroppedPacketCount() {
        synchronized (lock) {
            return droppedPackets;
        }
    }

    public boolean isOpen() {
        synchronized (lock) {
            return open;
        }
    }

    public boolean send(byte[] data) {
        MidiInputPort port;
        synchronized (lock) {
            port = open ? inputPort : null;
        }
        if (port == null) {
            return false;
        }
        try {
            port.send(data, 0, data.length);
            return true;
        } catch (IOException | RuntimeException error) {
            Log.w(TAG, "Unable to send MIDI data", error);
            close();
            return false;
        }
    }

    public void close() {
        MidiInputPort oldInput;
        MidiOutputPort oldOutput;
        MidiDevice oldDevice;
        synchronized (lock) {
            generation++;
            opening = false;
            open = false;
            oldInput = inputPort;
            oldOutput = outputPort;
            oldDevice = device;
            inputPort = null;
            outputPort = null;
            device = null;
            packets.clear();
            queuedBytes = 0;
            epoch++;
            if (callbackRegistered) {
                callbackRegistered = false;
                try {
                    manager.unregisterDeviceCallback(deviceCallback);
                } catch (RuntimeException error) {
                    Log.w(TAG, "Unable to unregister MIDI device callback", error);
                }
            }
            lock.notifyAll();
        }
        closeQuietly(oldOutput);
        closeQuietly(oldInput);
        closeQuietly(oldDevice);
    }

    private static void closeQuietly(AutoCloseable value) {
        if (value != null) {
            try {
                value.close();
            } catch (Exception ignored) {
            }
        }
    }
}
