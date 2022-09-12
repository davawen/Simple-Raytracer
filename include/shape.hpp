#pragma once

#include <exception>
#include <stdexcept>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>

#define CL_TARGET_OPENCL_VERSION 300
#include <boost/compute/types.hpp>

#include "color.hpp"
#include "material.hpp"

struct Sphere
{
	Material material;
	alignas(cl_float3) glm::vec3 position;

	alignas(cl_float3) float radius;

	Sphere(const Material &material, const glm::vec3 &position, float radius);

	bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const;
};

struct Plane
{
	Material material;
	glm::vec3 position;

	alignas(cl_float3) glm::vec3 normal;

	Plane(const Material &material, const glm::vec3 &position, const glm::vec3 &normal);

	bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const;
};

struct Triangle
{
	Material material;

	struct Vertex
	{
		alignas(cl_float3) glm::vec3 p;
	} vertices[3];

	Triangle(const Material &material, const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2);

	bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const;
};

struct Box
{
	Material material;
	glm::vec3 position;

	alignas(cl_float3) glm::vec3 size;

	Box(const Material &material, const glm::vec3 &position, const glm::vec3 &size);

	bool intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint, glm::vec3 &intersectionNormal) const;

	std::vector<Triangle> to_triangles();
};

enum ShapeType {
	SHAPE_SPHERE,
	SHAPE_PLANE,
	SHAPE_TRIANGLE
};

struct Shape {
	ShapeType type;
	union U {
		Sphere sphere;
		Plane plane;
		Triangle triangle;

		U() {}
	} shape;

	Shape(const Sphere &s) {
		type = SHAPE_SPHERE;
		shape.sphere = s;
	}
	Shape(const Plane &p) {
		type = SHAPE_PLANE;
		shape.plane = p;
	}
	Shape(const Triangle &t) {
		type = SHAPE_TRIANGLE;
		shape.triangle = t;
	}
};


