#pragma once

#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "material.hpp"

struct Camera {
	glm::vec3 position;
	float yaw;
	float pitch;

	glm::mat4 camera_matrix() const {
		glm::mat4 camera = glm::translate(glm::mat4(1.0f), position);
		camera *= glm::eulerAngleYXZ(yaw, pitch, 0.0f);

		return camera;
	}

	glm::mat4 view_matrix() const {
		return glm::inverse(camera_matrix());
	}
};

struct MaterialHelper {
	std::vector<Material> materials;
	std::vector<std::string> names;

	MaterialHelper() : materials(), names() {
	}

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

inline auto mptr(glm::mat4 &m) -> decltype(glm::value_ptr(m)) {
	return glm::value_ptr(m);
}

/**
 * @brief Transforms a vector by the given matrix
 *
 * @param m Input matrix
 * @param v Input vector
 * @param translate Wether to use the translation part of the matrix (= set the 4th component of the
 * vector to 1 or 0)
 */
inline glm::vec3 transform_vec3(const glm::mat4 &m, const glm::vec3 &v, bool translate) {
	return glm::vec3(m * glm::vec4(v, translate));
}

inline void decompose(
	const glm::mat4 &m, glm::vec3 *scale, glm::quat *orientation, glm::vec3 *position
) {
	glm::vec3 p, s, skew;
	glm::quat o;
	glm::vec4 perspective;
	glm::decompose(m, s, o, p, skew, perspective);
	if (scale != nullptr)
		*scale = s;
	if (orientation != nullptr)
		*orientation = o;
	if (position != nullptr)
		*position = p;
}

inline float randf() {
	return (float)rand() / (float)RAND_MAX;
}
