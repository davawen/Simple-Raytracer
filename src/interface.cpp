#include "interface.hpp"

bool interface::sphere_properties(
	Sphere &sphere, gizmo_context &ctx, bool opened, bool selected
) {
	bool rerender = false;
	if (opened) {
		rerender |= ImGui::DragFloat3("Position", &sphere.position.x, 0.1f);
		rerender |= ImGui::DragFloat("Radius", &sphere.radius, 0.05f, 1.0f, 0.1f);
	}

	if (selected) {
		static tinygizmo::rigid_transform t;
		t.position = sphere.position;
		glm::vec3 previous_scale = t.scale;
		t.scale = glm::vec3(sphere.radius);
		t.orientation = glm::quat_identity<float, glm::defaultp>();

		bool manipulated = tinygizmo::transform_gizmo("sphere", ctx, t);
		if (manipulated) {
			sphere.position = t.position;

			float diff = 0.0f;
			diff += t.scale.x - previous_scale.x;
			diff += t.scale.y - previous_scale.y;
			diff += t.scale.z - previous_scale.z;

			// std::cout << "From " << glm::to_string(s) << " to " <<
			// glm::to_string(glm::vec3(t.scale)) << ", diff = " << diff << '\n';

			sphere.radius += diff;

			rerender |= manipulated;
		}
	}
	return rerender;
}

bool interface::plane_properties(Plane &plane, gizmo_context &ctx, bool opened, bool selected) {
	bool rerender = false;
	if (opened) {
		rerender |= ImGui::DragFloat3("Position", &plane.position.x, 0.1f);
		rerender |= ImGui::InputFloat3("Normal", &plane.normal.x, "%.3f");
	}

	if (selected) {
		tinygizmo::rigid_transform t;
		t.position = plane.position;
		glm::vec3 up = {0.0f, 1.0f, 0.0f};
		glm::vec3 v = glm::cross(up, plane.normal);
		t.orientation.xyz() = v;
		t.orientation.w =
			glm::sqrt(glm::length2(up) * glm::length2(plane.normal)) + glm::dot(up, plane.normal);
		t.orientation = minalg::normalize(t.orientation);

		bool manipulated = tinygizmo::transform_gizmo("plane", ctx, t);

		if (manipulated) {
			plane.position = t.position;
			plane.normal = glm::rotate((glm::quat)t.orientation, up);

			rerender |= manipulated;
		}
	}

	return rerender;
}

bool interface::model_properties(
	Model &model, std::vector<Triangle> &triangles, gizmo_context &ctx, bool opened, bool selected
) {
	bool moved = false;

	glm::vec3 position, scale;
	glm::quat orientation;
	decompose(model.transform, &scale, &orientation, &position);
	if (selected) {
		tinygizmo::rigid_transform t;
		t.orientation = orientation;
		t.position = position, t.scale = scale;
		bool manipulated = tinygizmo::transform_gizmo("model", ctx, t);
		if (manipulated) {
			position = t.position;
			orientation = t.orientation;
			scale = t.scale;
			moved |= manipulated;
		}
	}

	if (opened) {
		ImGui::Text("%d triangles", model.num_triangles);

		moved |= ImGui::DragFloat3("Position", &position.x, 0.1f);
		// moved |= ImGui::SliderFloat4("Rotation", &orientation.x, -180.0f, 180.0f, "%.1f deg");
		moved |= ImGui::DragFloat3("Size", &scale.x, 0.1f);
	}

	if (moved) {
		model.transform = glm::translate(position) * glm::toMat4(orientation) * glm::scale(scale);
		model.compute_bounding_box(triangles);
		return true;
	}
	return false;
}

bool interface::shape_parameters(
	std::vector<Shape> &shapes, std::vector<Triangle> &triangles, gizmo_context &ctx,
	MaterialHelper &materials
) {
	static int guizmo_selected = -1;

	bool rerender = false;
	if (ImGui::BeginTabItem("Shapes")) {
		auto mode = ctx.get_mode();
		using Mode = tinygizmo::transform_mode;
		const char *text[] = { ICON_FA_UP_DOWN_LEFT_RIGHT, ICON_FA_ROTATE, ICON_FA_MAXIMIZE};
		const char *tooltip[] = {"Translate (Ctrl+T)", "Rotate (Ctrl+R)", "Scale (Ctrl+S)"};
		Mode modes[] = {Mode::translate, Mode::rotate, Mode::scale};
		for (int i = 0; i < 3; i++) {
			if (i > 0)
				ImGui::SameLine();

			bool active = mode == modes[i];
			if (active) {
				auto col = ImGui::GetStyleColorVec4(ImGuiCol_Button);
				col.x *= 0.5f; col.y *= 0.5f; col.z *= 0.5f; // darken color
				ImGui::PushStyleColor(ImGuiCol_Button, col);
			}
			ImGui::Button(text[i]);
			if (active) ImGui::PopStyleColor();

			if (ImGui::BeginItemTooltip()) {
				ImGui::Text("%s", tooltip[i]);
				ImGui::EndTooltip();
			}
		}

		ImGui::BeginDisabled(guizmo_selected == -1);
		if (ImGui::Button(ICON_FA_CLONE " Duplicate")){
			shapes.push_back(shapes[guizmo_selected]);
			guizmo_selected = shapes.size()-1;
		}
		ImGui::EndDisabled();

		if (ImGui::Button("Add sphere")) {
			guizmo_selected = shapes.size();
			shapes.push_back({0, Sphere(glm::vec3(0.0f), 1.0f)});
			rerender |= true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Add plane")) {
			guizmo_selected = shapes.size();
			shapes.push_back({0, Plane(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f))});
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

		ImGui::Separator();

		ImGui::BeginChild("shape_list");
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

			auto flags = ImGuiTreeNodeFlags_FramePadding
				| (selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None);
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
				rerender |= sphere_properties(shape.shape.sphere, ctx, opened, selected);
			else if (shape.type == ShapeType::SHAPE_PLANE)
				rerender |= plane_properties(shape.shape.plane, ctx, opened, selected);
			else if (shape.type == ShapeType::SHAPE_MODEL)
				rerender |= model_properties(shape.shape.model, triangles, ctx, opened, selected);

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
		ImGui::EndChild();

		if (ImGui::BeginPopup("model")) {
			static enum {
				STL,
				OBJ
			} filetype = OBJ;
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

bool interface::camera_parameters(
	Camera &camera, float &movement_speed, float &look_around_speed,
	const std::vector<uint8_t> &pixels, glm::ivec2 canvas_size
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

bool interface::scene_parameters(Tracer::SceneData &scene_data) {
	bool rerender = false;
	if (ImGui::BeginTabItem("Scene")) {
		rerender |= ImGui::ColorEdit3("Horizon color", (float *)&scene_data.horizon_color);
		rerender |= ImGui::ColorEdit3("Zenith color", (float *)&scene_data.zenith_color);
		rerender |= ImGui::ColorEdit3("Ground color", (float *)&scene_data.ground_color);

		rerender |= ImGui::SliderFloat("Sun focus", &scene_data.sun_focus, 0.0f, 100.0f);
		rerender |= ImGui::ColorEdit3("Sun color", (float *)&scene_data.sun_color);
		rerender |= ImGui::SliderFloat(
			"Sun intensity", &scene_data.sun_intensity, 0.0f, 1000.0f, "%.3f",
			ImGuiSliderFlags_Logarithmic
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

bool interface::render_parameters(Tracer::RenderData &render_data, bool &render_raytracing) {
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

bool interface::material_window(MaterialHelper &materials, std::vector<Shape> &shapes) {
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
					"Emission Strength", &material.emission_strength, 0.0f, 100.0f, "%.3f",
					ImGuiSliderFlags_Logarithmic
				);
				rerender |=
					ImGui::SliderFloat("Transmittance", &material.transmittance, 0.0f, 1.0f);
				if (material.transmittance > 0.0f) {
					ImGui::PushItemWidth(-32.0f);
					rerender |=
						ImGui::DragFloat("IOR", &material.refraction_index, 0.01f, 1.0f, 20.0f);
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

void interface::frame_time_window(
	std::deque<float> &frame_times, int &num_frame_samples, bool &limit_fps, int &fps_limit,
	bool &log_fps
) {
	if (ImGui::Begin("Frame times")) {
		ImGui::PlotLines(
			"Timings (ms)",
			[](void *data, int idx) { return ((std::deque<float> *)data)->at(idx); }, &frame_times,
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

void interface::guizmo_render(
	SDL_Renderer *renderer, const glm::mat4 &clip_mat, glm::vec2 win_size,
	const tinygizmo::geometry_mesh &r
) {
	auto trans = [&clip_mat,
	              &win_size](const tinygizmo::geometry_vertex &vd, glm::vec2 &p) -> bool {
		glm::vec4 v = {vd.position.x, vd.position.y, vd.position.z, 1.0f};
		glm::vec4 x = clip_mat * v;
		p = glm::vec2(x) / x.w;
		p = glm::vec2(p.x * 0.5f + 0.5f, 0.5f - 0.5f * p.y);
		p *= win_size;
		return x.z < 0.0f;
	};

	static std::vector<SDL_Vertex> vertices; // keep memory allocation
	vertices.clear();
	vertices.reserve(r.vertices.size());
	for (int i = 0; i < r.vertices.size(); i += 3) {
		SDL_Vertex a[3];
		int clipped = 0;
		for (int j = 0; j < 3; j++) {
			auto &v = r.vertices[i + j];
			glm::vec2 p;
			if (trans(v, p))
				clipped++;
			auto c = minalg::vec<uint8_t, 4>(v.color * 255.0f);
			a[j] = SDL_Vertex{
				.position = SDL_FPoint{p.x, p.y},
				.color = {c.x, c.y, c.z, c.w} //(SDL_Color)v.m_color
			};
		}
		if (clipped == 3)
			continue;

		for (int j = 0; j < 3; j++) {
			SDL_SetRenderDrawColor(
				renderer, a[j].color.r, a[j].color.g, a[j].color.b, a[j].color.a
			);
			SDL_RenderDrawPointsF(renderer, &a[j].position, 1);
			vertices.push_back(a[j]);
		}
	}
	SDL_RenderGeometry(
		renderer, NULL, vertices.data(), vertices.size(), (int *)r.triangles.data(),
		r.triangles.size() * 3
	);
}

void interface::update_guizmo_state(
	tinygizmo::gizmo_application_state &guizmo_state, const ImGuiIO &io, const Camera &camera,
	const glm::mat4 &camera_mat, float aspect_ratio, float fov, float fov_scale, glm::vec2 win_size
) {
	guizmo_state.mouse_left = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	guizmo_state.hotkey_ctrl = ImGui::IsKeyDown(ImGuiKey_ModCtrl);
	guizmo_state.hotkey_local = ImGui::IsKeyDown(ImGuiKey_L);
	guizmo_state.hotkey_translate = ImGui::IsKeyDown(ImGuiKey_T);
	guizmo_state.hotkey_scale = ImGui::IsKeyDown(ImGuiKey_S);
	guizmo_state.hotkey_rotate = ImGui::IsKeyDown(ImGuiKey_R);
	guizmo_state.viewport_size = win_size;
	{
		guizmo_state.ray_origin = camera.position;
		glm::vec2 ndc = {io.MousePos.x, io.MousePos.y};
		ndc /= win_size;
		glm::vec2 screen = {
			(2.0f * ndc.x - 1.0f) * aspect_ratio * fov_scale, (1.0f - 2.0f * ndc.y) * fov_scale};
		glm::vec3 ray = {screen, -1.0f};
		ray = glm::normalize(transform_vec3(camera_mat, ray, false));
		guizmo_state.ray_direction = ray;
	}

	guizmo_state.cam = tinygizmo::camera_parameters{
		.yfov = fov,
		.near_clip = 0.1f,
		.far_clip = 1000.0f,
		.position = camera.position, // quick hacky conversion
		.orientation = glm::toQuat(camera_mat)};
}
