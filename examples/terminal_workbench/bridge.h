#pragma once
#include <oneui/oneui_c_api.h>
#include <cstdint>
extern "C" {
std::uint32_t terminal_demo_abi_version();
void* terminal_demo_create_v1();
OneUiWidget* terminal_demo_widget_v1(void*,std::uint32_t);
void terminal_demo_command_v1(void*,std::uint32_t,OneUiUtf8String);
void terminal_demo_clear_v1(void*,std::uint32_t);
void terminal_demo_destroy_v1(void*);
}
