#pragma once
#include <cstddef>
namespace oneui::mobile {
inline constexpr int maxFrameSide=8192;
inline constexpr size_t maxFramePixels=33554432;
inline bool validFrameDimensions(int width,int height){
    return width>0&&height>0&&width<=maxFrameSide&&height<=maxFrameSide&&size_t(width)*height<=maxFramePixels;
}
}
