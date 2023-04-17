#pragma once

#include <glm/glm.hpp>

#define CL_TARGET_OPENCL_VERSION 200
#include <boost/compute/types.hpp>

#include "color.hpp"

struct Material {
    float smoothness;
    float metalness;
    float emission_strength;
	/// Refraction index of 0 = opaque
	float refraction_index;

    alignas(cl_float3) Color color;
    alignas(cl_float3) Color specular_color;
    alignas(cl_float3) Color emission;

    Material(const Color &color = color::white, const float smoothness = 0.0f, const float metalness = 0.0f, const Color specular_color = color::white, const float refraction_index = 0.0f, const Color &emission = color::black, const float emission_strength = 0.0f) {
        this->color = color;
        this->smoothness = smoothness;
        this->metalness = metalness;
		this->specular_color = specular_color;
		this->refraction_index = refraction_index;

        this->emission = emission;
        this->emission_strength = emission_strength;
    }
};
