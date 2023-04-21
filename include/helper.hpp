#pragma once

#include <vector>
#include <string> 

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include "material.hpp"

struct Camera {
	glm::vec3 position;
	glm::vec3 rotation;

	glm::mat4 view_matrix() {
		glm::mat4 view = glm::translate(glm::mat4(1.f), position);
		view *= glm::eulerAngleYZX(rotation.y, rotation.z, rotation.x);

		return view;
	}
};

struct MaterialHelper {
	std::vector<Material> materials;
	std::vector<std::string> names;

	MaterialHelper() : materials(), names() {}

	void remove(int index) {
		materials.erase(materials.begin() + index);
		names.erase(names.begin() + index);
	}

	void push(Material &&material, std::string &&name) {
		materials.push_back(material);
		names.push_back(name);
	}

	/// Returns the index of the last material pushed
	int last_index() {
		return materials.size() - 1;
	}

	int len() {
		return materials.size();
	}
};
