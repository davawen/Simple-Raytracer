#pragma once

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>

#define CL_TARGET_OPENCL_VERSION 200
#include <boost/compute/types.hpp>

struct Sphere {
	alignas(cl_float3) glm::vec3 position;
	alignas(cl_float3) float radius;

	Sphere(const glm::vec3 &position, float radius);
};

struct Plane {
	glm::vec3 position;
	alignas(cl_float3) glm::vec3 normal;

	Plane(const glm::vec3 &position, const glm::vec3 &normal);
};

struct Triangle {
	struct Vertex {
		alignas(cl_float3) glm::vec3 normal;
		alignas(cl_float3) glm::vec3 pos;
	};

	Vertex vertices[3];

	/// Initialize every field to 0
	Triangle();

	/// Constuct a flat shaded triangle
	Triangle(glm::vec3 normal, glm::vec3 pos0, glm::vec3 pos1, glm::vec3 pos2);

	Triangle(Vertex v0, Vertex v1, Vertex v2);
};

/// Collection of triangles
struct Model {
	cl_uint triangle_index;
	cl_uint num_triangles;
	alignas(cl_float3) glm::vec3 bounding_min;
	alignas(cl_float3) glm::vec3 bounding_max;
	alignas(cl_float3) glm::mat4 transform;

	Model();

	/// Create model and compute its bounding box
	Model(
		const std::vector<Triangle> &triangles, cl_uint triangle_index, cl_uint num_triangles
	);

	void compute_bounding_box(const std::vector<Triangle> &triangles);

	/// Change position and recalculate bounding box
	// void move(glm::vec3 position);

	/// Change size and recalculate bounding box
	// void scale(glm::vec3 size);
};

struct Box {
	static int triangle_index;

	/// Creates the triangles necessary for a box
	static void create_triangle(std::vector<Triangle> &triangles);
	static Model model(const glm::vec3 &position, const glm::vec3 &size);
};

enum ShapeType {
	SHAPE_SPHERE,
	SHAPE_PLANE,
	SHAPE_MODEL
};

struct Shape {
	ShapeType type;
	cl_int material;
	union U {
		Sphere sphere;
		Plane plane;
		Model model;

		U() {
		}
	} shape;

	Shape(cl_int material_index, const Sphere &s) {
		type = SHAPE_SPHERE;
		material = material_index;
		shape.sphere = s;
	}
	Shape(cl_int material_index, const Plane &p) {
		type = SHAPE_PLANE;
		material = material_index;
		shape.plane = p;
	}
	Shape(cl_int material_index, const Model &m) {
		type = SHAPE_MODEL;
		material = material_index;
		shape.model = m;
	}
};
