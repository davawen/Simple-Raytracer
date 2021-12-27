#pragma once

#include <vector>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include <glm/gtc/noise.hpp>
#include <glm/gtx/norm.hpp>

#include "color.hpp"
#include "shape.hpp"

#define MAX_BOUNCE 4

float noise3D(glm::vec3 v)
{
    return glm::fract(glm::sin(v.x*112.9898f + v.y*179.233f + v.z*237.212f) * 43758.5453f);
}

struct Intersection
{
	glm::vec3 point;
	glm::vec3 normal;
	Shape *shape;

	Intersection()
	{
		this->shape = NULL;
	}
};

Shape *closestIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, const std::vector<Shape *> &shapes, glm::vec3 &point, glm::vec3 &normal)
{
	Shape *closestShape = NULL;

	// Find closest intersection
	for(auto &shape : shapes)
	{
		glm::vec3 intersectionPoint, intersectionNormal;
		if(shape->intersectRay(rayOrigin, rayDirection, intersectionPoint, intersectionNormal))
		{
			if(closestShape == NULL || (glm::distance2(rayOrigin, intersectionPoint) < glm::distance2(rayOrigin, point)))
			{
				closestShape = shape;
				point = intersectionPoint;
				normal = intersectionNormal;
			}
		}
	}

	return closestShape;
}

Color ray_cast(const glm::vec3 &startOrigin, const glm::vec3 &startDirection, const std::vector<Shape *> shapes)
{
	Color result(0);
	//Color mask(1);
	Color energy(1.f);

	glm::vec3 rayOrigin = startOrigin;
	glm::vec3 rayDirection = startDirection;

	for(size_t i = 0; i < MAX_BOUNCE; i++)
	{
		Intersection intersection;

		intersection.shape = closestIntersection(rayOrigin, rayDirection, shapes, intersection.point, intersection.normal);

		if(intersection.shape != NULL) // Intersection happenned
		{
			rayOrigin = intersection.point;
			rayDirection = glm::reflect(rayDirection, intersection.normal);

			result += energy * intersection.shape->material.color * glm::dot(rayDirection, intersection.normal);

			energy *= intersection.shape->material.specular;

			// mask *= intersection.shape->material.color;

			//energy += intersection.shape->material.emission; 
		}
		else
		{
			result += energy * color::from_RGB(0x42, 0x79, 0xbc);
			break;
		}

		//if(glm::length2(mask) < 0.01f*0.01f) break;
	}

	return glm::clamp(result, Color(0), Color(1));
}
