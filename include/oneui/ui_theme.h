#pragma once
#include "oneui/ui_template_support.h"
#include "oneui/ui_density.h"
#include "oneui/ui_declarative.h"
namespace oneui::ui {
inline std::string declarativeTheme(bool dark=false,Density density=Density::Comfortable) {
    const std::string tokens=dark ? R"(:root {
      --canvas:#101517; --surface:#171e21; --ink:#e5ece8; --muted:#a5b5af;
      --line:#52645d; --control:#232e29; --hover:#34473c; --accent:#c4ed87;
      --accent-ink:#16230f; --selection:#36503e; --error:#ffb4a8; --warning:#efce88; --success:#b2dca4;
    })" : R"(:root {
      --canvas:#f5f7f4; --surface:#ffffff; --ink:#1c2b23; --muted:#54675b;
      --line:#bac8bf; --control:#e7eee8; --hover:#d5e2d7; --accent:#28613d;
      --accent-ink:#ffffff; --selection:#d5e9d7; --error:#a02b24; --warning:#785315; --success:#28613d;
    })";
    const std::string metrics=density==Density::Compact?R"(
      :root { --page-pad:20px; --section-pad:16px; --section-gap:16px; --field-gap:12px; --control-pad:6px; }
    )":R"(
      :root { --page-pad:28px; --section-pad:24px; --section-gap:24px; --field-gap:20px; --control-pad:10px; }
    )";
    return syntax::css(tokens+metrics+R"(
      :root { --space-sm:8px; --space-md:16px; --space-lg:24px; }
      Column { gap:var(--space-md); }
      Content { gap:var(--space-md); }
      SettingsPage { gap:16px; }
      ListPage { gap:16px; }
      DetailPage { gap:16px; }
      Row { gap:var(--space-md); }
      Page { background-color:var(--canvas); padding:var(--page-pad); gap:var(--section-gap); }
      Header { gap:8px; }
      Section { background-color:var(--surface); padding:var(--section-pad); gap:var(--section-gap); border-radius:12px; }
      Toolbar { gap:8px; }
      ActionBar { gap:12px; padding:12px 0px; }
      FormRow { gap:var(--field-gap); }
      FormGrid { gap:var(--section-gap); }
      FormRow.grid-field { gap:8px; }
      Column.field-copy { gap:6px; }
      Column.field-control { gap:6px; }
      EmptyState { padding:32px; gap:12px; background-color:var(--surface); border-radius:12px; }
      LoadingState { padding:32px; gap:12px; background-color:var(--surface); border-radius:12px; }
      Scroll { background-color:var(--canvas); }
      Text { color:var(--ink); font-size:14px; font-weight:400; }
      Text.heading { font-size:28px; font-weight:600; }
      Text.section-title { font-size:18px; font-weight:600; }
      Text.page-title { font-size:22px; font-weight:600; }
      Text.muted { font-size:13px; color:var(--muted); }
      Text.field-label { font-weight:600; }
      ValidationMessage { color:var(--error); font-size:13px; }
      Status { color:var(--muted); font-size:13px; }
      Status.tone-success { color:var(--success); }
      Status.tone-warning { color:var(--warning); }
      Status.tone-error { color:var(--error); }
      Status.tone-pending { color:var(--accent); }
      Button { color:var(--ink); background-color:var(--control); padding:var(--control-pad) 18px;
        font-size:14px; font-weight:500; border-width:0; border-radius:6px;
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
        border-width:1px; border-radius:6px; padding:var(--control-pad) 12px;
        outline-color:var(--accent); outline-width:2px; outline-offset:2px; }
      Input:hover { border-color:var(--accent); }
      Input.invalid { border-color:var(--error); }
      Input.invalid:focus { border-color:var(--error); outline-color:var(--error); }
      Input:disabled { background-color:var(--control); color:var(--muted); }
      SearchInput { background-color:var(--surface); color:var(--ink); placeholder-color:var(--muted);
        caret-color:var(--accent); selection-color:var(--selection); border-color:var(--line);
        border-width:1px; border-radius:6px; padding:var(--control-pad) 12px;
        outline-color:var(--accent); outline-width:2px; outline-offset:2px; }
      Select { background-color:var(--surface); color:var(--ink); border-color:var(--line);
        border-width:1px; border-radius:6px; padding:var(--control-pad) 12px; font-size:14px;
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
      DataTable:hover { background-color:var(--hover); }
      DataTable:selected { background-color:var(--selection); }
    )");
}
inline void applyTheme(Mount& mount,bool dark=false,Density density=Density::Comfortable) {
    mount.styles()->replace(declarativeTheme(dark,density),density);
}
}
