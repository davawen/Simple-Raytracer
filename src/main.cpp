#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#define CL_TARGET_OPENCL_VERSION 300

#include <glm/glm.hpp>

#include <glm/ext/matrix_common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

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

	struct StlHeader {
		uint8_t header[80];
		uint32_t num;
	};

	struct __attribute__((packed)) StlTriangle {
		float normal[3];
		float v1[3];
		float v2[3];
		float v3[3];
		uint16_t attribute;
	};

	FILE *monke = fopen("monke.stl", "r");

	StlHeader header;
	fread(&header, sizeof(StlHeader), 1, monke);

	size_t monke_index = triangles.size();

	for (size_t i = 0; i < header.num; i++) {
		StlTriangle triangle;
		fread(&triangle, sizeof(StlTriangle), 1, monke);

#define ARRAY_TO_VEC3(x) (glm::vec3(x[0], x[1], x[2]))
		auto v1 = glm::rotateX(ARRAY_TO_VEC3(triangle.v1), -1.6f);
		auto v2 = glm::rotateX(ARRAY_TO_VEC3(triangle.v2), -1.6f);
		auto v3 = glm::rotateX(ARRAY_TO_VEC3(triangle.v3), -1.6f);

		triangles.push_back(Triangle(v1, v2, v3));
	}

	fclose(monke);

	auto monke_model = Model(triangles, Material(), monke_index, header.num);

	// shapes.push_back(monke_model);

	for (int i = 0; i < 25; i++) {
		float x = (float)(i % 5) / 4.0f;
		float y = (int)(i / 5) / 4.0f;

		Sphere sphere = Sphere(Material(color::white, 1.0f, 1.0f, color::white, (i+1)/10.0f), {x * 80.0f, 10, y * 80.0f}, 7.0f);
		shapes.push_back(sphere);
	}

	Plane ground_plane = Plane(Material(color::from_RGB(0xDF, 0x2F, 0x00), 0.0f), {0, 0, 0}, {0, 1, 0});
	shapes.push_back(ground_plane);

	Camera camera = {{0.0f, 50.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

	float aspect_ratio = static_cast<float>(RENDER_WIDTH) / RENDER_HEIGHT;

	float fov = glm::pi<float>() / 2.f; // 90 degrees
	float fov_scale = glm::tan(fov / 2.f);

	glm::mat4 camera_to_world;

	Tracer tracer(RENDER_WIDTH, RENDER_HEIGHT);
	Tracer::RenderData options(RENDER_WIDTH, RENDER_HEIGHT);

	options.num_samples = 2;
	options.num_bounces = 10;

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
	double average = 0.;

	bool render_raytracing = true;
	bool demo_window = false;

	int num_frame_samples = 30;
	std::deque<float> frame_times;
	frame_times.resize(num_frame_samples);

	bool limit_fps = true;
	int fps_limit = 30;

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
			case SDL_MOUSEMOTION:
				if (!cursor_moving)
					break;

				if (event.motion.xrel != 0) {
					camera.rotation.y += glm::pi<float>() * event.motion.xrel / 1000.f * fov_scale;
				}

				if (event.motion.yrel != 0) {
					camera.rotation.x += glm::pi<float>() * event.motion.yrel / 1000.f * fov_scale;
				}
				time_not_moved = 1;
				break;
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

				auto show_material = [&rerender](Material &material) {
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
				};

				bool not_removed = true;

				if (shape.type == ShapeType::SHAPE_SPHERE) {
					auto &sphere = shape.shape.sphere;
					if (ImGui::CollapsingHeader("Sphere", &not_removed)) {
						rerender |= ImGui::DragFloat3("Position", &sphere.position.x, 0.1f);
						rerender |= ImGui::DragFloat("Radius", &sphere.radius, 0.05f, 1.0f, 0.1f);
						show_material(sphere.material);
					}
				} else if (shape.type == ShapeType::SHAPE_PLANE) {
					auto &plane = shape.shape.plane;
					if (ImGui::CollapsingHeader("Plane", &not_removed)) {
						rerender |= ImGui::DragFloat3("Position", &plane.position.x, 0.1f);
						show_material(plane.material);
					}
				} else if (shape.type == ShapeType::SHAPE_MODEL) {
					auto &model = shape.shape.model;
					if (ImGui::CollapsingHeader("Model", &not_removed)) {
						ImGui::Text("%d triangles", model.num_triangles);

						auto position = model.position;
						if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
							model.move(position);
							rerender |= true;
						}
						auto size = model.size;
						if (ImGui::DragFloat3("Size", &size.x, 0.1f)) {
							model.scale(size);
							rerender |= true;
						}

						show_material(model.material);
						// ImGui::TreePop();
					}
				}

				// ImGui::SameLine(0.0f, 30.0f);
				if (!not_removed) {
					shapes.erase(shapes.begin() + i);
					i -= 1;
					rerender |= true;
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

			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Camera")) {
			rerender |= ImGui::DragFloat3("Position", &camera.position.x, 0.1f);
			rerender |= ImGui::DragFloat3("Orientation", &camera.rotation.x, 0.1f);

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
			ImGui::SliderInt("Samples", &options.num_samples, 1, 32);
			rerender |= ImGui::SliderInt("Bounces", &options.num_bounces, 1, 32);

			ImGui::Checkbox("Limit FPS", &limit_fps);
			if (limit_fps) {
				ImGui::SameLine(); ImGui::SliderInt("Limit", &fps_limit, 10, 240);
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
				ImGui::SameLine(); ImGui::Text("limited to %d FPS", fps_limit);
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
				camera.position += movement;
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
			options.aspect_ratio = aspect_ratio;
			options.fov_scale = fov_scale;
			options.set_matrix(camera_to_world);
			options.time = start * 1000;
			options.tick = tick;

			tracer.render(time_not_moved, options, pixels);

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

		double loop_duration = (now() - start);
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
		if (limit_fps && loop_duration < 1.0/fps_limit)
			SDL_Delay((1.0/fps_limit - loop_duration) * 1000);
	}

	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return EXIT_SUCCESS;
}
