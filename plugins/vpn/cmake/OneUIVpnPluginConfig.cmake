include(CMakeFindDependencyMacro)
find_dependency(OneUIPlugin CONFIG)
find_dependency(OneUIVpn CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/OneUIVpnPluginTargets.cmake")
