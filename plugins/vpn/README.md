# OneUI VPN plugin

`oneui.vpn` publishes the renderer-independent `oneui.vpn.sessions` service. It reuses the OneUI VPN configuration, session state machine and native host contract. Applications select the platform VPN adapter and packet engine appropriate to their target.

Build this package against installed SDKs:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/absolute/path/to/sdk
cmake --build build
cmake --install build --prefix /absolute/path/to/plugin-sdk
```

The SDK prefix must provide `OneUIPlugin` and `OneUIVpn`. Match the target architecture, toolchain, C++ runtime and build configuration. The package builds its own source; it does not build OneUI or Skia.

In the application's CMake project:

```cmake
find_package(OneUIVpnPlugin 0.1 CONFIG REQUIRED)
oneui_target_plugins(my_app REGISTRAR register_app_plugins
    PLUGINS OneUI::vpn_plugin)
```

Include `oneui_generated_plugins.h`, create an application-owned plugin runtime, call `register_app_plugins(runtime)`, then start it. Acquire `ONEUI_VPN_SERVICE_ID` with ABI 1 and check `OneUiVpnPluginApiV1` before calling the service. Keep the lease until every session has been disconnected and destroyed. The platform host remains responsible for real permission, native resource and packet-engine events.

When closing, release application leases and stop the runtime. A pending native teardown returns `ONEUI_PLUGIN_BUSY`; continue dispatching native cleanup events and retry stop. Destroy only after stop succeeds. The plugin never treats a cancellation request as a cleanup acknowledgment.

`examples/plugins/vpn_app` is a C SDK-composition example. `tests/plugin/vpn_plugin_tests.cpp` exercises asynchronous native cleanup through a test host. Neither starts a system VPN.
