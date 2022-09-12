#pragma once

#include <glm/glm.hpp>

#define CL_TARGET_OPENCL_VERSION 300
#include <boost/compute/types.hpp>

#include "color.hpp"

struct Material
{
	enum Type
	{
		DIFFUSE,
		REFLECTIVE
	};

	Type type;

	alignas(cl_float3) Color color;

	alignas(cl_float3) Color specular;
	alignas(cl_float3) float specularExponent;

	alignas(cl_float3) Color emission;

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
