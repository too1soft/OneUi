include(CMakeFindDependencyMacro)
find_dependency(OneUIVpn CONFIG)
find_dependency(OneUIJson CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/OneUIVpnJsonTargets.cmake")
