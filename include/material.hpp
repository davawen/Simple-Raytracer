#pragma once

#include <glm/glm.hpp>

#include "color.hpp"

struct Material
{
	Color color;

	Color albedo;
	float reflectivness;

	Color emission;

	Material(){}
	Material(const Color &color, float reflectivness)
	{
		this->color = color;
		
		this->reflectivness = reflectivness;
	}

	Material(const Color &color, const Color &albedo, float reflectivness)
	{
		this->color = color;

		this->albedo = albedo;
		this->reflectivness = reflectivness;
	}

	Material(const Color &color, const Color &albedo, const Color &emission, float reflectivness)
	{
		this->color = color;

		this->albedo = albedo;
		this->reflectivness = reflectivness;

		this->emission = emission;
	}
};
