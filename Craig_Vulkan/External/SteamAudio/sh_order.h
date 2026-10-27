#pragma once

#include <algorithm>
#include <cmath>

// extracted from Valve Corporation's Steam Audio 4.8.1 sh.cpp, Apache-2.0
// https://github.com/ValveSoftware/steam-audio/tree/v4.8.1/core/src/core
// modified for Craig: keep channel counting independent of SH rotation/Eigen dependencies
namespace ipl::SphericalHarmonics {
    inline int numCoeffsForOrder(int order) { return (order + 1) * (order + 1); }
}
