#include "shape.hpp"

Sphere::Sphere(const Material &material, const glm::vec3 &position, float radius) {
    this->material = material;
    this->position = position;
    this->radius = radius;
}

Plane::Plane(const Material &material, const glm::vec3 &position, const glm::vec3 &normal) {
    this->material = material;
    this->position = position;
    this->normal = normal;
}

Triangle::Triangle() {
	this->vertices[0].p = glm::vec3(0.0f);
	this->vertices[1].p = glm::vec3(0.0f);
	this->vertices[2].p = glm::vec3(0.0f);
}

Triangle::Triangle(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2) {
    this->vertices[0].p = v0;
    this->vertices[1].p = v1;
    this->vertices[2].p = v2;
}

Model::Model() {}
Model::Model(const std::vector<Triangle> &triangles, Material material, cl_uint triangle_index, cl_uint num_triangles) {
	this->material = material;
	this->triangle_index = triangle_index;
	this->num_triangles = num_triangles;

	this->bounding_min = glm::vec3(INFINITY);
	this->bounding_max = glm::vec3(-INFINITY);

	for (uint i = 0; i < num_triangles; i++) {
		auto &triangle = triangles[triangle_index + i];

		for (uint j = 0; j < 3; j++) {
			this->bounding_min = glm::min(this->bounding_min, triangle.vertices[0].p);
			this->bounding_max = glm::max(this->bounding_max, triangle.vertices[0].p);
		}
	}

	this->position = glm::vec3(0.0f);
	this->size = glm::vec3(1.0f);
}

void Model::move(glm::vec3 position) {
	auto movement = position - this->position;
	this->bounding_min += movement;
	this->bounding_max += movement;
	this->position = position;
}

void Model::scale(glm::vec3 size) {
	auto change = size / this->size;
	this->bounding_min = (this->bounding_min - this->position) * change + this->position;
	this->bounding_max = (this->bounding_max - this->position) * change + this->position;
	this->size = size;
}

int Box::triangle_index = -1;

Model Box::model(const Material &material, const glm::vec3 &position, const glm::vec3 &size) {
    if (Box::triangle_index == -1) {
        throw std::runtime_error("uninitialized box model, you forgot to call Box::create_triangle");
    }

    Model model;
    model.material = material;
    model.triangle_index = Box::triangle_index;
    model.num_triangles = 12;
	model.bounding_min = position - size*0.5f;
	model.bounding_max = position + size*0.5f;
	model.position = position;
	model.size = size;

	return model;
}

void Box::create_triangle(std::vector<Triangle> &triangles) {
    // 6---7 5
    // |\   \↓
    // 4 2---3
    // \ |   |
    //  \0---1

    const glm::vec3 minCorner = -glm::vec3(0.5f);
    const glm::vec3 maxCorner = glm::vec3(0.5f);

    const glm::vec3 vertices[8] = {{minCorner},
                                   {maxCorner.x, minCorner.yz()},
                                   {minCorner.x, maxCorner.y, minCorner.z},
                                   {maxCorner.xy(), minCorner.z},
                                   {minCorner.xy(), maxCorner.z},
                                   {maxCorner.x, minCorner.y, maxCorner.z},
                                   {minCorner.x, maxCorner.y, maxCorner.z},
                                   {maxCorner}};

    const int table[12][3] = {{0, 1, 3}, {0, 2, 3}, {0, 4, 6}, {0, 2, 6}, {0, 4, 5}, {0, 1, 5},
                              {4, 5, 7}, {4, 6, 7}, {1, 5, 7}, {1, 3, 7}, {2, 6, 7}, {2, 3, 7}};

	Box::triangle_index = triangles.size();
    for (size_t i = 0; i < 12; i++) {
        triangles.push_back(Triangle(vertices[table[i][0]], vertices[table[i][1]], vertices[table[i][2]]));
    }
}
