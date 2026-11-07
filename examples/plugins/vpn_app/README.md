# Consume a VPN plugin SDK

This C example creates an application plugin runtime, invokes its generated registrar, acquires the VPN service and creates/destroys an idle session. It verifies linking and ownership without requesting VPN permission or opening a tunnel.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/absolute/path/to/installed/sdk
cmake --build build
```

The prefix must contain OneUIPlugin, OneUIVpn and OneUIVpnPlugin. If these are installed in separate directories, provide all directories in CMAKE_PREFIX_PATH. For a shared SDK, place its runtime libraries on the application's runtime library path or package them beside the executable as appropriate for the target OS.

Real VPN applications supply OneUI's platform adapter callbacks and a packet engine, then use the existing VPN connect/report/disconnect contract. Hold the plugin service lease until all sessions have been destroyed.
