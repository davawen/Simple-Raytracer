#pragma once

#include <glm/glm.hpp>

#define CL_TARGET_OPENCL_VERSION 300
#include <boost/compute/types.hpp>

#include "color.hpp"

struct Material {
    alignas(cl_float3) Color color;
    alignas(cl_float3) float smoothness;

    alignas(cl_float3) Color emission;

    Material() {
    }
    Material(const Color &color, const float smoothness = 0.0f, const Color &emission = color::black) {
        this->color = color;
        this->smoothness = smoothness;

        this->emission = emission;
    }
};
