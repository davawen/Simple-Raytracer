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
#include "tiny-gizmo.hpp"
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

	tinygizmo::gizmo_context guizmo_ctx;
	tinygizmo::gizmo_application_state guizmo_state;

	SDL_Texture *texture = SDL_CreateTexture(
		renderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH, RENDER_HEIGHT
	);

	std::vector<Shape> shapes;
	std::vector<Triangle> triangles;

	MaterialHelper materials;

	materials.push(Material(), "Material0");

	Box::create_triangle(triangles);

	// std::unordered_map<fs::path, ModelPair> model_cache;

	Camera camera = {{0.0f, 0.0f, 5.0f}, 0.0f, 0.0f};

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

	int num_frame_samples = 60;
	std::deque<float> frame_times;
	frame_times.resize(num_frame_samples);

	bool limit_fps = true;
	bool log_fps = false;
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
					return -glm::pi<float>() * rel * delta_time * look_around_speed * fov_scale
						/ 1000.f;
				};

				if (event.motion.xrel != 0) {
					camera.yaw += get_look_speed(event.motion.xrel);
				}

				if (event.motion.yrel != 0) {
					camera.pitch += get_look_speed(event.motion.yrel);
				}
				time_not_moved = 1;
				break;
			}
			default:
				break;
			}
		}

		bool rerender = false;

		// Move camera
		{
			float horizontal = pressed_keys[SDLK_d] - pressed_keys[SDLK_a];
			float transversal = pressed_keys[SDLK_s] - pressed_keys[SDLK_w];
			float vertical = pressed_keys[SDLK_SPACE] - pressed_keys[SDLK_c];

			const glm::vec3 movement = glm::normalize(
				glm::vec3(camera_to_world * glm::vec4(horizontal, 0, transversal, 0))
				+ glm::vec3(0, vertical, 0)
			); // 0 at the end nullify's translation

			if (!glm::all(glm::isnan(movement)) && !glm::isNull(movement, glm::epsilon<float>())) {
				camera.position += movement * delta_time * movement_speed;
				time_not_moved = 1;
			}
		}
		camera_to_world = camera.camera_matrix();

		// Handle imgui
		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		auto &io = ImGui::GetIO();
		glm::vec2 win_size = glm::vec2(io.DisplaySize.x, io.DisplaySize.y);

		// Im3d state
		glm::mat4 perspective_mat = glm::infinitePerspective(fov, aspect_ratio, 0.1f);
		glm::mat4 view_mat = camera.view_matrix();

		guizmo_state.mouse_left = ImGui::IsMouseDown(ImGuiMouseButton_Left);
		guizmo_state.hotkey_ctrl = ImGui::IsKeyDown(ImGuiKey_ModCtrl);
		guizmo_state.hotkey_local = ImGui::IsKeyDown(ImGuiKey_L);
		guizmo_state.hotkey_translate = ImGui::IsKeyDown(ImGuiKey_T);
		guizmo_state.hotkey_scale = ImGui::IsKeyDown(ImGuiKey_S);
		guizmo_state.hotkey_rotate = ImGui::IsKeyDown(ImGuiKey_R);
		guizmo_state.viewport_size = minalg::float2(win_size.x, win_size.y);
		{
			ImGui::Begin("aaa");
			guizmo_state.ray_origin = minalg::float3(camera.position.x, camera.position.y, camera.position.z);
			ImGui::InputFloat3("ray origin", &camera.position.x);
			glm::vec2 ndc = {io.MousePos.x, io.MousePos.y};
			ndc /= win_size;
			ImGui::InputFloat2("ndc", &ndc.x);
			glm::vec2 screen = {
				(2.0f * ndc.x - 1.0f) * aspect_ratio * fov_scale,
				(1.0f - 2.0f * ndc.y) * fov_scale};
			ImGui::InputFloat2("screen", &screen.x);
			glm::vec3 ray = {screen, -1.0f};
			ImGui::InputFloat3("ray dir", &ray.x);
			ray = glm::normalize(transform_vec3(camera_to_world, ray, false));
			ImGui::InputFloat3("normalized", &ray.x);
			guizmo_state.ray_direction = *(minalg::float3 *)&ray;


			ImGui::End();
		}
		auto orientation = glm::toQuat(camera_to_world);
		guizmo_state.cam = tinygizmo::camera_parameters {
			.yfov = fov, .near_clip = 0.1f, .far_clip = 1000.0f,
			.position = *(minalg::float3 *)&camera.position, // quick hacky conversion
			.orientation = *(minalg::float4 *)&orientation
		};

		glm::mat4 cam_mat = perspective_mat * view_mat;

		auto trans = [&cam_mat, &win_size, &view_mat, &camera, &fov_scale](const tinygizmo::geometry_vertex &vd, glm::vec2 &p) -> bool {
			glm::vec4 v = {vd.position.x, vd.position.y, vd.position.z, 1.0f};
			glm::vec4 x = cam_mat * v;
			p = glm::vec2(x) / x.w;
			p = glm::vec2(p.x*0.5f + 0.5f, 0.5f - 0.5f*p.y);
			// std::string s = glm::to_string(v) + ", " + glm::to_string(p);
			// ImGui::Text("%s", s.c_str());
			p *= win_size;
			return x.z < 0.0f;
		};

		guizmo_ctx.render = [&renderer, &trans](const tinygizmo::geometry_mesh &r) {
			std::vector<SDL_Vertex> vertices; // keep memory allocation
			// triangles.clear();
			vertices.reserve(r.vertices.size());
			for (int i = 0; i < r.vertices.size(); i += 3) {
				SDL_Vertex a[3];
				int clipped = 0;
				for (int j = 0; j < 3; j++) {
					auto &v = r.vertices[i+j];
					glm::vec2 p;
					if (trans(v, p)) clipped++;
					auto c = minalg::vec<uint8_t, 4>(v.color*255.0f);
					a[j] = SDL_Vertex{
						.position = SDL_FPoint{p.x, p.y},
						.color = { c.x, c.y, c.z, c.w }//(SDL_Color)v.m_color
					};
				}
				if (clipped == 3) continue;

				for (int j = 0; j < 3; j++) {
					SDL_SetRenderDrawColor(renderer, a[j].color.r, a[j].color.g, a[j].color.b, a[j].color.a);
					SDL_RenderDrawPointsF(renderer, &a[j].position, 1);
					vertices.push_back(a[j]);
				}
			}
			SDL_RenderGeometry(renderer, NULL, vertices.data(), vertices.size(), (int *)r.triangles.data(), r.triangles.size()*3);
		};

		guizmo_ctx.update(guizmo_state);

		if (ImGui::Begin("Parameters")) {
			if (ImGui::BeginTabBar("params_tab_bar", ImGuiTabBarFlags_Reorderable)) {
				rerender |= interface::shape_parameters(
					shapes, triangles, guizmo_ctx, materials
				);
				rerender |= interface::camera_parameters(
					camera, movement_speed, look_around_speed, pixels,
					glm::ivec2(WINDOW_WIDTH, WINDOW_HEIGHT)
				);
				rerender |= interface::scene_parameters(tracer.scene_data);
				rerender |= interface::render_parameters(tracer.options, render_raytracing);

				ImGui::EndTabBar();
			}
		}
		ImGui::End();

		rerender |= interface::material_window(materials, shapes);
		if (rerender) {
			time_not_moved = 1;
		}

		interface::frame_time_window(frame_times, num_frame_samples, limit_fps, fps_limit, log_fps);

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

			int width = win_size.x;
			int height = win_size.y;

			int target_height = (int)(width * (1.0f / aspect_ratio));
			int target_y = (height - target_height) / 2;

			// Clear top and bottom
			SDL_Rect r = {0, 0, width, target_y};
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderFillRect(renderer, &r);
			r = {0, target_y + target_height, width, height - target_y - target_height};
			SDL_RenderFillRect(renderer, &r);

			// Render to screen
			SDL_UpdateTexture(texture, NULL, pixels.data(), RENDER_WIDTH * 4);

			SDL_Rect dstrect = {.x = 0, .y = target_y, .w = width, .h = target_height};
			SDL_RenderCopy(renderer, texture, NULL, &dstrect);
		} else {
			int width = win_size.x;
			int height = win_size.y;

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_Rect r = {0, 0, width, height};
			SDL_RenderFillRect(renderer, &r);
		}

		if (pressed_keys[SDLK_p]) {
			save_ppm("out.ppm", pixels, WINDOW_WIDTH, WINDOW_HEIGHT);
			pressed_keys[SDLK_p] = false;
		}

		// ImGui::Begin("AAA");

		guizmo_ctx.draw();

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
			if (log_fps)
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
