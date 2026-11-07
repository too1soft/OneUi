package net.too1.oneui.mobile;

/** Semantic UI geometry for native automation and inspection; no editor values. */
public final class NativeUiNode {
    public final String name;
    public final int role;
    public final float x, y, width, height;
    public final float layoutX, layoutY, layoutWidth, layoutHeight;
    public final boolean disabled, selected, checked, focused;
    public NativeUiNode(String name,int role,float x,float y,float width,float height,
                        boolean disabled,boolean selected,boolean checked,boolean focused) {
        this(name,role,x,y,width,height,disabled,selected,checked,focused,x,y,width,height);
    }
    public NativeUiNode(String name,int role,float x,float y,float width,float height,
                        boolean disabled,boolean selected,boolean checked,boolean focused,
                        float layoutX,float layoutY,float layoutWidth,float layoutHeight) {
        this.name=name;this.role=role;this.x=x;this.y=y;this.width=width;this.height=height;
        this.disabled=disabled;this.selected=selected;this.checked=checked;this.focused=focused;
        this.layoutX=layoutX;this.layoutY=layoutY;this.layoutWidth=layoutWidth;this.layoutHeight=layoutHeight;
    }
}
