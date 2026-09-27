#pragma once
namespace oneui::ui {
// Opt-in metrics for the declarative adapter; native control defaults stay intact.
enum class Density { Comfortable, Compact };
inline float controlHeight(Density density) { return density==Density::Compact?32.f:40.f; }
inline float tableRowHeight(Density density) { return density==Density::Compact?36.f:44.f; }
}
