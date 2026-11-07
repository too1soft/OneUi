package net.too1.oneui.mobile;

import android.graphics.Rect;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Virtual Android semantics for the shared GPU widget surface. Password values
 * remain private; text input belongs to the native editor/Android IME.
 */
final class NativeAccessibility extends View.AccessibilityDelegate {
    interface Bridge {
        NativeUiNode[] nodes();
        boolean activate(NativeUiNode node);
        boolean replaceText(String value);
        boolean sensitive();
        String editorText();
        void scroll(boolean forward);
    }
    private static final int NONE = Integer.MIN_VALUE;
    private final View view;
    private final Bridge bridge;
    private final AccessibilityManager manager;
    private final Map<String, Integer> identifiers = new HashMap<>();
    private final Map<Integer, NativeUiNode> nodes = new LinkedHashMap<>();
    private final java.util.Set<Integer> passwordNodes = new java.util.HashSet<>();
    private int next = 1, focused = NONE, hovered = NONE;
    private String fingerprint = "";
    private final AccessibilityNodeProvider provider = new AccessibilityNodeProvider() {
        @Override
        public AccessibilityNodeInfo createAccessibilityNodeInfo(int id) {
            refreshNodes();
            return info(id);
        }
        @Override
        public boolean performAction(int id, int action, Bundle args) {
            return act(id, action, args);
        }
        @Override
        public AccessibilityNodeInfo findFocus(int kind) {
            refreshNodes();
            if (kind == AccessibilityNodeInfo.FOCUS_ACCESSIBILITY)
                return info(focused);
            for (Map.Entry<Integer, NativeUiNode> entry : nodes.entrySet())
                if (entry.getValue().focused)
                    return info(entry.getKey());
            return null;
        }
        @Override
        public List<AccessibilityNodeInfo> findAccessibilityNodeInfosByText(String text, int id) {
            refreshNodes();
            List<AccessibilityNodeInfo> found = new ArrayList<>();
            for (Map.Entry<Integer, NativeUiNode> entry : nodes.entrySet())
                if (entry.getValue().name.contains(text))
                    found.add(info(entry.getKey()));
            return found;
        }
    };
    NativeAccessibility(View view, Bridge bridge) {
        this.view = view;
        this.bridge = bridge;
        manager = (AccessibilityManager) view.getContext().getSystemService(
            android.content.Context.ACCESSIBILITY_SERVICE);
        view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        view.setAccessibilityDelegate(this);
        view.setOnHoverListener((target, event) -> hover(event));
    }
    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider(View host) {
        return provider;
    }
    private void refreshNodes() {
        nodes.clear();
        Map<String, Integer> occurrences = new HashMap<>();
        float density = view.getResources().getDisplayMetrics().density;
        for (NativeUiNode node : bridge.nodes()) {
            if (node.width <= 0 || node.height <= 0 || node.x + node.width <= 0 || node.y + node.height <= 0
                || node.x * density >= view.getWidth() || node.y * density >= view.getHeight())
                continue;
            String name = node.role + ":" + node.name;
            int occurrence = occurrences.containsKey(name) ? occurrences.get(name) : 0;
            occurrences.put(name, occurrence + 1);
            String key = name + ":" + occurrence;
            if (!identifiers.containsKey(key))
                identifiers.put(key, next++);
            nodes.put(identifiers.get(key), node);
        }
        if (identifiers.size() > 4096) {
            identifiers.clear();
            passwordNodes.clear();
            focused = NONE;
            hovered = NONE;
            next = 1;
        }
    }
    void changed() {
        if (!manager.isEnabled())
            return;
        refreshNodes();
        StringBuilder value = new StringBuilder();
        for (Map.Entry<Integer, NativeUiNode> entry : nodes.entrySet()) {
            NativeUiNode node = entry.getValue();
            value.append(entry.getKey())
                .append(':')
                .append(node.x)
                .append(',')
                .append(node.y)
                .append(',')
                .append(node.width)
                .append(',')
                .append(node.height)
                .append(node.disabled)
                .append(node.selected)
                .append(node.checked)
                .append(node.focused);
        }
        if (value.toString().equals(fingerprint))
            return;
        fingerprint = value.toString();
        if (focused != NONE && !nodes.containsKey(focused)) {
            send(focused, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
            focused = NONE;
        }
        send(View.NO_ID, AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
    }
    private AccessibilityNodeInfo info(int id) {
        if (id == View.NO_ID) {
            AccessibilityNodeInfo root = AccessibilityNodeInfo.obtain(view);
            view.onInitializeAccessibilityNodeInfo(root);
            root.setClassName("android.widget.ScrollView");
            root.setScrollable(true);
            root.addAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD);
            root.addAction(AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD);
            for (int child : nodes.keySet()) root.addChild(view, child);
            return root;
        }
        NativeUiNode node = nodes.get(id);
        if (node == null)
            return null;
        AccessibilityNodeInfo result = AccessibilityNodeInfo.obtain();
        result.setPackageName(view.getContext().getPackageName());
        result.setSource(view, id);
        result.setParent(view);
        result.setClassName(className(node.role));
        if (node.role == 3) {
            result.setContentDescription(node.name);
            result.setHintText(node.name);
            if (node.focused && !bridge.sensitive())
                result.setText(bridge.editorText());
        } else
            result.setText(node.name);
        result.setEnabled(!node.disabled);
        result.setSelected(node.selected);
        result.setFocused(node.focused);
        boolean checkable = node.role == 4 || node.role == 5;
        result.setCheckable(checkable);
        result.setChecked(node.checked);
        result.setFocusable(actionable(node.role));
        result.setClickable(actionable(node.role));
        result.setEditable(node.role == 3);
        if (node.role == 3 && node.focused) {
            if (bridge.sensitive())
                passwordNodes.add(id);
            else
                passwordNodes.remove(id);
        }
        result.setPassword(node.role == 3 && passwordNodes.contains(id));
        float scale = view.getResources().getDisplayMetrics().density;
        Rect bounds = new Rect((int) (node.x * scale), (int) (node.y * scale),
            (int) Math.ceil((node.x + node.width) * scale), (int) Math.ceil((node.y + node.height) * scale));
        bounds.intersect(0, 0, view.getWidth(), view.getHeight());
        result.setBoundsInParent(bounds);
        int[] origin = new int[2];
        view.getLocationOnScreen(origin);
        bounds.offset(origin[0], origin[1]);
        result.setBoundsInScreen(bounds);
        result.setVisibleToUser(view.isShown());
        result.setAccessibilityFocused(focused == id);
        result.addAction(focused == id ? AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS
                                       : AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
        if (!node.disabled && actionable(node.role)) {
            result.addAction(AccessibilityNodeInfo.ACTION_CLICK);
            if (node.role == 3) {
                result.addAction(AccessibilityNodeInfo.ACTION_FOCUS);
                result.addAction(AccessibilityNodeInfo.ACTION_SET_TEXT);
            }
        }
        return result;
    }
    private boolean act(int id, int action, Bundle args) {
        refreshNodes();
        if (id == View.NO_ID
            && (action == AccessibilityNodeInfo.ACTION_SCROLL_FORWARD
                || action == AccessibilityNodeInfo.ACTION_SCROLL_BACKWARD)) {
            bridge.scroll(action == AccessibilityNodeInfo.ACTION_SCROLL_FORWARD);
            return true;
        }
        NativeUiNode node = nodes.get(id);
        if (node == null)
            return false;
        if (action == AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS) {
            if (focused == id)
                return false;
            if (focused != NONE)
                send(focused, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
            focused = id;
            send(id, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED);
            return true;
        }
        if (action == AccessibilityNodeInfo.ACTION_CLEAR_ACCESSIBILITY_FOCUS && focused == id) {
            focused = NONE;
            send(id, AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED);
            return true;
        }
        if (node.disabled)
            return false;
        if (action == AccessibilityNodeInfo.ACTION_CLICK || action == AccessibilityNodeInfo.ACTION_FOCUS) {
            if (!actionable(node.role) || (action == AccessibilityNodeInfo.ACTION_FOCUS && node.role != 3))
                return false;
            boolean result = bridge.activate(node);
            if (node.role == 3 && bridge.sensitive())
                passwordNodes.add(id);
            refreshNodes();
            if (result)
                send(id, AccessibilityEvent.TYPE_VIEW_CLICKED);
            return result;
        }
        if (action == AccessibilityNodeInfo.ACTION_SET_TEXT && node.role == 3 && args != null) {
            CharSequence value =
                args.getCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE);
            if (value == null || value.length() > 65536 || !bridge.activate(node))
                return false;
            if (bridge.sensitive())
                passwordNodes.add(id);
            return bridge.replaceText(value.toString());
        }
        return false;
    }
    private boolean hover(MotionEvent event) {
        if (!manager.isTouchExplorationEnabled())
            return false;
        refreshNodes();
        int hit = NONE;
        if (event.getActionMasked() != MotionEvent.ACTION_HOVER_EXIT) {
            float scale = view.getResources().getDisplayMetrics().density;
            for (Map.Entry<Integer, NativeUiNode> entry : nodes.entrySet()) {
                NativeUiNode node = entry.getValue();
                if (event.getX() >= node.x * scale && event.getX() < (node.x + node.width) * scale
                    && event.getY() >= node.y * scale && event.getY() < (node.y + node.height) * scale)
                    hit = entry.getKey();
            }
        }
        if (hit != hovered) {
            int old = hovered;
            hovered = hit;
            if (hit != NONE)
                send(hit, AccessibilityEvent.TYPE_VIEW_HOVER_ENTER);
            if (old != NONE)
                send(old, AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
        }
        return hit != NONE;
    }
    private void send(int id, int type) {
        if (!manager.isEnabled() || view.getParent() == null)
            return;
        AccessibilityEvent event = AccessibilityEvent.obtain(type);
        event.setPackageName(view.getContext().getPackageName());
        event.setSource(view, id);
        NativeUiNode node = nodes.get(id);
        if (node != null) {
            event.setClassName(className(node.role));
            event.setContentDescription(node.name);
            event.setChecked(node.checked);
            event.setEnabled(!node.disabled);
        }
        if (type == AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED)
            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_SUBTREE);
        view.getParent().requestSendAccessibilityEvent(view, event);
    }
    private static boolean actionable(int role) {
        return role == 1 || role == 3 || role == 4 || role == 5 || role == 7 || role == 10;
    }
    private static String className(int role) {
        switch (role) {
            case 1:
                return "android.widget.Button";
            case 3:
                return "android.widget.EditText";
            case 4:
                return "android.widget.CheckBox";
            case 5:
                return "android.widget.RadioButton";
            case 7:
                return "android.widget.Spinner";
            case 8:
                return "android.widget.SeekBar";
            case 9:
                return "android.widget.ProgressBar";
            case 10:
                return "android.widget.Button";
            default:
                return "android.widget.TextView";
        }
    }
}
