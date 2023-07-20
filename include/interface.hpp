#pragma once

#include <concepts>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/string_cast.hpp>

#include "imgui.h"
#include "tiny-gizmo.hpp"
#include <IconsFontAwesome6.h>
#include <SDL_render.h>

#include "helper.hpp"
#include "parser.hpp"
#include "shape.hpp"
#include "tracer.hpp"

namespace interface {

using tinygizmo::gizmo_context;

static glm::vec3 quaternion_to_eulerZYX(const glm::quat &q) {
	glm::vec3 a;
	// roll
	double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
	double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
	a.x = atan2(sinr_cosp, cosr_cosp);

	// pitch
	double sinp = std::sqrt(1 + 2 * (q.w * q.y - q.x * q.z));
	double cosp = std::sqrt(1 - 2 * (q.w * q.y - q.x * q.z));
	a.y = 2.0f * atan2(sinp, cosp) - M_PI / 2;

	// yaw
	double siny_cosp = 2 * (q.w * q.z + q.x * q.y);
	double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
	a.z = atan2(siny_cosp, cosy_cosp);

	return a;
}

/// Puts a button at the end of the line
/// @returns The size of this button
static bool end_button(const char *text, float end_offset = 0.0f, float *out_size = nullptr) {
	ImGui::SameLine();

	float size = ImGui::CalcTextSize(text).x + 10.0f;
	if (out_size != nullptr)
		*out_size = size;

	// Right-align
	ImGui::SetCursorPosX(
		ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - size - end_offset
	);
	// ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f)); // small button

	bool out = ImGui::Button(text, ImVec2(size, 0.0f));

	// ImGui::PopStyleVar();

	return out;
}

bool sphere_properties(Sphere &sphere, gizmo_context &ctx, bool opened, bool selected);

bool plane_properties(Plane &plane, gizmo_context &ctx, bool opened, bool selected);

bool model_properties(
	Model &model, std::vector<Triangle> &triangles, gizmo_context &ctx, bool opened, bool selected
);

bool shape_parameters(
	std::vector<Shape> &shapes, std::vector<Triangle> &triangles, gizmo_context &ctx,
	MaterialHelper &materials
);

bool scene_parameters(Tracer::SceneData &scene_data);

bool camera_parameters(
	Camera &camera, float &movement_speed, float &look_around_speed,
	const std::vector<uint8_t> &pixels, glm::ivec2 canvas_size
);

bool render_parameters(Tracer::RenderData &render_data, bool &render_raytracing);

bool material_window(MaterialHelper &materials, std::vector<Shape> &shapes);

void frame_time_window(
	std::deque<float> &frame_times, int &num_frame_samples, bool &limit_fps, int &fps_limit,
	bool &log_fps
);

void update_guizmo_state(
	tinygizmo::gizmo_application_state &guizmo_state, const ImGuiIO &io, const Camera &camera,
	const glm::mat4 &camera_mat, float aspect_ratio, float fov, float fov_scale, glm::vec2 win_size
);

void guizmo_render(
	SDL_Renderer *renderer, const glm::mat4 &clip_mat, glm::vec2 win_size,
	const tinygizmo::geometry_mesh &r
);

} // namespace interface
