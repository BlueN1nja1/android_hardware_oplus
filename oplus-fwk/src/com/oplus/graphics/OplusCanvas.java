package com.oplus.graphics;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;

public class OplusCanvas {
    private final Canvas mCanvas;

    public OplusCanvas(Canvas canvas) {
        mCanvas = canvas;
    }

    public void drawSmoothRoundRect(RectF rect, float rx, float ry, Paint paint, float weight) {
        if (mCanvas != null && rect != null && paint != null) {
            mCanvas.drawRoundRect(rect, rx, ry, paint);
        }
    }
}
