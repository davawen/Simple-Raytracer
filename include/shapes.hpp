#pragma once

#include <exception>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>

#include "color.hpp"
#include "material.hpp"

struct Shape
{
	static const enum Type
	{
		SPHERE,
		PLANE,
		BOX
	} type;

	Material material;
	glm::vec3 position;
	
	virtual ~Shape(){}

	virtual bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const = 0;
};

struct Sphere : public Shape
{
	static const Shape::Type type = Shape::Type::SPHERE;

	float radius;

	Sphere(Material material, glm::vec3 position, float radius)
	{
		this->material = material;
		this->position = position;
		this->radius = radius;
	}

	bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const
	{
		if(glm::intersectRaySphere(rayOrigin, rayDirection, this->position, this->radius, intersectionPoint, intersectionNormal))
		{
			intersectionPoint += intersectionNormal*.001f; // intersection point shouldn't self collision

			return true;
		}

		return false;
	}
};

struct Plane : public Shape
{
	static const Shape::Type type = Shape::Type::PLANE;

	glm::vec3 normal;

	Plane(Material material, glm::vec3 position, glm::vec3 normal)
	{
		this->material = material;
		this->position = position;
		this->normal = normal;
	}

	bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const
	{
		float distance;

		if(glm::intersectRayPlane(rayOrigin, rayDirection, this->position, this->normal, distance))
		{
			intersectionPoint = rayOrigin + rayDirection * distance + normal*.001f; // intersection point shouldn't self collision
			intersectionNormal = normal;

			return true;
		}

		return false;
	}
};
