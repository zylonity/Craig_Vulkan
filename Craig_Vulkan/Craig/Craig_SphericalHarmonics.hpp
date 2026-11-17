#pragma once

#include <array>
#include <cstdint>

//Google's spherical harmonics library, Apache-2.0 - https://github.com/google/spherical-harmonics
//Through Valve Corporation's Steam Audio 4.8.1 sh.cpp, Apache-2.0 - https://github.com/ValveSoftware/steam-audio
//Changed for Craig: only the hardcoded orders up to 3, floats instead of doubles
//Has to match the library exactly, its reconstructor and decoder read whatever channels we write
namespace Craig::SphericalHarmonics {

	//Order 3 is 16 coefficients
	inline constexpr uint32_t kMaxCoefficients = 16;

	//Fills (order + 1)^2 coefficients for a unit direction in Steam Audio's space (+x right, +y up, +z backwards)
	inline void evaluate(uint32_t order, float dx, float dy, float dz, float* coefficients) {
		//Google's library is +x forward, +y left, +z up, so the axes get swapped round first (convertedDirection upstream)
		const float x = -dz, y = -dx, z = dy;
		//Condon-Shortley phase is left out, same as upstream
		coefficients[0] = 0.282095f;
		if (order < 1) return;
		coefficients[1] = 0.488603f * y;
		coefficients[2] = 0.488603f * z;
		coefficients[3] = 0.488603f * x;
		if (order < 2) return;
		coefficients[4] = 1.092548f * x * y;
		coefficients[5] = 1.092548f * y * z;
		coefficients[6] = 0.315392f * (-x * x - y * y + 2.0f * z * z);
		coefficients[7] = 1.092548f * x * z;
		coefficients[8] = 0.546274f * (x * x - y * y);
		if (order < 3) return;
		coefficients[9] = 0.590044f * y * (3.0f * x * x - y * y);
		coefficients[10] = 2.890611f * x * y * z;
		coefficients[11] = 0.457046f * y * (4.0f * z * z - x * x - y * y);
		coefficients[12] = 0.373176f * z * (2.0f * z * z - 3.0f * x * x - 3.0f * y * y);
		coefficients[13] = 0.457046f * x * (4.0f * z * z - x * x - y * y);
		coefficients[14] = 1.445306f * z * (x * x - y * y);
		coefficients[15] = 0.590044f * x * (x * x - 3.0f * y * y);
	}

}
