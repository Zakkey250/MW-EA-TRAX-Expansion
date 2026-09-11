#pragma once
#include <cmath>
namespace eatrax {
// FE coordinates use 480 vertical units. Preserve the native layout's left
// margin and vertical position while extending its horizontal viewport.
inline float TraxHudOffset(int width,int height,bool widescreen,bool world,float feScale=1.0f) {
    if(!world || width<320 || height<240)return 0.0f;
    const float aspect=static_cast<float>(width)/height;
    if(!std::isfinite(aspect) || aspect<0.75f || aspect>8.0f)return 0.0f;
    if(!std::isfinite(feScale) || feScale<0.1f || feScale>4.0f)return 0.0f;
    return 240.0f*((widescreen?16.0f/9.0f:4.0f/3.0f)-aspect)/feScale;
}
}
