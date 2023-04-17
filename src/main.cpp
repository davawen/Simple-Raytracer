#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <vector>
#include <filesystem>
#include <optional>

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

void save_ppm(std::vector<uint8_t> &pixels) {
	std::ofstream file;
	file.open("out.ppm", std::ios::binary | std::ios::out);
	std::string header = "P6 ";
	header += std::to_string(WINDOW_WIDTH) + ' ' + std::to_string(WINDOW_HEIGHT) + ' ';
	header += "255\n";
	file.write(header.c_str(), header.size());

	for (size_t i = 0; i < pixels.size(); i += 4) {
		uint8_t *p = &pixels[i]; // ARGB

		file.write((char *)p + 1, 3);
	}
}

/// Loads an STL model from a file.
/// Caches which models are already loaded.
///
/// Returns the triangle index at which the model starts and its number of triangles.
/// Returns nullopt if the given file does not exist
std::optional<std::pair<uint, uint>> load_stl_model(const std::filesystem::path &filename, std::vector<Triangle> &triangles, bool clear_cache) {
	namespace fs = std::filesystem;
	static std::unordered_map<fs::path, std::pair<uint, uint>> loaded;

	if (!clear_cache && loaded.contains(filename)) {
		return loaded.at(filename);
	}

	std::ifstream file;
	file.open(filename, std::ios::binary | std::ios::in);
	if (file.fail()) {
		return std::nullopt;
	}

	struct StlHeader {
		uint8_t header[80];
		uint32_t num_triangles;
	};

	struct __attribute__((packed)) StlTriangle {
		float normal[3];
		float v1[3];
		float v2[3];
		float v3[3];
		uint16_t attribute;
	};

	StlHeader header;
	file.read((char *)&header, sizeof(StlHeader));

	size_t model_index = triangles.size();

	for (size_t i = 0; i < header.num_triangles; i++) {
		StlTriangle t;
		file.read((char *)&t, sizeof(StlTriangle));

#define ARRAY_TO_VEC3(x) (glm::vec3(x[0], x[1], x[2]))
		triangles.push_back(Triangle(
			ARRAY_TO_VEC3(t.normal),
			ARRAY_TO_VEC3(t.v1),
			ARRAY_TO_VEC3(t.v2),
			ARRAY_TO_VEC3(t.v3)
		));
	}

	loaded[filename] = { model_index, header.num_triangles };
	return {{ model_index, header.num_triangles }};
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

	Box::create_triangle(triangles);

	// for (int i = 0; i < 25; i++) {
	// 	float x = (float)(i % 5) / 4.0f;
	// 	float y = (int)(i / 5) / 4.0f;
	//
	// 	Sphere sphere = Sphere(
	// 		Material(color::white, i < 12 ? 1.0f : 0.0f, 1.0f, color::white, i < 12 ? 1.4f : 0.0f),
	// 		{x * 80.0f, 10, y * 80.0f}, 7.0f
	// 	);
	// 	shapes.push_back(sphere);
	// }

	Plane ground_plane = Plane(Material(color::from_RGB(0xDF, 0x2F, 0x00), 0.0f), {0, 0, 0}, {0, 1, 0});
	shapes.push_back(ground_plane);

	Camera camera = {{0.0f, 50.0f, 0.0f}, {0.974f, 0.811f, 0.0f}};

	float aspect_ratio = static_cast<float>(RENDER_WIDTH) / RENDER_HEIGHT;

	float fov = glm::pi<float>() / 2.f; // 90 degrees
	float fov_scale = glm::tan(fov / 2.f);

	glm::mat4 camera_to_world;

	Tracer tracer(RENDER_WIDTH, RENDER_HEIGHT);

	tracer.options.num_samples = 2;
	tracer.options.num_bounces = 10;

	tracer.scene_data.horizon_color = color::from_hex(0x91c8f2);
	tracer.scene_data.zenith_color = color::from_hex(0x40aff9);
	tracer.scene_data.ground_color = color::from_hex(0x777777);
	tracer.scene_data.sun_focus = 25.0f;
	tracer.scene_data.sun_color = color::from_hex(0xddffcc);
	tracer.scene_data.sun_intensity = 1.0f;
	tracer.scene_data.sun_direction = VEC3TOCL(glm::normalize(glm::vec3(1.0, -1.0, 0.0)));

	std::vector<uint8_t> pixels(RENDER_WIDTH * RENDER_HEIGHT * 4);

	// SDL state
	bool running = true;

	bool cursor_moving = false;
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
				pressed_keys[event.key.keysym.sym] = true;
				if (event.key.keysym.sym == SDLK_v) {
					cursor_moving = !cursor_moving;
					SDL_SetRelativeMouseMode(cursor_moving ? SDL_TRUE : SDL_FALSE);
				}
				break;
			case SDL_KEYUP:
				pressed_keys[event.key.keysym.sym] = false;
				break;
			case SDL_QUIT:
				running = false;
				break;
			case SDL_MOUSEWHEEL:
				if (!cursor_moving)
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
				if (!cursor_moving)
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

				std::function<Material &()> properties[] = {
					[&shape, &rerender]() -> Material & {
						auto &sphere = shape.shape.sphere;
						rerender |= ImGui::DragFloat3("Position", &sphere.position.x, 0.1f);
						rerender |= ImGui::DragFloat("Radius", &sphere.radius, 0.05f, 1.0f, 0.1f);
						return sphere.material;
					},
					[&shape, &rerender]() -> Material & {
						auto &plane = shape.shape.plane;
						rerender |= ImGui::DragFloat3("Position", &plane.position.x, 0.1f);
						return plane.material;
					},
					[&triangles, &shape, &rerender]() -> Material & {
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

						return model.material;
					}};
				std::function<Material &()> inner = properties[shape.type];

				auto drag_and_drop = [&shapes, &i, &name]() {
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

				if (ImGui::TreeNode(name)) {
					drag_and_drop();
					close_button();

					auto &material = inner();
					bool transparent = material.refraction_index != 0.0f;

					if (ImGui::TreeNode("Material")) {
						rerender |= ImGui::ColorEdit3("Color", &material.color.x);
						rerender |= ImGui::ColorEdit3("Specular Color", &material.specular_color.x);
						rerender |= ImGui::SliderFloat("Smoothness", &material.smoothness, 0.0f, 1.0f);
						rerender |= ImGui::SliderFloat("Metalness", &material.metalness, 0.0f, 1.0f);
						rerender |= ImGui::ColorEdit3("Emission", &material.emission.x);
						rerender |= ImGui::SliderFloat(
							"Emission Strength", &material.emission_strength, 0.0f, 100.0f, "%.3f",
							ImGuiSliderFlags_Logarithmic
						);
						if (ImGui::Checkbox("Transparent", &transparent)) {
							material.refraction_index = 1.0f;
							rerender |= true;
						}
						if (transparent) {
							ImGui::SameLine();
							ImGui::PushItemWidth(-32.0f);
							rerender |= ImGui::SliderFloat("IOR", &material.refraction_index, 0.1f, 10.0f);
							ImGui::PopItemWidth();
						} else {
							material.refraction_index = 0.0f;
						}

						ImGui::TreePop();
					}

					ImGui::TreePop();
				} else {
					drag_and_drop();
					close_button();
				}

				ImGui::PopID();
			}

			if (ImGui::Button("Add sphere")) {
				shapes.push_back(Sphere(Material(), glm::vec3(0.0f), 10.0f));
				rerender |= true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Add box")) {
				shapes.push_back(Box::model(Material(), glm::vec3(0.0f), glm::vec3(1.0f)));
				rerender |= true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Add model")) {
				ImGui::OpenPopup("model");
			}

			if (ImGui::BeginPopup("model")) {
				static char filename[1024];
				static bool error = false;
				ImGui::InputText("STL filename", filename, 1024);

				if (error) {
					ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "Inexistant file");
				}

				if (ImGui::Button("Add to scene")) {
					auto indices = load_stl_model(filename, triangles, false);
					if (!indices.has_value()) {
						error = true;
					} else {
						std::memset(filename, 0, 1024);
						error = false;

						auto model = Model(triangles, Material(), indices->first, indices->second);
						shapes.push_back(model);
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

		if (rerender) {
			time_not_moved = 1;
		}

		ImGui::End();

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
			tracer.update_scene(shapes, triangles);
		}

		if (render_raytracing) {
			auto &options = tracer.options;
			options.aspect_ratio = aspect_ratio;
			options.fov_scale = fov_scale;
			options.camera_to_world = camera_to_world;
			options.time = start * 1000;
			options.tick = tick;

			tracer.render(time_not_moved, pixels);

			// Render to screen
			SDL_UpdateTexture(texture, NULL, pixels.data(), RENDER_WIDTH * 4);
			SDL_RenderCopy(renderer, texture, NULL, NULL);
		}

		if (pressed_keys[SDLK_p]) {
			save_ppm(pixels);
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
