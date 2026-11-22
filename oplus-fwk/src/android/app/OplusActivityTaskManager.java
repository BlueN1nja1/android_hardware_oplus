package android.app;

import android.content.ComponentName;

import java.util.Collections;
import java.util.List;

public class OplusActivityTaskManager {
    private static final OplusActivityTaskManager sInstance = new OplusActivityTaskManager();

    public static OplusActivityTaskManager getInstance() {
        return sInstance;
    }

    public ComponentName getTopActivityComponentName() {
        return null;
    }

    public List<?> getVisibleTasks(int displayId) {
        return Collections.emptyList();
    }
}
