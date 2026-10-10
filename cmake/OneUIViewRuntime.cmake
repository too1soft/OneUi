# One source of binary contracts, shared by the authoring compiler and runtime.
set(ONEUI_VIEW_FORMAT_SOURCES
    src/views/format/binary.cpp
    src/views/format/schema.cpp
    src/views/format/component_contract.cpp
    src/views/format/ir_writer.cpp
    src/views/format/ir_reader.cpp
    src/views/format/ir_validate.cpp
    src/views/format/ir_graph.cpp
    src/views/format/validate.cpp
)
set(ONEUI_VIEW_RUNTIME_SOURCES
    ${ONEUI_VIEW_FORMAT_SOURCES}
    src/views/component_factory.cpp
    src/views/component_properties.cpp
    src/views/value.cpp
    src/views/lifetime.cpp
    src/views/tree.cpp
    src/views/notifications.cpp
    src/views/lists.cpp
    src/views/record_properties.cpp
    src/views/events.cpp
    src/views/cpp_mount.cpp
    src/views/reload.cpp
    src/views/reload_plan.cpp
    src/views/reload_properties.cpp
)

# Optional language-neutral composition layer, never part of the core widgets.
add_library(oneui_views OBJECT ${ONEUI_VIEW_RUNTIME_SOURCES})
target_link_libraries(oneui_views PUBLIC oneui_core)
target_include_directories(oneui_views PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(oneui_views PRIVATE ONEUI_BUILDING_DLL)
set_target_properties(oneui_views PROPERTIES POSITION_INDEPENDENT_CODE ON)
