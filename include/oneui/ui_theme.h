#pragma once
#include "oneui/ui_template_support.h"
#include "oneui/ui_density.h"
#include "oneui/ui_declarative.h"
namespace oneui::ui {
enum class VisualPreset { Standard, Soft };
inline std::string declarativeTheme(bool dark=false,Density density=Density::Comfortable,VisualPreset preset=VisualPreset::Standard) {
    const std::string tokens=dark ? R"(:root {
      --chrome:#232e29; --canvas:#101517; --surface:#171e21; --ink:#e5ece8; --muted:#a5b5af;
      --line:#52645d; --control:#232e29; --hover:#34473c; --accent:#c4ed87;
      --accent-ink:#16230f; --selection:#36503e; --error:#ffb4a8; --warning:#efce88; --success:#b2dca4;
    })" : R"(:root {
      --chrome:#e7eee8; --canvas:#f5f7f4; --surface:#ffffff; --ink:#1c2b23; --muted:#54675b;
      --line:#bac8bf; --control:#e7eee8; --hover:#d5e2d7; --accent:#28613d;
      --accent-ink:#ffffff; --selection:#d5e9d7; --error:#a02b24; --warning:#785315; --success:#28613d;
    })";
    const std::string metrics=density==Density::Compact?R"(
      :root { --page-pad:20px; --section-pad:16px; --section-gap:16px; --field-gap:12px; --control-pad:6px; }
    )":R"(
      :root { --page-pad:28px; --section-pad:24px; --section-gap:24px; --field-gap:20px; --control-pad:10px; }
    )";
    const std::string material=preset==VisualPreset::Soft
      ? R"(:root { --control-radius:8px; --surface-radius:14px; --surface-shadow:0px 3px 10px #0000001c; })"
      : R"(:root { --control-radius:6px; --surface-radius:12px; --surface-shadow:none; })";
    const std::string elevation=dark?R"(:root { --raised-shadow:0px 5px 14px #00000070; })":R"(:root { --raised-shadow:0px 5px 14px #00000026; })";
    return syntax::css(tokens+metrics+material+elevation+R"(
      :root { --space-sm:8px; --space-md:16px; --space-lg:24px; --copy-gap:4px; --label-control-gap:8px; }
      Workspace { background-color:var(--line); gap:1px; }
      Column.workspace-core { gap:0px; }
      Row.workspace-core { gap:0px; }
      TitleBar { background-color:var(--chrome); padding:0px 12px; gap:8px; }
      NavigationRail { background-color:var(--chrome); padding:12px 6px; gap:12px; }
      SessionBar { background-color:var(--chrome); padding:0px 8px; gap:4px; }
      WorkspaceBody { background-color:var(--canvas); gap:1px; }
      DockPanel { background-color:var(--surface); gap:0px; }
      PanelHeader { background-color:var(--chrome); padding:0px 12px; gap:8px; }
      PanelBody { background-color:var(--surface); gap:0px; }
      PanelFooter { background-color:var(--surface); padding:0px 12px; gap:8px; }
      StatusBar { background-color:var(--chrome); padding:0px 12px; gap:12px; }
      ToolButton { background-color:transparent; color:var(--ink); border-width:0px; border-radius:4px; padding:6px; font-size:13px; outline-color:var(--accent); outline-width:1px; outline-offset:0px; }
      ToolButton:hover { background-color:var(--hover); }
      ToolButton:pressed { background-color:var(--selection); }
      ToolButton:disabled { color:var(--muted); }
      ToolButton.primary { background-color:var(--accent); color:var(--accent-ink); }
      Button.ghost { background-color:transparent; border-width:0px; }
      Surface { background-color:var(--surface); padding:var(--section-pad); gap:var(--field-gap); border-radius:var(--surface-radius); box-shadow:var(--surface-shadow); }
      Surface.surface-flat { box-shadow:none; border-width:0px; }
      Surface.surface-outlined { box-shadow:none; border-width:1px; border-color:var(--line); }
      Surface.surface-raised { box-shadow:var(--raised-shadow); border-width:0px; }
      Surface.surface-tinted { background-color:var(--selection); box-shadow:none; border-width:0px; }
      Sidebar { gap:8px; padding:16px; background-color:var(--surface); border-radius:var(--surface-radius); }
      SidebarLayout { gap:var(--section-gap); }
      MasterDetail { gap:var(--section-gap); }
      Column { gap:var(--space-md); }
      Content { gap:var(--section-gap); }
      SettingsPage { gap:var(--section-gap); }
      ListPage { gap:var(--section-gap); }
      DetailPage { gap:var(--section-gap); }
      Row { gap:var(--space-md); }
      Page { background-color:var(--canvas); padding:var(--page-pad); gap:var(--section-gap); }
      Header { gap:8px; }
      Section { background-color:var(--surface); padding:var(--section-pad); gap:var(--field-gap); border-radius:var(--surface-radius); }
      Toolbar { gap:8px; }
      ActionBar { gap:12px; padding:12px 0px; }
      FormRow { gap:var(--field-gap); }
      FormGrid { gap:var(--field-gap); }
      FormRow.grid-field { gap:var(--label-control-gap); }
      Column.field-copy { gap:var(--copy-gap); }
      Column.field-control { gap:var(--copy-gap); }
      EmptyState { padding:32px; gap:12px; background-color:var(--surface); border-radius:var(--surface-radius); }
      LoadingState { padding:32px; gap:12px; background-color:var(--surface); border-radius:var(--surface-radius); }
      Scroll { background-color:var(--canvas); }
      Text { color:var(--ink); font-size:14px; font-weight:400; }
      Text.heading { font-size:28px; font-weight:600; }
      Text.section-title { font-size:18px; font-weight:600; }
      Text.page-title { font-size:22px; font-weight:600; }
      Text.muted { font-size:13px; color:var(--muted); }
      Text.field-label { font-weight:600; }
      Text.read-only-label { color:var(--muted); font-size:13px; font-weight:400; }
      Text.field-value { font-size:15px; font-weight:500; }
      ValidationMessage { color:var(--error); font-size:13px; }
      Status { color:var(--muted); font-size:13px; }
      Status.tone-success { color:var(--success); }
      Status.tone-warning { color:var(--warning); }
      Status.tone-error { color:var(--error); }
      Status.tone-pending { color:var(--accent); }
      Button { color:var(--ink); background-color:var(--control); padding:var(--control-pad) 18px;
        font-size:14px; font-weight:500; border-width:0; border-radius:var(--control-radius);
        outline-color:var(--accent); outline-width:2px; outline-offset:2px;
        transition-duration:120ms; }
      Button:hover { background-color:var(--hover); }
      Button:pressed { background-color:var(--selection); }
      Button.primary { background-color:var(--accent); color:var(--accent-ink); }
      Button.primary:hover { background-color:var(--ink); color:var(--canvas); }
      Button:disabled { background-color:var(--control); color:var(--muted); }
      Button.primary:disabled { background-color:var(--control); color:var(--muted); }
      Button.danger { color:var(--error); background-color:var(--surface); border-color:var(--line); border-width:1px; }
      Button.danger:hover { border-color:var(--error); }
      Button.danger:disabled { color:var(--muted); }
      Input { background-color:var(--surface); color:var(--ink); placeholder-color:var(--muted);
        caret-color:var(--accent); selection-color:var(--selection); border-color:var(--line);
        border-width:1px; border-radius:var(--control-radius); padding:var(--control-pad) 12px;
        outline-color:var(--accent); outline-width:2px; outline-offset:2px; }
      Input:hover { border-color:var(--accent); }
      Input.invalid { border-color:var(--error); }
      Input.invalid:focus { border-color:var(--error); outline-color:var(--error); }
      Input:disabled { background-color:var(--control); color:var(--muted); }
      SearchInput { background-color:var(--surface); color:var(--ink); placeholder-color:var(--muted);
        caret-color:var(--accent); selection-color:var(--selection); border-color:var(--line);
        border-width:1px; border-radius:var(--control-radius); padding:var(--control-pad) 12px;
        outline-color:var(--accent); outline-width:2px; outline-offset:2px; }
      Select { background-color:var(--surface); color:var(--ink); border-color:var(--line);
        border-width:1px; border-radius:var(--control-radius); padding:var(--control-pad) 12px; font-size:14px;
        outline-color:var(--accent); outline-width:2px; outline-offset:2px; }
      Select:selected { background-color:var(--selection); }
      Select:hover { border-color:var(--accent); }
      Select:disabled { background-color:var(--control); color:var(--muted); }
      Switch { background-color:var(--line); content-background-color:var(--surface); color:var(--ink);
        outline-color:var(--accent); outline-width:2px; outline-offset:2px; }
      Switch:selected { background-color:var(--accent); content-background-color:var(--accent-ink); }
      DataTable { background-color:var(--surface); color:var(--ink); border-color:var(--line);
        placeholder-color:var(--muted);
        border-width:0; content-background-color:var(--control); font-size:14px;
        scrollbar-color:var(--muted); scrollbar-width:5px; }
      Reveal { transition-duration:220ms; transition-timing-function:ease-out; }
      Button.motion-lift { box-shadow:0px 2px 6px #00000018; transition-duration:140ms; }
      Button.motion-lift:hover { box-shadow:0px 5px 12px #00000030; }
      Button.motion-lift:pressed { box-shadow:0px 1px 3px #00000018; }
      Button.motion-lift:disabled { box-shadow:none; }
      Button.motion-press { box-shadow:0px 2px 4px #00000020; transition-duration:110ms; }
      Button.motion-press:pressed { box-shadow:inset 0px 2px 5px #00000038; }
      Button.motion-press:disabled { box-shadow:none; }
      DataTable:hover { background-color:var(--hover); }
      DataTable:selected { background-color:var(--selection); }
    )");
}
// Opt-in workbench palette. Existing application defaults remain unchanged.
inline std::string workspaceTheme(bool dark=true) {
    return declarativeTheme(dark,Density::Compact)+syntax::css(dark?R"(:root {
        --canvas:#1c2126; --surface:#1c2228; --chrome:#282e34; --ink:#e3e8ed; --muted:#aebac6;
        --line:#3b444e; --control:#2b343d; --hover:#333f49; --accent:#3699ff; --accent-ink:#ffffff;
        --selection:#243e55; --success:#36d69b;
    })":R"(:root {
        --canvas:#edf0f3; --surface:#fafbfc; --chrome:#f0f3f6; --ink:#202732; --muted:#596574;
        --line:#d1d8e0; --control:#e5ebf1; --hover:#e4ebf2; --accent:#0067c7; --accent-ink:#ffffff;
        --selection:#dceafb; --success:#12825d;
    })");
}
inline void applyTheme(Mount& mount,bool dark=false,Density density=Density::Comfortable,VisualPreset preset=VisualPreset::Standard) {
    mount.styles()->replace(declarativeTheme(dark,density,preset),density);
}
}
