package net.too1.oneui.inputtests;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.text.InputType;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.InputConnection;
import net.too1.oneui.EditorSurface;
import static net.too1.oneui.inputtests.InputProtocol.check;

final class SecureChecks {
    static void privateQueries(InputConnection c) {
        check(c.getTextBeforeCursor(100, 0) == null, "password before-cursor exposed");
        check(c.getTextAfterCursor(100, 0) == null, "password after-cursor exposed");
        check(c.getSelectedText(0) == null, "password selection exposed");
        check(c.getExtractedText(new ExtractedTextRequest(), 0) == null, "password extracted text exposed");
        check(c.getSurroundingText(100, 100, 0) == null, "password surrounding text exposed");
        check(c.takeSnapshot() == null, "password snapshot exposed");
        check(((BaseInputConnection)c).getEditable() == null, "password Java buffer exists");
    }
    static void verify(EditorSurface surface) {
        EditorInfo info = new EditorInfo();
        InputConnection c = surface.onCreateInputConnection(info);
        check(c != null && (info.inputType & InputType.TYPE_MASK_VARIATION) == InputType.TYPE_TEXT_VARIATION_PASSWORD,
              "native password editor type missing");
        check((info.imeOptions & EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING) != 0, "learning flag missing");
        check(info.getInitialTextBeforeCursor(100, 0) == null, "EditorInfo leaked context");
        check(c.commitText("a😀b", 1), "secure commit");
        check(c.setSelection(1, 3), "secure emoji selection"); privateQueries(c);
        check(c.setComposingText("中文", 1), "secure preedit"); privateQueries(c);
        check(c.commitText("中文", 1), "secure composition commit");
        check(c.setSelection(1, 3) && c.commitText("😀", 1), "secure emoji replacement");
        check(!c.setSelection(2, 2), "surrogate midpoint accepted");
        check(c.deleteSurroundingTextInCodePoints(1, 0), "secure codepoint deletion");
        check(!c.commitText("\ud800", 1), "malformed secret accepted");
        check(c.beginBatchEdit() && c.setSelection(0, 2) && c.commitText("batch", 1), "secure batch");
        c.endBatchEdit(); privateQueries(c);
        ClipboardManager clipboard = (ClipboardManager) surface.getContext().getSystemService(Context.CLIPBOARD_SERVICE);
        clipboard.setPrimaryClip(ClipData.newPlainText("test", "fixture"));
        check(c.performContextMenuAction(android.R.id.selectAll), "secure select all");
        check(!c.performContextMenuAction(android.R.id.copy), "secret copy accepted");
        check(!c.performContextMenuAction(android.R.id.cut), "secret cut accepted");
        check("fixture".contentEquals(clipboard.getPrimaryClip().getItemAt(0).getText()), "clipboard overwritten");
        check(c.performContextMenuAction(android.R.id.paste), "password paste failed"); privateQueries(c);
        EditorInfo resumed = new EditorInfo();
        InputConnection replacement = surface.onCreateInputConnection(resumed);
        check(resumed.initialSelStart == 7 && resumed.initialSelEnd == 7, "native secure length mismatch");
        check(!c.commitText("stale", 1), "closed secure connection accepted");
        surface.suspend(); check(!replacement.commitText("late", 1), "suspended secure connection accepted");
        surface.resume(); clipboard.clearPrimaryClip();
    }
}
