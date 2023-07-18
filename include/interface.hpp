#pragma once

#include <concepts>
#include <glm/gtx/matrix_decompose.hpp>

#include "imgui.h"

#include "helper.hpp"
#include "parser.hpp"
#include "shape.hpp"
#include "tracer.hpp"

namespace interface {

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
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - size - end_offset);
	// ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f)); // small button

	bool out = ImGui::Button(text, ImVec2(size, 0.0f));

	// ImGui::PopStyleVar();

	return out;
}

inline bool sphere_properties(
	Sphere &sphere, glm::mat4 view, glm::mat4 perspective, /* ImGuizmo::OPERATION op ,*/ bool opened, bool selected
) {
	bool rerender = false;
	if (opened) {
		rerender |= ImGui::DragFloat3("Position", &sphere.position.x, 0.1f);
		rerender |= ImGui::DragFloat("Radius", &sphere.radius, 0.05f, 1.0f, 0.1f);
	}

	if (selected) {
		glm::mat4 m16 = glm::translate(sphere.position);
		auto scale = glm::vec3(sphere.radius);
		m16 *= glm::scale(scale);
		float scratch[3], s[3];

		bool manipulated = false;//ImGuizmo::Manipulate(mptr(view), mptr(perspective), op, ImGuizmo::MODE::LOCAL, mptr(m16));
		if (manipulated) {
			// ImGuizmo::DecomposeMatrixToComponents(mptr(m16), (float *)&sphere.position, scratch, s);

			float diff = 0.0f;
			// diff += s[0] - sphere.radius;
			// diff += s[1] - sphere.radius;
			// diff += s[2] - sphere.radius;
			sphere.radius = s[0];

			rerender |= manipulated;
		}
	}
	return rerender;
}

inline bool plane_properties(
	Plane &plane, glm::mat4 view, glm::mat4 perspective, /* ImGuizmo::OPERATION op,  */bool opened, bool selected
) {
	bool rerender = false;
	if (opened) {
		rerender |= ImGui::DragFloat3("Position", &plane.position.x, 0.1f);
		ImGui::InputFloat3("Normal", &plane.normal.x, "%.3f", ImGuiInputTextFlags_ReadOnly);
	}

	if (selected) {
		glm::mat4 m16 = glm::rotate(0.0f, plane.normal);
		bool manipulated = false;//ImGuizmo::Manipulate(mptr(view), mptr(perspective), op, ImGuizmo::MODE::WORLD, mptr(m16));

		if (manipulated) {
			glm::quat orientation;
			glm::vec3 s;  // Don't care
			glm::vec4 s2; // Don't care

			glm::decompose(m16, s, orientation, plane.position, s, s2);
			plane.normal = glm::axis(orientation);

			rerender |= manipulated;
		}
	}

	return rerender;
}

inline bool model_properties(
	Model &model, std::vector<Triangle> &triangles, glm::mat4 view, glm::mat4 perspective, /* ImGuizmo::OPERATION op,  */bool opened, bool selected
) {
	bool moved = false;
	// ImGuizmo::SetID(rand());
	if (selected) {
		moved |= false;//ImGuizmo::Manipulate(mptr(view), mptr(perspective), op, ImGuizmo::MODE::LOCAL, mptr(model.transform));
	}

	if (opened) {
		ImGui::Text("%d triangles", model.num_triangles);

		// float t[3], r[3], s[3];
		// ImGuizmo::DecomposeMatrixToComponents(mptr(model.transform), t, r, s);
		// moved |= ImGui::DragFloat3("Position", t, 0.1f);
		// moved |= ImGui::SliderFloat3("Rotation", r, -180.0f, 180.0f, "%.1f deg");
		// moved |= ImGui::DragFloat3("Size", s, 0.1f);
	}

	if (moved) {
		// ImGuizmo::RecomposeMatrixFromComponents(t, r, s, mptr(model.transform));
		model.compute_bounding_box(triangles);
		return true;
	}
	return false;
}

inline bool shape_parameters(
	glm::mat4 view_matrix, glm::mat4 perspective_matrix, std::vector<Shape> &shapes, std::vector<Triangle> &triangles,
	MaterialHelper &materials
) {
	static int guizmo_selected = -1;
	//static ImGuizmo::OPERATION guizmo_operation = ImGuizmo::TRANSLATE;

	bool rerender = false;
	if (ImGui::BeginTabItem("Shapes")) {
		// Non shortcutting OR
		// if (ImGui::IsKeyPressed(ImGuiKey_E) | ImGui::RadioButton("Translate", guizmo_operation == ImGuizmo::TRANSLATE))
		// 	guizmo_operation = ImGuizmo::TRANSLATE;
		// ImGui::SameLine();
		// if (ImGui::IsKeyPressed(ImGuiKey_R) | ImGui::RadioButton("Rotate", guizmo_operation == ImGuizmo::ROTATE))
		// 	guizmo_operation = ImGuizmo::ROTATE;
		// ImGui::SameLine();
		// if (ImGui::IsKeyPressed(ImGuiKey_T) | ImGui::RadioButton("Scale", guizmo_operation == ImGuizmo::SCALE))
		// 	guizmo_operation = ImGuizmo::SCALE;
		// ImGui::SameLine();
		// if (ImGui::IsKeyPressed(ImGuiKey_Y) | ImGui::RadioButton("Universal", guizmo_operation == ImGuizmo::UNIVERSAL))
		// 	guizmo_operation = ImGuizmo::UNIVERSAL;

		for (size_t i = 0; i < shapes.size(); i++) {
			auto &shape = shapes[i];
			ImGui::PushID(i);

			const char *names[] = {"Sphere", "Plane", "Model"};
			const char *name = names[shape.type];

			bool selected = guizmo_selected == i;

			auto drag_and_drop = [&rerender, &shapes, &i, &name]() {
				if (ImGui::BeginDragDropSource()) {
					ImGui::SetDragDropPayload("SHAPE", &i, sizeof(i));
					ImGui::Text("Swap with %s", name);
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("SHAPE")) {
						if (payload->DataSize != sizeof(i)) {
							return;
						}
						size_t j = *(size_t *)payload->Data;
						std::swap(shapes[i], shapes[j]);
					} else if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MATERIAL")) {
						if (payload->DataSize != sizeof(cl_int)) {
							return;
						}
						cl_int material = *(cl_int *)payload->Data;

						shapes[i].material = material;
						rerender |= true;
					}

					ImGui::EndDragDropTarget();
				}
			};

			
			auto flags = ImGuiTreeNodeFlags_FramePadding | (selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None);
			bool opened = ImGui::TreeNodeEx(name, flags);
			drag_and_drop();
			float x_size;
			if (end_button("X", 0.0f, &x_size)) {
				shapes.erase(shapes.begin() + i);
				i -= 1;
				rerender |= true;
			}
			if (end_button("S", x_size + 5.0f)) {
				guizmo_selected = selected ? -1 : i;
			}

			if (shape.type == ShapeType::SHAPE_SPHERE)
				rerender |= sphere_properties(shape.shape.sphere, view_matrix, perspective_matrix, /* guizmo_operation, */ opened, selected);
			else if (shape.type == ShapeType::SHAPE_PLANE)
				rerender |= plane_properties(shape.shape.plane, view_matrix, perspective_matrix, /* guizmo_operation, */ opened, selected);
			else if (shape.type == ShapeType::SHAPE_MODEL)
				rerender |= model_properties(shape.shape.model, triangles, view_matrix, perspective_matrix, /* guizmo_operation, */ opened, selected);

			if (opened) {
				rerender |= ImGui::Combo(
					"Material", &shape.material,
					[](void *void_names, int index, const char **out) {
						auto &names = *(std::vector<std::string> *)void_names;
						*out = names[index].c_str();
						return true;
					},
					&materials.names, materials.len()
				);

				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("MATERIAL")) {
						if (payload->DataSize == sizeof(cl_int)) {
							cl_int material = *(cl_int *)payload->Data;

							shape.material = material;
							rerender |= true;
						}
					}

					ImGui::EndDragDropTarget();
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		if (ImGui::Button("Add sphere")) {
			guizmo_selected = shapes.size();
			shapes.push_back({0, Sphere(glm::vec3(0.0f), 1.0f)});
			rerender |= true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Add box")) {
			guizmo_selected = shapes.size();
			shapes.push_back({0, Box::model(glm::vec3(0.0f), glm::vec3(2.0f))});
			rerender |= true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Add model")) {
			ImGui::OpenPopup("model");
		}

		if (ImGui::BeginPopup("model")) {
			static enum {
				STL,
				OBJ
			} filetype;
			ImGui::Text("Filetype");
			ImGui::SameLine();
			ImGui::RadioButton("STL", (int *)&filetype, STL);
			ImGui::SameLine();
			ImGui::RadioButton("OBJ", (int *)&filetype, OBJ);

			static char filename[1024];
			static bool error = false;
			ImGui::InputText("filename", filename, 1024);

			if (error) {
				ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Inexistant file");
			}

			if (ImGui::Button("Add to scene")) {
				std::optional<ModelPair> indices;
				if (filetype == STL) {
					indices = load_stl_model(filename, triangles);
				} else if (filetype == OBJ) {
					indices = load_obj_model(filename, triangles);
				}

				if (!indices.has_value()) {
					error = true;
				} else {
					error = false;

					auto model = Model(triangles, indices->first, indices->second);
					guizmo_selected = shapes.size();
					shapes.push_back({0, model});
					rerender |= true;

					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		ImGui::EndTabItem();
	}

	return rerender;
}

inline bool camera_parameters(
	Camera &camera, float &movement_speed, float &look_around_speed, const std::vector<uint8_t> &pixels,
	glm::ivec2 canvas_size
) {
	bool rerender = false;
	if (ImGui::BeginTabItem("Camera")) {
		rerender |= ImGui::DragFloat3("Position", &camera.position.x, 0.1f);
		rerender |= ImGui::DragFloat2("Orientation", &camera.yaw, 0.1f);
		ImGui::DragFloat("Movement Speed", &movement_speed, 0.1f, 1.0f, 50.0f);
		ImGui::DragFloat("Look Speed", &look_around_speed, 0.1f, 1.0f, 50.0f);

		if (ImGui::Button("Screenshot")) {
			ImGui::OpenPopup("screenshot");
		}

		if (ImGui::BeginPopup("screenshot")) {
			static char filename[1024];
			ImGui::InputText("Save to", filename, 1024);

			if (ImGui::Button("Save")) {
				save_ppm(filename, pixels, canvas_size.x, canvas_size.y);
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::EndTabItem();
	}
	return rerender;
}

inline bool scene_parameters(Tracer::SceneData &scene_data) {
	bool rerender = false;
	if (ImGui::BeginTabItem("Scene")) {
		rerender |= ImGui::ColorEdit3("Horizon color", (float *)&scene_data.horizon_color);
		rerender |= ImGui::ColorEdit3("Zenith color", (float *)&scene_data.zenith_color);
		rerender |= ImGui::ColorEdit3("Ground color", (float *)&scene_data.ground_color);

		rerender |= ImGui::SliderFloat("Sun focus", &scene_data.sun_focus, 0.0f, 100.0f);
		rerender |= ImGui::ColorEdit3("Sun color", (float *)&scene_data.sun_color);
		rerender |= ImGui::SliderFloat(
			"Sun intensity", &scene_data.sun_intensity, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic
		);
		auto &dir = scene_data.sun_direction;
		if (ImGui::DragFloat3("Sun direction", (float *)&dir)) {
			auto glm_dir = glm::normalize(glm::vec3(dir.x, dir.y, dir.z));
			dir = VEC3TOCL(glm_dir);
			rerender = true;
		}
		ImGui::EndTabItem();
	}

	return rerender;
}

inline bool render_parameters(Tracer::RenderData &render_data, bool &render_raytracing) {
	bool rerender = false;
	if (ImGui::BeginTabItem("Render")) {
		ImGui::SliderInt("Samples", &render_data.num_samples, 1, 32);
		rerender |= ImGui::SliderInt("Bounces", &render_data.num_bounces, 1, 32);
		rerender |= ImGui::Checkbox("Show normals", &render_data.show_normals);
		if (ImGui::Button("Rerender")) {
			rerender = true;
		}

		ImGui::Checkbox("Render", &render_raytracing);

		ImGui::EndTabItem();
	}

	return rerender;
}

inline bool material_window(MaterialHelper &materials, std::vector<Shape> &shapes) {
	static int editing_name = -1;
	static char choosen_name[128];

	bool rerender = false;
	if (ImGui::Begin("Materials")) {
		for (cl_int i = 0; i < materials.len(); i++) {
			ImGui::PushID(i);

			std::string &name = materials.names[i];

			bool opened = ImGui::TreeNode(name.c_str());
			if (ImGui::BeginDragDropSource()) {
				ImGui::SetDragDropPayload("MATERIAL", &i, sizeof(i));
				ImGui::Text("Set material to %s", name.c_str());
				ImGui::EndDragDropSource();
			}

			// close button
			if (end_button("X")) {
				materials.remove(i);

				// Avoid having no material
				if (materials.len() == 0) {
					materials.push(Material(), "Material0");
				}

				// Fix ordering
				for (auto &shape : shapes) {
					if (shape.material == i) {
						shape.material = 0;
					} else if (shape.material > i) {
						shape.material -= 1;
					}
				}
				i -= 1;

				rerender |= true;
			}

			// edit name button
			if (end_button("Edit", 15.0f)) {
				std::memcpy(choosen_name, name.c_str(), 127);
				choosen_name[127] = '\0';
				editing_name = i;

				ImGui::OpenPopup("edit_material_name");
			}

			if (ImGui::BeginPopup("edit_material_name")) {
				ImGui::InputText("Name", choosen_name, 128);
				if (ImGui::Button("Enter")) {
					materials.names[editing_name] = choosen_name;
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			if (opened) {
				auto &material = materials.materials[i];

				rerender |= ImGui::ColorEdit3("Color", &material.color.x);
				rerender |= ImGui::SliderFloat("Smoothness", &material.smoothness, 0.0f, 1.0f);
				rerender |= ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
				rerender |= ImGui::SliderFloat("Specular", &material.specular, 0.0f, 1.0f);
				rerender |= ImGui::ColorEdit3("Emission", &material.emission.x);
				rerender |= ImGui::SliderFloat(
					"Emission Strength", &material.emission_strength, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic
				);
				rerender |= ImGui::SliderFloat("Transmittance", &material.transmittance, 0.0f, 1.0f);
				if (material.transmittance > 0.0f) {
					ImGui::PushItemWidth(-32.0f);
					rerender |= ImGui::DragFloat("IOR", &material.refraction_index, 0.01f, 1.0f, 20.0f);
					ImGui::PopItemWidth();
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		if (ImGui::Button("New material")) {
			materials.push(Material(), "Material" + std::to_string(materials.len()));
		}
	}
	ImGui::End();

	return rerender;
}

inline void frame_time_window(std::deque<float> &frame_times, int &num_frame_samples, bool &limit_fps, int &fps_limit, bool &log_fps) {
	if (ImGui::Begin("Frame times")) {
		ImGui::PlotLines(
			"Timings (ms)", [](void *data, int idx) { return ((std::deque<float> *)data)->at(idx); }, &frame_times,
			num_frame_samples
		);
		float sum = 0.0f;
		float min_timing = INFINITY;
		float max_timing = -INFINITY;

		for (auto x : frame_times) {
			sum += x;
			min_timing = glm::min(min_timing, x);
			max_timing = glm::max(max_timing, x);
		}
		sum /= num_frame_samples;

		ImGui::Text("Min: %.3f / Max: %.3f", min_timing * 1000.f, max_timing * 1000.f);
		ImGui::Text("Average timing: %.3f ms", sum * 1000.0f);
		ImGui::Text("FPS: %.1f", 1.0f / sum);
		if (limit_fps) {
			ImGui::SameLine();
			ImGui::Text("limited to %d FPS", fps_limit);
		}

		ImGui::Checkbox("Limit FPS", &limit_fps);
		if (limit_fps) {
			ImGui::SameLine();
			ImGui::PushID("Limit");
			ImGui::SliderInt("", &fps_limit, 10, 240);
			ImGui::PopID();
		}

		if (ImGui::SliderInt("Frametime samples", &num_frame_samples, 1, 120)) {
			frame_times.resize(num_frame_samples);
		}

		ImGui::Checkbox("Log FPS (Console)", &log_fps);

		static bool demo_window = false;
		ImGui::Checkbox("Show demo window", &demo_window);
		if (demo_window) {
			ImGui::ShowDemoWindow();
		}
	}
	ImGui::End();
}

} // namespace interface
