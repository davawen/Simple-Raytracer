#pragma once

#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string> 

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

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

inline auto mptr(glm::mat4 &m) -> decltype(glm::value_ptr(m)) {
	return glm::value_ptr(m);
}

/**
 * @brief Transforms a vector by the given matrix
 *
 * @param m Input matrix
 * @param v Input vector
 * @param translate Wether to use the translation part of the matrix (= set the 4th component of the vector to 1 or 0)
 */
inline glm::vec3 transform_vec3(const glm::mat4 &m, const glm::vec3 &v, bool translate) {
	return glm::vec3(m * glm::vec4(v, translate));
}

/// Converts euler angles to the axis angle representation
/// @returns The axis in the first three components and the angle in the last one
inline glm::vec4 euler_angle_YZX_axis(float heading, float attitude, float bank) {
	// Assuming the angles are in radians.
	float c1 = cosf(heading/2);
	float s1 = sinf(heading/2);
	float c2 = cosf(attitude/2);
	float s2 = sinf(attitude/2);
	float c3 = cosf(bank/2);
	float s3 = sinf(bank/2);
	float c1c2 = c1*c2;
	float s1s2 = s1*s2;
	float w = c1c2*c3 - s1s2*s3;
	float x = c1c2*s3 + s1s2*c3;
	float y = s1*c2*c3 + c1*s2*s3;
	float z = c1*s2*c3 - s1*c2*s3;
	float angle = 2.0f * acosf(w);
	double norm = x*x+y*y+z*z;
	if (norm < 0.001) { // when all euler angles are zero angle =0 so
		// we can set axis to anything to avoid divide by zero
		x=1;
		y=0;
		z=0;
	} else {
		norm = sqrtf(norm);
    	x /= norm;
    	y /= norm;
    	z /= norm;
	}

	return glm::vec4(x, y, z, angle);
}
