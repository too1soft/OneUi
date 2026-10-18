package net.too1.oneui.inputtests;
import android.inputmethodservice.InputMethodService;
import android.view.View;
import android.view.inputmethod.InputConnection;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

/** Deterministic OS keyboard fixture. Installed separately, selected only during
 * emulator tests and restored/uninstalled afterward. Never part of the demo. */
public final class TestKeyboard extends InputMethodService {
    @Override public boolean onEvaluateFullscreenMode() { return false; }
    @Override public boolean onEvaluateInputViewShown() { return true; }
    @Override public View onCreateInputView() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL); root.setBackgroundColor(0xffe7eee8);
        int h = Math.round(52 * getResources().getDisplayMetrics().density);
        TextView title = new TextView(this); title.setText("OneUI 系统键盘测试夹具"); title.setTextSize(16);
        title.setGravity(17); root.addView(title,new LinearLayout.LayoutParams(-1,h));
        add(root,"预编辑中文",h,() -> getCurrentInputConnection().setComposingText("中文",1));
        add(root,"提交中文",h,() -> getCurrentInputConnection().commitText("中文",1));
        add(root,"插入 emoji",h,() -> getCurrentInputConnection().commitText("😀",1));
        add(root,"删除一个码点",h,() -> getCurrentInputConnection().deleteSurroundingTextInCodePoints(1,0));
        return root;
    }
    void add(LinearLayout root,String label,int height,Runnable action) {
        Button b = new Button(this); b.setText(label);
        b.setOnClickListener(v -> { if(getCurrentInputConnection()!=null)action.run(); });
        root.addView(b,new LinearLayout.LayoutParams(-1,height));
    }
}
