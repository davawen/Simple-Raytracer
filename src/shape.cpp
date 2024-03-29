#include "shape.hpp"
#include "helper.hpp"

Sphere::Sphere(const glm::vec3 &position, float radius) {
	this->position = position;
	this->radius = radius;
}

Plane::Plane(const glm::vec3 &position, const glm::vec3 &normal) {
	this->position = position;
	this->normal = normal;
}

Triangle::Triangle() {
	for (int i = 0; i < 3; i++) {
		vertices[i] = {.normal = glm::vec3(0.0f), .pos = glm::vec3(0.0f)};
	}
}

Triangle::Triangle(glm::vec3 normal, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2) {
	for (int i = 0; i < 3; i++) {
		vertices[i].normal = normal;
	}
	this->vertices[0].pos = v0;
	this->vertices[1].pos = v1;
	this->vertices[2].pos = v2;
}

Triangle::Triangle(Vertex v0, Vertex v1, Vertex v2) {
	this->vertices[0] = v0;
	this->vertices[1] = v1;
	this->vertices[2] = v2;
}

Model::Model() {
}
Model::Model(const std::vector<Triangle> &triangles, cl_uint triangle_index, cl_uint num_triangles) {
	this->triangle_index = triangle_index;
	this->num_triangles = num_triangles;

	this->transform = glm::mat4(1.0f); // identity
	this->compute_bounding_box(triangles);
}

void Model::compute_bounding_box(const std::vector<Triangle> &triangles) {
	bounding_min = glm::vec3(INFINITY);
	bounding_max = glm::vec3(-INFINITY);

	for (uint i = 0; i < num_triangles; i++) {
		auto &triangle = triangles[triangle_index + i];

		for (uint j = 0; j < 3; j++) {
			auto vertex = transform_vec3(transform, triangle.vertices[j].pos, true);
			bounding_min = glm::min(bounding_min, vertex);
			bounding_max = glm::max(bounding_max, vertex);
		}
	}
}

// void Model::move(glm::vec3 position) {
// 	auto movement = position - this->position;
// 	this->bounding_min += movement;
// 	this->bounding_max += movement;
// 	this->position = position;
// }
//
// void Model::scale(glm::vec3 size) {
// 	auto change = size / this->size;
// 	this->bounding_min = (this->bounding_min - this->position) * change + this->position;
// 	this->bounding_max = (this->bounding_max - this->position) * change + this->position;
// 	this->size = size;
// }

int Box::triangle_index = -1;

Model Box::model(const glm::vec3 &position, const glm::vec3 &size) {
	if (Box::triangle_index == -1) {
		throw std::runtime_error("uninitialized box model, you forgot to call Box::create_triangle");
	}

	Model model;
	model.triangle_index = Box::triangle_index;
	model.num_triangles = 12;
	model.bounding_min = position - size * 0.5f;
	model.bounding_max = position + size * 0.5f;
	model.transform = glm::translate(position);

	return model;
}

void Box::create_triangle(std::vector<Triangle> &triangles) {
	// 6---7 5
	// |\   \↓
	// 4 2---3
	// \ |   |
	//  \0---1

	const glm::vec3 vertices[8] = {{-1.0f, -1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f}, {-1.0f, -1.0f, -1.0f},
	                               {-1.0f, 1.0f, -1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f},
	                               {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}};

	const int table[12][3] = {{1, 2, 0}, {3, 6, 2}, {7, 4, 6}, {5, 0, 4}, {6, 0, 2}, {3, 5, 7},
	                          {1, 3, 2}, {3, 7, 6}, {7, 5, 4}, {5, 1, 0}, {6, 4, 0}, {3, 1, 5}};

	Box::triangle_index = triangles.size();
	for (size_t i = 0; i < 12; i++) {
		glm::vec3 v1 = vertices[table[i][0]];
		glm::vec3 v2 = vertices[table[i][1]];
		glm::vec3 v3 = vertices[table[i][2]];

		glm::vec3 A = v2 - v1;
		glm::vec3 B = v3 - v1;

		glm::vec3 normal = glm::cross(A, B);
		normal *= glm::dot(v1, normal) > 0.0f ? 1.0f : -1.0f; // flip if pointing towards the center of the cube

		triangles.push_back(Triangle(glm::normalize(normal), v1, v2, v3));
	}
}
