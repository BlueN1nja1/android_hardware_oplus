package com.oplus.wrapper.util;

public class StatsLog {
    public static void write(StatsEvent statsEvent) {
        if (statsEvent != null) {
            android.util.StatsLog.write(statsEvent.getPlatformStatsEvent());
        }
    }
}
