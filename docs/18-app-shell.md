# AppShell

`AppShell` is the reusable top-level product layout for OneUI applications. It exists for tools such as remote control clients, cloud consoles, developer utilities, and admin apps that need the same durable regions without reimplementing window layout each time.

## Regions

- `sidebar`: primary navigation, account/device identity, or app-level command groups.
- `header`: page title, primary actions, tabs, connection state, or search.
- `content`: the active page or tool surface.
- `footer`: status text, transport metrics, background task state, or lightweight diagnostics.

The component is layout-only. It does not paint a theme background and does not impose product copy, icons, or interaction rules. Product-specific visuals should be composed from existing OneUI controls inside each region.

## Behavior

- Fixed sidebar/header/footer dimensions are configured with `setSidebarWidth`, `setHeaderHeight`, and `setFooterHeight`.
- If a fixed dimension is set to `0`, the child preferred size is used.
- `setPadding` reserves outer chrome spacing.
- `setGap` controls the spacing between sidebar/main and between header/content/footer.
- `setSidebarVisible(false)` collapses the sidebar and lets the main region fill the available width while preserving the sidebar child and its last frame.

## Remote Client Usage

The remote client should use `AppShell` as its main window layout:

- Sidebar: navigation for overview, remote control, unattended access, devices, settings.
- Header: current page title and connection actions.
- Content: forms, session preview, session window launcher, and diagnostics.
- Footer: service state, API endpoint, transport mode, and package/runtime warnings.

Remote-specific controls that are missing from OneUI must be added to OneUI first as generic components, then consumed by the remote client.
