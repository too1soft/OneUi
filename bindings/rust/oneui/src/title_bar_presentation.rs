use super::{sys, Color, Rect, WindowTitleBar};

/// A line-art command in a 16x16 glyph view box. Paths are copied on submission.
#[derive(Debug, Clone, Copy)]
pub enum CaptionPathCommand { Move(f32,f32), Line(f32,f32), Close }
#[derive(Debug, Clone)]
pub struct CaptionButtonPresentation {
    pub frame: Rect, pub visual: Rect, pub icon: Rect,
    pub glyph: Vec<CaptionPathCommand>, pub maximized_glyph: Vec<CaptionPathCommand>,
    pub stroke_width: f32,
    /// Negative inherits CSS. Nonnegative explicitly overrides the corner radius.
    pub corner_radius: f32,
    pub ellipse: bool, pub glyph_on_group_hover: bool, pub glyph_uses_foreground: bool,
    pub fill: Color, pub pressed_fill: Color, pub glyph_color: Color,
}
impl Default for CaptionButtonPresentation {
    fn default() -> Self { Self {
        frame: Rect::default(), visual: Rect::default(), icon: Rect::default(),
        glyph: vec![], maximized_glyph: vec![], stroke_width: 1.5, corner_radius: -1.0,
        ellipse: false, glyph_on_group_hover: false, glyph_uses_foreground: true,
        fill: Color::rgba(0,0,0,0), pressed_fill: Color::rgba(0,0,0,0), glyph_color: Color::rgb(0,0,0),
    }}
}
/// App-owned local logical coordinates. Resubmit after responsive size changes.
/// Geometry is shared by painting, pointer handling and native window hit tests.
#[derive(Debug, Clone)]
pub struct TitleBarPresentation {
    pub logo: Rect, pub logo_icon: Rect, pub title: Rect, pub leading: Rect, pub accessory: Rect,
    pub logo_stroke_width: f32, pub title_font_size: f32, pub hide_logo_border: bool,
    /// Minimize, maximize/restore, close. Their visual order is unrestricted.
    pub buttons: [CaptionButtonPresentation;3],
}
impl Default for TitleBarPresentation {
    fn default() -> Self { Self {
        logo: Rect::default(), logo_icon: Rect::default(), title: Rect::default(),
        leading: Rect::default(), accessory: Rect::default(), logo_stroke_width: 1.5,
        title_font_size: 12.0, hide_logo_border: false, buttons: std::array::from_fn(|_| Default::default()),
    }}
}
impl WindowTitleBar {
    /// Copies valid data atomically. Returns false without changing the previous
    /// presentation for non-finite/negative geometry or paths over 64 commands.
    /// None restores default geometry; call only on the owning UI thread.
    pub fn set_presentation(&self, presentation: Option<&TitleBarPresentation>) -> bool {
        let Some(p) = presentation else { return unsafe { sys::oneui_title_bar_set_presentation(self.widget.as_raw(), std::ptr::null()) != 0 }; };
        let rect=|r:Rect| sys::OneUiRect{x:r.x,y:r.y,width:r.width,height:r.height};
        let color=|c:Color| sys::OneUiColor{r:c.r,g:c.g,b:c.b,a:c.a};
        let commands=|path:&[CaptionPathCommand]| path.iter().map(|c| match *c {
            CaptionPathCommand::Move(x,y)=>sys::OneUiCaptionPathCommand{verb:0,x,y},
            CaptionPathCommand::Line(x,y)=>sys::OneUiCaptionPathCommand{verb:1,x,y},
            CaptionPathCommand::Close=>sys::OneUiCaptionPathCommand{verb:2,x:0.0,y:0.0},
        }).collect::<Vec<_>>();
        let paths:[_;3]=std::array::from_fn(|i|(commands(&p.buttons[i].glyph),commands(&p.buttons[i].maximized_glyph)));
        let raw=sys::OneUiTitleBarPresentation {
            struct_size:std::mem::size_of::<sys::OneUiTitleBarPresentation>(),
            logo:rect(p.logo),logo_icon:rect(p.logo_icon),title:rect(p.title),leading:rect(p.leading),accessory:rect(p.accessory),
            logo_stroke_width:p.logo_stroke_width,title_font_size:p.title_font_size,hide_logo_border:p.hide_logo_border as i32,
            buttons:std::array::from_fn(|i| {let b=&p.buttons[i];sys::OneUiCaptionButtonPresentation{
                frame:rect(b.frame),visual:rect(b.visual),icon:rect(b.icon),
                glyph:sys::OneUiCaptionPath{commands:paths[i].0.as_ptr(),count:paths[i].0.len()},
                maximized_glyph:sys::OneUiCaptionPath{commands:paths[i].1.as_ptr(),count:paths[i].1.len()},
                stroke_width:b.stroke_width,corner_radius:b.corner_radius,ellipse:b.ellipse as i32,
                glyph_on_group_hover:b.glyph_on_group_hover as i32,glyph_uses_foreground:b.glyph_uses_foreground as i32,
                fill:color(b.fill),pressed_fill:color(b.pressed_fill),glyph_color:color(b.glyph_color),
            }}),
        };
        unsafe{sys::oneui_title_bar_set_presentation(self.widget.as_raw(),&raw)!=0}
    }
}
