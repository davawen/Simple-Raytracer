#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#define CL_TARGET_OPENCL_VERSION 200
#include <glm/glm.hpp>

#include <glm/ext/matrix_common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/vector_query.hpp>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer.h"
#include <SDL2/SDL.h>

#include "color.hpp"
#include "parser.hpp"
#include "shape.hpp"
#include "tracer.hpp"

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 540

#define RENDER_WIDTH (WINDOW_WIDTH)
#define RENDER_HEIGHT (WINDOW_HEIGHT)

struct Camera {
	glm::vec3 position;
	glm::vec3 rotation;
};

glm::mat4 view_matrix(const Camera &camera) {
	glm::mat4 view = glm::translate(glm::mat4(1.f), camera.position);
	view *= glm::eulerAngleYZX(camera.rotation.y, camera.rotation.z, camera.rotation.x);

	return view;
}

glm::vec3 quaternion_to_eulerZYX(const glm::quat &q) {
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

double now() {
	return std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1'000'000'000.0;
}

int main(int argc, char **) {
	if (argc != 1) {
		printf("Usage: tracer");
		return -1;
	}

	SDL_Renderer *renderer;
	SDL_Window *window;

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

	SDL_CreateWindowAndRenderer(
		WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE, &window, &renderer
	);

	SDL_RendererInfo info;
	SDL_GetRendererInfo(renderer, &info);

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer_Init(renderer);

	SDL_Texture *texture =
		SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH, RENDER_HEIGHT);

	std::vector<Shape> shapes;
	std::vector<Triangle> triangles;
	std::vector<Material> materials;

	Box::create_triangle(triangles);

	// std::unordered_map<fs::path, ModelPair> model_cache;
	auto randcolor = []() { return color::from_RGB(rand() % 256, rand() % 256, rand() % 256); };
	auto randf = []() { return (float)rand() / (float)RAND_MAX; };

	int sphere_material = materials.size();
	materials.push_back(Material(randcolor(), randf(), randf(), randf(), randf(), 1.0f + randf()));

	for (int i = 0; i < 25; i++) {

		float x = (float)(i % 5) * 20.0f;
		float y = (int)(i / 5) * 20.0f;

		Sphere sphere = Sphere({x, 15.0f, y}, 10.0f);
		shapes.push_back({sphere_material, sphere});
	}

	int ground_material = materials.size();
	materials.push_back(Material(color::from_RGB(0xDF, 0x2F, 0x00), 0.0f));

	Plane ground_plane = Plane({0, 0, 0}, {0, 1, 0});
	shapes.push_back({ground_material, ground_plane});

	Camera camera = {{0.0f, 50.0f, 0.0f}, {0.974f, 0.811f, 0.0f}};

	float aspect_ratio = static_cast<float>(RENDER_WIDTH) / RENDER_HEIGHT;

	float fov = glm::pi<float>() / 2.f; // 90 degrees
	float fov_scale = glm::tan(fov / 2.f);

	glm::mat4 camera_to_world;

	Tracer tracer(RENDER_WIDTH, RENDER_HEIGHT);

	tracer.options.num_samples = 2;
	tracer.options.num_bounces = 10;

	tracer.scene_data.horizon_color = color::from_hex(0x374F62);
	tracer.scene_data.zenith_color = color::from_hex(0x11334A);
	tracer.scene_data.ground_color = color::from_hex(0x777777);
	tracer.scene_data.sun_focus = 25.0f;
	tracer.scene_data.sun_color = color::from_hex(0xffffd3);
	tracer.scene_data.sun_intensity = 1.0f;
	tracer.scene_data.sun_direction = VEC3TOCL(glm::normalize(glm::vec3(1.0, -1.0, 0.0)));

	std::vector<uint8_t> pixels(RENDER_WIDTH * RENDER_HEIGHT * 4);

	// SDL state
	bool running = true;

	bool accepting_input = false;
	SDL_SetRelativeMouseMode(SDL_FALSE);

	std::unordered_map<int, bool> pressed_keys;

	// Other state
	int tick = 0;
	cl_uint time_not_moved = 1;
	double average = 0.0;
	float delta_time = 0.0;

	float movement_speed = 15.0f;
	float look_around_speed = 25.0f;

	bool render_raytracing = true;
	bool demo_window = false;

	int num_frame_samples = 60;
	std::deque<float> frame_times;
	frame_times.resize(num_frame_samples);

	bool limit_fps = true;
	int fps_limit = 60;

	SDL_Event event;
	while (running) {
		double start = now();

		while (SDL_PollEvent(&event) != 0) {
			ImGui_ImplSDL2_ProcessEvent(&event);

			switch (event.type) {
			case SDL_KEYDOWN:
				// ctrl-f to toggle input
				if ((event.key.keysym.mod & KMOD_CTRL) && event.key.keysym.sym == SDLK_f) {
					accepting_input = !accepting_input;
					SDL_SetRelativeMouseMode(accepting_input ? SDL_TRUE : SDL_FALSE);
				}

				if (!accepting_input)
					break;
				pressed_keys[event.key.keysym.sym] = true;
				break;
			case SDL_KEYUP:
				pressed_keys[event.key.keysym.sym] = false;
				break;
			case SDL_QUIT:
				running = false;
				break;
			case SDL_MOUSEWHEEL:
				if (!accepting_input)
					break;

				if (event.wheel.y > 0) {
					fov += glm::pi<float>() / 180.f; // 1 degree
				} else if (event.wheel.y < 0) {
					fov -= glm::pi<float>() / 180.f; // 1 degree
				}

				fov_scale = glm::tan(fov / 2.f);
				time_not_moved = 1;
				break;
			case SDL_MOUSEMOTION: {
				if (!accepting_input)
					break;

				auto get_look_speed = [delta_time, look_around_speed, fov_scale](float rel) {
					return glm::pi<float>() * rel * delta_time * look_around_speed * fov_scale / 1000.f;
				};

				if (event.motion.xrel != 0) {
					camera.rotation.y += get_look_speed(event.motion.xrel);
				}

				if (event.motion.yrel != 0) {
					camera.rotation.x += get_look_speed(event.motion.yrel);
				}
				time_not_moved = 1;
				break;
			}
			default:
				break;
			}
		}

		// Handle imgui
		ImGui_ImplSDLRenderer_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Parameters");

		bool rerender = false;

		if (ImGui::TreeNode("Shapes")) {
			for (size_t i = 0; i < shapes.size(); i++) {
				auto &shape = shapes[i];
				ImGui::PushID(i);

				const char *names[] = {"Sphere", "Plane", "Model"};
				const char *name = names[shape.type];

				std::function<void()> properties[] = {
					[&shape, &rerender]() {
						auto &sphere = shape.shape.sphere;
						rerender |= ImGui::DragFloat3("Position", &sphere.position.x, 0.1f);
						rerender |= ImGui::DragFloat("Radius", &sphere.radius, 0.05f, 1.0f, 0.1f);
					},
					[&shape, &rerender]() {
						auto &plane = shape.shape.plane;
						rerender |= ImGui::DragFloat3("Position", &plane.position.x, 0.1f);
					},
					[&triangles, &shape, &rerender]() {
						auto &model = shape.shape.model;
						ImGui::Text("%d triangles", model.num_triangles);

						glm::vec3 angles = quaternion_to_eulerZYX(model.orientation);
						angles = glm::degrees(angles);

						bool moved = false;
						moved |= ImGui::DragFloat3("Position", &model.position.x, 0.1f);
						moved |= ImGui::DragFloat3("Size", &model.scale.x, 0.1f);
						if (ImGui::SliderFloat3("Rotation", &angles.x, -180.0f, 180.0f, "%.1f deg")) {
							model.orientation = glm::quat(glm::radians(angles));
							// glm::eulerAngleZYX
							moved |= true;
						}

						if (moved) {
							model.compute_bounding_box(triangles);
							rerender |= true;
						}
					}};
				std::function<void()> shape_properties = properties[shape.type];

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

				auto close_button = [&shapes, &i, &rerender]() {
					ImGui::SameLine();
					// Right-align
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 10.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f)); // small button

					if (ImGui::Button("X", ImVec2(10.0f, 0.0f))) {
						shapes.erase(shapes.begin() + i);
						i -= 1;
						rerender |= true;
					}

					ImGui::PopStyleVar();
				};

				bool opened = ImGui::TreeNode(name);
				drag_and_drop();
				close_button();

				if (opened) {
					shape_properties();

					std::string name;
					rerender |= ImGui::Combo(
						"Material", &shape.material,
						[](void *void_name, int index, const char **out) {
							std::string &name = *(std::string *)void_name;
							name = "Material" + std::to_string(index);
							*out = name.data(); // hopefully it doesn't try to accumulate multiple results...
							return true;
						},
						&name, materials.size()
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
				if (materials.size() == 0) {
					materials.push_back(Material());
				}
				shapes.push_back({0, Sphere(glm::vec3(0.0f), 10.0f)});
				rerender |= true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Add box")) {
				if (materials.size() == 0) {
					materials.push_back(Material());
				}
				shapes.push_back({0, Box::model(glm::vec3(0.0f), glm::vec3(1.0f))});
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

						if (materials.size() == 0) {
							materials.push_back(Material());
						}
						auto model = Model(triangles, indices->first, indices->second);
						shapes.push_back({0, model});
						rerender |= true;

						ImGui::CloseCurrentPopup();
					}
				}

				ImGui::EndPopup();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Camera")) {
			rerender |= ImGui::DragFloat3("Position", &camera.position.x, 0.1f);
			rerender |= ImGui::DragFloat3("Orientation", &camera.rotation.x, 0.1f);
			ImGui::DragFloat("Movement Speed", &movement_speed, 0.1f, 1.0f, 50.0f);
			ImGui::DragFloat("Look Speed", &look_around_speed, 0.1f, 1.0f, 50.0f);

			if (ImGui::Button("Screenshot")) {
				ImGui::OpenPopup("screenshot");
			}

			if (ImGui::BeginPopup("screenshot")) {
				static char filename[1024];
				ImGui::InputText("Save to", filename, 1024);

				if (ImGui::Button("Save")) {
					save_ppm(filename, pixels, WINDOW_WIDTH, WINDOW_HEIGHT);
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Scene Parameters")) {
			rerender |= ImGui::ColorEdit3("Horizon color", (float *)&tracer.scene_data.horizon_color);
			rerender |= ImGui::ColorEdit3("Zenith color", (float *)&tracer.scene_data.zenith_color);
			rerender |= ImGui::ColorEdit3("Ground color", (float *)&tracer.scene_data.ground_color);

			rerender |= ImGui::SliderFloat("Sun focus", &tracer.scene_data.sun_focus, 0.0f, 100.0f);
			rerender |= ImGui::ColorEdit3("Sun color", (float *)&tracer.scene_data.sun_color);
			rerender |= ImGui::SliderFloat(
				"Sun intensity", &tracer.scene_data.sun_intensity, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic
			);
			auto &dir = tracer.scene_data.sun_direction;
			if (ImGui::DragFloat3("Sun direction", (float *)&dir)) {
				auto glm_dir = glm::normalize(glm::vec3(dir.x, dir.y, dir.z));
				dir = VEC3TOCL(glm_dir);
				rerender = true;
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Render Parameters")) {
			ImGui::SliderInt("Samples", &tracer.options.num_samples, 1, 32);
			rerender |= ImGui::SliderInt("Bounces", &tracer.options.num_bounces, 1, 32);

			ImGui::Checkbox("Limit FPS", &limit_fps);
			if (limit_fps) {
				ImGui::SameLine();
				ImGui::SliderInt("Limit", &fps_limit, 10, 240);
			}

			ImGui::TreePop();
		}

		ImGui::Checkbox("Show demo window", &demo_window);
		if (demo_window) {
			ImGui::ShowDemoWindow();
		}

		if (ImGui::Button("Rerender")) {
			rerender = true;
		}
		ImGui::Checkbox("Render", &render_raytracing);

		ImGui::End();

		if (ImGui::Begin("Materials")) {
			for (cl_int i = 0; i < materials.size(); i++) {
				std::string name = "Material" + std::to_string(i);

				bool opened = ImGui::TreeNode(name.data());
				if (ImGui::BeginDragDropSource()) {
					ImGui::SetDragDropPayload("MATERIAL", &i, sizeof(i));
					ImGui::Text("Set material to Material%d", i);
					ImGui::EndDragDropSource();
				}

				// Close button
				ImGui::SameLine();
				// Right-align
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 10.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f)); // small button

				if (ImGui::Button("X", ImVec2(10.0f, 0.0f))) {
					materials.erase(materials.begin() + i);

					// Avoid having no material
					if (materials.size() == 0) {
						materials.push_back(Material());
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

				ImGui::PopStyleVar();

				if (opened) {
					auto &material = materials[i];

					rerender |= ImGui::ColorEdit3("Color", &material.color.x);
					rerender |= ImGui::SliderFloat("Smoothness", &material.smoothness, 0.0f, 1.0f);
					rerender |= ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
					rerender |= ImGui::SliderFloat("Specular", &material.specular, 0.0f, 1.0f);
					rerender |= ImGui::ColorEdit3("Emission", &material.emission.x);
					rerender |= ImGui::SliderFloat(
						"Emission Strength", &material.emission_strength, 0.0f, 100.0f, "%.3f",
						ImGuiSliderFlags_Logarithmic
					);
					rerender |= ImGui::SliderFloat("Transmittance", &material.transmittance, 0.0f, 1.0f);
					if (material.transmittance > 0.0f) {
						ImGui::PushItemWidth(-32.0f);
						rerender |= ImGui::DragFloat("IOR", &material.refraction_index, 0.01f, 1.0f, 20.0f);
						ImGui::PopItemWidth();
					}

					ImGui::TreePop();
				}
			}
		}
		ImGui::End();

		if (rerender) {
			time_not_moved = 1;
		}

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

			if (ImGui::SliderInt("Number of samples", &num_frame_samples, 1, 120)) {
				frame_times.resize(num_frame_samples);
			}
		}

		ImGui::End();

		// Move camera
		{
			float horizontal = pressed_keys[SDLK_d] - pressed_keys[SDLK_a];
			float transversal = pressed_keys[SDLK_w] - pressed_keys[SDLK_s];
			float vertical = pressed_keys[SDLK_SPACE] - pressed_keys[SDLK_c];

			const glm::vec3 movement = glm::normalize(
				glm::vec3(camera_to_world * glm::vec4(horizontal, 0, transversal, 0)) + glm::vec3(0, vertical, 0)
			); // 0 at the end nullify's translation

			if (!glm::all(glm::isnan(movement)) && !glm::isNull(movement, glm::epsilon<float>())) {
				camera.position += movement * delta_time * movement_speed;
				time_not_moved = 1;
			}
		}
		camera_to_world = view_matrix(camera);

		// Handle ray tracing
		if (time_not_moved == 1) {
			tracer.clear_canvas();
			tracer.update_scene(shapes, triangles, materials);
		}

		if (render_raytracing) {
			auto &options = tracer.options;
			options.aspect_ratio = aspect_ratio;
			options.fov_scale = fov_scale;
			options.camera_to_world = camera_to_world;
			options.time = start * 1000;
			options.tick = tick;

			tracer.render(time_not_moved, pixels);

			int width, height;
			SDL_GetWindowSizeInPixels(window, &width, &height);
			int target_height = (int)(width * (1.0f / aspect_ratio));
			int target_y = (height - target_height)/2;

			// Render to screen
			SDL_UpdateTexture(texture, NULL, pixels.data(), RENDER_WIDTH * 4);

			SDL_Rect dstrect = { .x = 0, .y = target_y, .w = width, .h = target_height };
			SDL_RenderCopy(renderer, texture, NULL, &dstrect);
		}

		if (pressed_keys[SDLK_p]) {
			save_ppm("out.ppm", pixels, WINDOW_WIDTH, WINDOW_HEIGHT);
			pressed_keys[SDLK_p] = false;
		}

		// Render imgui output
		ImGui::Render();
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

		SDL_RenderPresent(renderer);

		double loop_duration = now() - start;
		frame_times.pop_front();
		frame_times.push_back(loop_duration);

		average += loop_duration;
		tick++;
		time_not_moved++;

		if (tick == 60) {
			std::cout << "Average time: " << average * 1000.0 / 60.0 << " ms\n";
			tick = 0;
			average = 0.0f;
		}
		if (limit_fps && loop_duration < 1.0 / fps_limit)
			SDL_Delay((1.0 / fps_limit - loop_duration) * 1000);

		delta_time = now() - start;
	}

	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return EXIT_SUCCESS;
}
