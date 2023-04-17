#pragma once

#include <exception>
#include <stdexcept>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#define CL_TARGET_OPENCL_VERSION 300
#include <boost/compute/types.hpp>

#include "color.hpp"
#include "material.hpp"

struct Sphere {
    Material material;
    alignas(cl_float3) glm::vec3 position;

    alignas(cl_float3) float radius;

    Sphere(const Material &material, const glm::vec3 &position, float radius);
};

struct Plane {
    Material material;
    glm::vec3 position;

    alignas(cl_float3) glm::vec3 normal;

    Plane(const Material &material, const glm::vec3 &position, const glm::vec3 &normal);
};

struct Triangle {
	alignas(cl_float3) glm::vec3 normal;
    struct Vertex {
        alignas(cl_float3) glm::vec3 p;
    } vertices[3];

	Triangle();
    Triangle(const glm::vec3 &normal, const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2);
};

/// Collection of triangles
struct Model {
    Material material;
    cl_uint triangle_index;
    cl_uint num_triangles;
    alignas(cl_float3) glm::vec3 bounding_min;
    alignas(cl_float3) glm::vec3 bounding_max;
	alignas(cl_float3) glm::vec3 position;
	alignas(cl_float3) glm::vec3 scale;
	alignas(cl_float3) glm::quat orientation;

	Model();

	/// Create model and compute its bounding box
	Model(const std::vector<Triangle> &triangles, Material material, cl_uint triangle_index, cl_uint num_triangles);

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
    static Model model(const Material &material, const glm::vec3 &position, const glm::vec3 &size);
};

enum ShapeType {
    SHAPE_SPHERE,
    SHAPE_PLANE,
    SHAPE_MODEL
};

struct Shape {
    ShapeType type;
    union U {
        Sphere sphere;
        Plane plane;
        Model model;

        U() {
        }
    } shape;

    Shape(const Sphere &s) {
        type = SHAPE_SPHERE;
        shape.sphere = s;
    }
    Shape(const Plane &p) {
        type = SHAPE_PLANE;
        shape.plane = p;
    }
    Shape(const Model &m) {
        type = SHAPE_MODEL;
        shape.model = m;
    }
};
