package com.termux.x11.input;

import org.junit.Before;
import org.junit.Test;
import static org.junit.Assert.*;

public class PalmRejectionFilterTest {
    private PalmRejectionFilter filter;
    private static final int TOOL_FINGER = 1; // MotionEvent.TOOL_TYPE_FINGER
    private static final int TOOL_STYLUS = 2; // MotionEvent.TOOL_TYPE_STYLUS
    private static final int TOOL_ERASER = 4; // MotionEvent.TOOL_TYPE_ERASER
    private static final int CLASSIFICATION_AMBIGUOUS_GESTURE = 1;
    private static final int ACTION_DOWN = 0;
    private static final int ACTION_MOVE = 2;
    private static final int ACTION_UP = 1;

    @Before
    public void setUp() {
        filter = new PalmRejectionFilter();
        filter.setMode(PalmRejectionFilter.MODE_STYLUS_AND_AREA);
        filter.setStylusHysteresisMs(400);
        filter.setPalmTouchMajorThreshold(100.0f);
        filter.setPalmSizeThreshold(0.30f);
    }

    @Test
    public void testNormalFingertipAccepted() {
        long t = 1000;
        // Normal fingertip touch: small area (major=45px, minor=40px, size=0.10)
        boolean reject = filter.shouldRejectPointer(
                t, TOOL_FINGER, ACTION_DOWN,
                500f, 500f, 45f, 40f, 0.10f, 0, 1920f, 1080f
        );
        assertFalse("Normal fingertip touch should be accepted", reject);
    }

    @Test
    public void testStylusNeverRejected() {
        long t = 1000;
        boolean reject = filter.shouldRejectPointer(
                t, TOOL_STYLUS, ACTION_DOWN,
                500f, 500f, 5f, 5f, 0.05f, 0, 1920f, 1080f
        );
        assertFalse("Stylus should never be rejected", reject);
    }

    @Test
    public void testEraserNeverRejected() {
        long t = 1000;
        boolean reject = filter.shouldRejectPointer(
                t, TOOL_ERASER, ACTION_DOWN,
                500f, 500f, 15f, 15f, 0.05f, 0, 1920f, 1080f
        );
        assertFalse("Eraser should never be rejected", reject);
    }

    @Test
    public void testNativePalmFlagRejected() {
        long t = 1000;
        boolean reject = filter.shouldRejectPointer(
                t, TOOL_FINGER, ACTION_DOWN,
                500f, 500f, 50f, 50f, 0.10f, CLASSIFICATION_AMBIGUOUS_GESTURE, 1920f, 1080f
        );
        assertTrue("Native palm flag should be rejected", reject);
    }

    @Test
    public void testLargeContactAreaRejected() {
        long t = 1000;
        // Large resting palm (touchMajor = 150px > 100px)
        boolean reject = filter.shouldRejectPointer(
                t, TOOL_FINGER, ACTION_DOWN,
                500f, 500f, 150f, 80f, 0.15f, 0, 1920f, 1080f
        );
        assertTrue("Touch with large contact major axis should be rejected", reject);

        // Large normalized size (size = 0.45 > 0.30)
        boolean rejectSize = filter.shouldRejectPointer(
                t, TOOL_FINGER, ACTION_DOWN,
                500f, 500f, 80f, 70f, 0.45f, 0, 1920f, 1080f
        );
        assertTrue("Touch with large normalized size should be rejected", rejectSize);
    }

    @Test
    public void testAngledPalmEccentricityRejected() {
        long t = 1000;
        // Angled palm heel: major=90, minor=30 -> ratio 3.0 >= 2.0
        boolean reject = filter.shouldRejectPointer(
                t, TOOL_FINGER, ACTION_DOWN,
                500f, 500f, 90f, 30f, 0.15f, 0, 1920f, 1080f
        );
        assertTrue("Angled palm with high eccentricity should be rejected", reject);
    }

    @Test
    public void testStylusPriorityActiveSuppression() {
        long t = 1000;
        // Pen touches down
        filter.notifyStylusEvent(t, true);

        // Concurrent finger touch while pen is touching
        boolean reject = filter.shouldRejectPointer(
                t + 50, TOOL_FINGER, ACTION_DOWN,
                500f, 500f, 40f, 40f, 0.08f, 0, 1920f, 1080f
        );
        assertTrue("Finger touch while stylus is down must be rejected", reject);
    }

    @Test
    public void testStylusHysteresisGraceWindow() {
        long t = 1000;
        // Pen was down and now lifts at t=1000
        filter.notifyStylusEvent(t, false);

        // Palm shifts 150ms after pen lift (air interval during writing)
        boolean rejectWithinWindow = filter.shouldRejectPointer(
                t + 150, TOOL_FINGER, ACTION_DOWN,
                500f, 500f, 40f, 40f, 0.08f, 0, 1920f, 1080f
        );
        assertTrue("Palm touch within 400ms grace window must be rejected", rejectWithinWindow);

        // Intentional finger tap 500ms after pen lift (> 400ms window)
        boolean acceptAfterWindow = filter.shouldRejectPointer(
                t + 500, TOOL_FINGER, ACTION_DOWN,
                500f, 500f, 40f, 40f, 0.08f, 0, 1920f, 1080f
        );
        assertFalse("Finger tap after hysteresis window should be accepted", acceptAfterWindow);
    }

    @Test
    public void testRetroactiveFingerCancel() {
        long t = 1000;
        filter.notifyFingerTouch(true);
        assertFalse(filter.checkAndClearNeedFingerCancel());

        // Stylus touches down 20ms later
        filter.notifyStylusEvent(t + 20, true);
        assertTrue("Must flag retroactive cancellation when pen lands after finger",
                filter.checkAndClearNeedFingerCancel());
    }

    @Test
    public void testPenOnlyMode() {
        filter.setMode(PalmRejectionFilter.MODE_PEN_ONLY);
        long t = 1000;

        // Any finger touch is blocked
        boolean rejectFinger = filter.shouldRejectPointer(
                t, TOOL_FINGER, ACTION_DOWN,
                500f, 500f, 30f, 30f, 0.05f, 0, 1920f, 1080f
        );
        assertTrue("Pen-only mode must reject finger touch", rejectFinger);

        // Stylus is permitted
        boolean rejectStylus = filter.shouldRejectPointer(
                t, TOOL_STYLUS, ACTION_DOWN,
                500f, 500f, 5f, 5f, 0.05f, 0, 1920f, 1080f
        );
        assertFalse("Pen-only mode must permit stylus", rejectStylus);
    }
}
