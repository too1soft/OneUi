use super::Color;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum IconSymbol {
    BrandBloom = 0,
    Search = 1,
    RemoteAssist = 2,
    Monitor = 3,
    Device = 4,
    Toolbox = 5,
    Compass = 6,
    Settings = 7,
    Bell = 8,
    Minimize = 9,
    Maximize = 10,
    Restore = 11,
    Close = 12,
    Heart = 13,
    Desktop = 14,
    File = 15,
    Sparkle = 16,
    RadioOn = 17,
    RadioOff = 18,
    ToggleOn = 19,
    KeyDots = 20,
    Copy = 21,
    ChevronDown = 22,
    ChevronUp = 23,
    Plus = 24,
    User = 25,
    Globe = 26,
    Play = 27,
    Check = 28,
    BrandMark = 29,
    CheckCircle = 30,
    Terminal = 31,
    Server = 32,
    LayoutGrid = 33,
    List = 34,
    Refresh = 35,
    Upload = 36,
    Download = 37,
    Sliders = 38,
    Code = 39,
    Database = 40,
    Cube = 41,
    Notebook = 42,
    Edit = 43,
    Trash = 44,
    ChevronLeft = 45,
    ChevronRight = 46,
    Folder = 47,
    Headset = 48,
    OpenInNew = 49,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ListItem {
    pub title: String,
    pub detail: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SelectionMode {
    Single,
    Multiple,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct VirtualListItem {
    pub title: String,
    pub detail: String,
    pub badge: String,
    pub trailing: String,
    pub indicator_color: Option<Color>,
    pub trailing_color: Option<Color>,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct VirtualListRichMetrics {
    pub indicator_space: f32,
    pub indicator_diameter: f32,
    pub badge_height: f32,
    pub badge_radius: f32,
    pub badge_horizontal_padding: f32,
    pub title_badge_gap: f32,
    pub trailing_width: f32,
    pub trailing_gap: f32,
}

impl Default for VirtualListRichMetrics {
    fn default() -> Self {
        Self {
            indicator_space: 20.0,
            indicator_diameter: 9.0,
            badge_height: 20.0,
            badge_radius: 10.0,
            badge_horizontal_padding: 14.0,
            title_badge_gap: 7.0,
            trailing_width: 58.0,
            trailing_gap: 8.0,
        }
    }
}
