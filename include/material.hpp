#pragma once

#include <glm/glm.hpp>

#define CL_TARGET_OPENCL_VERSION 200
#include <boost/compute/types.hpp>

#include "color.hpp"

struct Material {
	float smoothness;
	/// Metallic = tinted by the base color
	float metallic;
	/// Specular = untinted reflection
	float specular;
	float emission_strength;
	float transmittance;
	float refraction_index;

	alignas(cl_float3) Color color;
	alignas(cl_float3) Color emission;

	Material(
		const Color &color = color::white, float smoothness = 0.0f, float metallic = 0.0f, float specular = 0.0f,
		float transmittance = 0.0f, float refraction_index = 1.0f, const Color &emission = color::black,
		float emission_strength = 0.0f
	) {
		this->color = color;
		this->smoothness = smoothness;
		this->metallic = metallic;
		this->specular = specular;
		this->transmittance = transmittance;
		this->refraction_index = refraction_index;

		this->emission = emission;
		this->emission_strength = emission_strength;
	}
};
