#pragma once

#include <glm/glm.hpp>

typedef glm::vec<3, float, glm::defaultp> Color;

namespace color
{
	const Color white(1);
	const Color black(0);
	const Color gray(.5f);

	static inline Color from_hex(const uint32_t value)
	{
		return Color(((value & 0xFF0000) >> 16) / 255.f, ((value & 0xFF00) >> 8) / 255.f, (value & 0xFF) / 255.f);
	}

	static inline Color from_RGB(const uint8_t r, const uint8_t g, const uint8_t b)
	{
		return Color(r / 255.f, g / 255.f, b / 255.f);
	}

	Color merge_color(const Color &c1, const Color &c2, const float weight);
};
