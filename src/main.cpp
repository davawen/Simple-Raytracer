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
#include "imgui_impl_sdlrenderer2.h"
#include <SDL2/SDL.h>

#include "color.hpp"
#include "helper.hpp"
#include "interface.hpp"
#include "parser.hpp"
#include "shape.hpp"
#include "tracer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 540

#define RENDER_WIDTH (WINDOW_WIDTH)
#define RENDER_HEIGHT (WINDOW_HEIGHT)

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
	ImGui_ImplSDLRenderer2_Init(renderer);

	SDL_Texture *texture =
		SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH, RENDER_HEIGHT);

	std::vector<Shape> shapes;
	std::vector<Triangle> triangles;

	MaterialHelper materials;

	materials.push(Material(), "Material0");

	Box::create_triangle(triangles);

	// std::unordered_map<fs::path, ModelPair> model_cache;
	auto randcolor = []() { return color::from_RGB(rand() % 256, rand() % 256, rand() % 256); };
	auto randf = []() { return (float)rand() / (float)RAND_MAX; };

	Camera camera = {{0.0f, 0.0f, -5.0f}, {0.0f, 0.0f, 0.0f}};

	float aspect_ratio = static_cast<float>(RENDER_WIDTH) / RENDER_HEIGHT;

	float fov = glm::pi<float>() / 2.f; // 90 degrees
	float fov_scale = glm::tan(fov / 2.f);

	glm::mat4 camera_to_world;

	Tracer tracer(RENDER_WIDTH, RENDER_HEIGHT);

	tracer.options.num_samples = 2;
	tracer.options.num_bounces = 10;
	tracer.options.show_normals = false;

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
		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		bool rerender = false;

		if (ImGui::Begin("Parameters")) {
			rerender |= interface::shape_parameters(shapes, triangles, materials);
			rerender |= interface::camera_parameters(
				camera, movement_speed, look_around_speed, pixels, glm::ivec2(WINDOW_WIDTH, WINDOW_HEIGHT)
			);
			rerender |= interface::scene_parameters(tracer.scene_data);
			rerender |= interface::render_parameters(tracer.options);

			ImGui::Checkbox("Show demo window", &demo_window);
			if (demo_window) {
				ImGui::ShowDemoWindow();
			}

			if (ImGui::Button("Rerender")) {
				rerender = true;
			}
			ImGui::Checkbox("Render", &render_raytracing);
		}
		ImGui::End();

		rerender |= interface::material_window(materials, shapes);
		if (rerender) {
			time_not_moved = 1;
		}

		interface::frame_time_window(frame_times, num_frame_samples, limit_fps, fps_limit);

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
		camera_to_world = camera.view_matrix();

		// Handle ray tracing
		if (time_not_moved == 1) {
			tracer.clear_canvas();
			tracer.update_scene(shapes, triangles, materials.materials);
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
			int target_y = (height - target_height) / 2;

			// Render to screen
			SDL_UpdateTexture(texture, NULL, pixels.data(), RENDER_WIDTH * 4);

			SDL_Rect dstrect = {.x = 0, .y = target_y, .w = width, .h = target_height};
			SDL_RenderCopy(renderer, texture, NULL, &dstrect);
		}

		if (pressed_keys[SDLK_p]) {
			save_ppm("out.ppm", pixels, WINDOW_WIDTH, WINDOW_HEIGHT);
			pressed_keys[SDLK_p] = false;
		}

		// Render imgui output
		ImGui::Render();
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());

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

	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return EXIT_SUCCESS;
}
