#pragma once

#include <glm/glm.hpp>

#include "color.hpp"

struct Material
{
	Color color;

	Color specular;
	float specularExponent;

	Color emission;

	Material(){}
	Material(const Color &color, const Color &specular, float specularExponent)
	{
		this->color = color;
		
		this->specular = specular;
		this->specularExponent = specularExponent;
		
		this->emission = color::black;
	}

	Material(const Color &color, const Color &specular, float specularExponent, const Color &emission)
	{
		this->color = color;
		
		this->specular = specular;
		this->specularExponent = specularExponent;
		
		this->emission = emission;
	}
};
