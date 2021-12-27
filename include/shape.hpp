#pragma once

#include <exception>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>

#include "color.hpp"
#include "material.hpp"

struct Shape
{
public:
	Material material;
	glm::vec3 position;

	virtual ~Shape() {}

	virtual bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const = 0;
};

struct Sphere : public Shape
{
	float radius;

	Sphere(Material material, glm::vec3 position, float radius);

	bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const;
};

struct Plane : public Shape
{
	glm::vec3 normal;

	Plane(Material material, glm::vec3 position, glm::vec3 normal);

	bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const;
};
