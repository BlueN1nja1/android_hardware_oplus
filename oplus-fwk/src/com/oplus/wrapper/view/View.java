package com.oplus.wrapper.view;

public class View {
    private final android.view.View mView;

    public View(android.view.View view) {
        mView = view;
    }

    public ViewRootImpl getViewRootImpl() {
        if (mView == null) {
            return null;
        }
        android.view.ViewRootImpl root = mView.getViewRootImpl();
        return root != null ? new ViewRootImpl(root) : null;
    }
}
