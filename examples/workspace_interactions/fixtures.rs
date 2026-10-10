use crate::{rows::*, vm::Model};
use oneui::view::StableList;
use std::{cell::RefCell, rc::Rc};
pub fn add<T>(list: &mut StableList<T>, key: String, value: T) {
    list.insert(list.len(), key, Rc::new(RefCell::new(value)))
        .unwrap();
}
pub fn populate(m: &mut Model) {
    for i in 0..16 {
        let id = format!("session/{i}");
        add(
            &mut m.panes,
            id.clone(),
            PaneModel {
                id,
                title: format!("本地 {:02}", i + 1),
                path: "~/workspace".into(),
                ..Default::default()
            },
        );
    }
    for family in [
        "Consolas",
        "Cascadia Mono",
        "Segoe UI",
        "Microsoft YaHei UI",
    ] {
        add(
            &mut m.fonts,
            family.into(),
            Font {
                id: family.into(),
                family: family.into(),
                label: family.into(),
                available: true,
            },
        );
    }
    let names = [
        "纸白", "雾灰", "沙岩", "薄荷", "海盐", "暖杏", "墨黑", "石墨", "深海", "森林", "紫夜",
        "赤陶",
    ];
    let accents = [
        0x32684aff, 0x536270ff, 0x806246ff, 0x227565ff, 0x286881ff, 0x966238ff, 0x84c49aff,
        0xa6b5c4ff, 0x73b9dcff, 0x85bd93ff, 0xc1a0dfff, 0xdfa18cff,
    ];
    let surfaces = [
        0xffffffff, 0xf0f3f6ff, 0xfff5eaff, 0xeaf8efff, 0xeaf4faff, 0xffeee2ff,
        0x202820ff, 0x293039ff, 0x203342ff, 0x20392bff, 0x352b41ff, 0x42302bff,
    ];
    for i in 0..12 {
        let mut tokens = StableList::new();
        for (id, rgba) in [
            ("surface", surfaces[i]),
            ("ink", if i < 6 { 0x21302bff } else { 0xe3eae5ff }),
            ("accent", accents[i]),
            ("control", if i < 6 { 0xe7eee8ff } else { 0x35453bff }),
            ("line", if i < 6 { 0xc6d0c8ff } else { 0x43574aff }),
        ] {
            add(
                &mut tokens,
                id.into(),
                Color {
                    id: id.into(),
                    rgba,
                },
            );
        }
        let id = format!("theme-{i}");
        add(
            &mut m.themes,
            id.clone(),
            Theme {
                id,
                title: names[i].into(),
                tokens,
            },
        );
    }
    m.activeGroup = "main".into();
    m.activePane = "session/0".into();
}
pub fn css(dark: bool) -> String {
    let css = include_str!("theme.css");
    if dark {
        css.replace("#f3f6f3", "#18201b")
            .replace("#ffffff", "#242d28")
            .replace("#21302b", "#e3eae5")
            .replace("#62716a", "#a5b5ac")
            .replace("#c6d0c8", "#43574a")
            .replace("#e7eee8", "#35453b")
            .replace("#32684a", "#84c49a")
    } else {
        css.into()
    }
}
