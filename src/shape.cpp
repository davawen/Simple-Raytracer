#include "shape.hpp"


Sphere::Sphere(const Material &material, const glm::vec3 &position, float radius)
{
	this->material = material;
	this->position = position;
	this->radius = radius;
}

bool Sphere::intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const
{
	if(glm::intersectRaySphere(rayOrigin, rayDirection, this->position, this->radius, intersectionPoint, intersectionNormal))
	{
		intersectionPoint += intersectionNormal*.001f; // intersection point shouldn't self collision

		return true;
	}

	return false;
}

Plane::Plane(const Material &material, const glm::vec3 &position, const glm::vec3 &normal)
{
	this->material = material;
	this->position = position;
	this->normal = normal;
}

bool Plane::intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const
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

Box::Box(const Material &material, const glm::vec3 &position, const glm::vec3 &size)
{
	this->material = material;

	this->position = position;
	this->size = size;
}

bool Box::intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const
{
	return false;
}
