//! Eight native resize targets around an existing widget. Content is untouched.
use super::workspace::FloatResizeEdge;
use crate::{
    Color, CommandRegistration, Error, Insets, InteractiveSurface, InteractiveSurfaceStateStyle,
    InteractiveSurfaceStyle, ItemDragPhase, KeyChord, KeyModifiers, OverlayAlignment, OverlayHost,
    Panel, PointerButton, PointerCursor, PointerDragEvent, PointerEvent, Widget,
};
use std::cell::Cell;
use std::rc::Rc;

pub struct FloatingFrame {
    host: OverlayHost,
    material: Panel,
    surface_classes: std::cell::RefCell<(String, String)>,
    handles: Vec<(FloatResizeEdge, InteractiveSurface)>,
    _commands: Vec<CommandRegistration>,
    enabled: Rc<Cell<bool>>,
}
impl FloatingFrame {
    pub fn new(
        content: &Widget,
        on_resize: Rc<dyn Fn(FloatResizeEdge, PointerDragEvent)>,
    ) -> Result<Self, Error> {
        use FloatResizeEdge::*;
        let host = OverlayHost::new()?;
        // A floating pane needs its own readable material, independent of whatever
        // its content is hovering above. Product classes provide theme/background policy.
        let material = Panel::new()?;
        material.set_background(Color::rgba(0, 0, 0, 0));
        material.set_border(Color::rgba(0, 0, 0, 0), 0.0);
        material.set_content(content);
        host.set_content(material.as_widget());
        let mut handles = Vec::new();
        let mut commands = Vec::new();
        let enabled = Rc::new(Cell::new(false));
        for edge in [
            North, South, East, West, NorthEast, NorthWest, SouthEast, SouthWest,
        ] {
            let mut handle = InteractiveSurface::new()?;
            let transparent = InteractiveSurfaceStateStyle::solid(
                Color::rgba(0, 0, 0, 0),
                Color::rgba(0, 0, 0, 0),
                0.0,
            );
            handle.set_style(InteractiveSurfaceStyle {
                normal: transparent,
                hovered: transparent,
                pressed: transparent,
                disabled: transparent,
                focus_visible: transparent,
            });
            handle.set_pointer_cursor(match edge {
                North | South => PointerCursor::ResizeVertical,
                East | West => PointerCursor::ResizeHorizontal,
                NorthWest | SouthEast => PointerCursor::ResizeNorthWestSouthEast,
                _ => PointerCursor::ResizeNorthEastSouthWest,
            });
            handle.as_widget().set_accessible_name("调整浮动面板大小");
            handle.as_widget().set_tab_stop(edge == SouthEast);
            if edge == SouthEast {
                for (key, dx, dy) in [
                    ("left", -1.0, 0.0),
                    ("right", 1.0, 0.0),
                    ("up", 0.0, -1.0),
                    ("down", 0.0, 1.0),
                ] {
                    for (modifiers, step) in
                        [(KeyModifiers::NONE, 10.0), (KeyModifiers::SHIFT, 40.0)]
                    {
                        let cb = Rc::clone(&on_resize);
                        let enabled = Rc::clone(&enabled);
                        commands.push(handle.as_widget().register_command_when(
                            &format!("resize-{key}-{step}"),
                            Some(KeyChord::new(key, modifiers)),
                            move || {
                                let event = PointerDragEvent {
                                    phase: ItemDragPhase::Started,
                                    origin_x: 0.0,
                                    origin_y: 0.0,
                                    pointer: PointerEvent {
                                        x: dx * step,
                                        y: dy * step,
                                        button: PointerButton::None,
                                        click_count: 0,
                                        shift: step == 40.0,
                                        control: false,
                                        alt: false,
                                    },
                                };
                                cb(SouthEast, event);
                                cb(
                                    SouthEast,
                                    PointerDragEvent {
                                        phase: ItemDragPhase::Dropped,
                                        ..event
                                    },
                                );
                            },
                            move || enabled.get(),
                        )?);
                    }
                }
                handle
                    .as_widget()
                    .set_tooltip("拖动缩放；方向键微调，Shift 加速；Esc 取消拖动");
            }
            handle.as_widget().set_visible(false);
            let cb = Rc::clone(&on_resize);
            handle.set_on_drag(0.0, move |event| cb(edge, event));
            host.add_anchored_overlay(
                handle.as_widget(),
                30,
                8.0,
                8.0,
                Insets::default(),
                OverlayAlignment::Start,
                OverlayAlignment::Start,
            );
            handles.push((edge, handle));
        }
        Ok(Self {
            host,
            material,
            surface_classes: std::cell::RefCell::new((String::new(), String::new())),
            handles,
            _commands: commands,
            enabled,
        })
    }
    /// Theme classes for the backing surface in docked and floating modes.
    /// The content instance and its focus/input state stay mounted unchanged.
    pub fn set_surface_classes(
        &self,
        docked: &str,
        floating: &str,
    ) -> Result<(), crate::StyleSheetError> {
        // Validate both values even if the unused mode has not been shown yet.
        if docked.contains('\0') || floating.contains('\0') {
            return Err(crate::StyleSheetError::InteriorNul);
        }
        self.material
            .as_widget()
            .set_classes(if self.enabled.get() { floating } else { docked })?;
        *self.surface_classes.borrow_mut() = (docked.into(), floating.into());
        Ok(())
    }
    pub fn surface_widget(&self) -> &Widget {
        self.material.as_widget()
    }
    pub fn as_widget(&self) -> &Widget {
        self.host.as_widget()
    }
    pub fn resize_handle(&self, edge: FloatResizeEdge) -> Option<&Widget> {
        self.handles
            .iter()
            .find(|(kind, _)| *kind == edge)
            .map(|(_, handle)| handle.as_widget())
    }
    pub fn refresh(&self, floating: bool, width: f32, height: f32) {
        if self.enabled.replace(floating) != floating {
            let classes = self.surface_classes.borrow();
            let _ = self.material.as_widget().set_classes(if floating {
                &classes.1
            } else {
                &classes.0
            });
        }
        use FloatResizeEdge::*;
        let w = width.max(0.0);
        let h = height.max(0.0);
        let n = 8.0f32.min(w * 0.5).min(h * 0.5);
        let c = (n * 1.5).min(w * 0.5).min(h * 0.5);
        for (edge, handle) in &self.handles {
            let (x, y, rw, rh) = match edge {
                North => (c, 0.0, w - 2.0 * c, n),
                South => (c, h - n, w - 2.0 * c, n),
                West => (0.0, c, n, h - 2.0 * c),
                East => (w - n, c, n, h - 2.0 * c),
                NorthWest => (0.0, 0.0, c, c),
                NorthEast => (w - c, 0.0, c, c),
                SouthWest => (0.0, h - c, c, c),
                SouthEast => (w - c, h - c, c, c),
            };
            handle.as_widget().set_visible(floating);
            self.host.update_anchored_overlay(
                handle.as_widget(),
                rw.max(0.0),
                rh.max(0.0),
                Insets {
                    left: x,
                    top: y,
                    ..Default::default()
                },
                OverlayAlignment::Start,
                OverlayAlignment::Start,
            );
        }
    }
}
