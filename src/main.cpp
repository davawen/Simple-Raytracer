#include <cstdint>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>

#define CL_TARGET_OPENCL_VERSION 300

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_common.hpp>

#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <SDL2/SDL.h>

#include "color.hpp"
#include "shape.hpp"
#include "ray_cast.hpp"
#include "tracer.hpp"

#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 562

#define RENDER_WIDTH (WINDOW_WIDTH)
#define RENDER_HEIGHT (WINDOW_HEIGHT)

#define FPS 30

int SDL_SetRenderDrawColor(SDL_Renderer *renderer, const Color &color)
{
	return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 0xFF);
}

struct Camera
{
	glm::vec3 position;
	glm::vec3 rotation;
};

glm::mat4 view_matrix(const Camera &camera)
{
	glm::mat4 view = glm::translate(glm::mat4(1.f), camera.position);
	view *= glm::eulerAngleYZX(camera.rotation.y, camera.rotation.z, camera.rotation.x);
	
	return view;
}

inline glm::vec3 uniform_sample_hemisphere(const float &r1, const float &r2) 
{ 
    // cos(theta) = r1 = y
    // cos^2(theta) + sin^2(theta) = 1 -> sin(theta) = srtf(1 - cos^2(theta))
    float sinTheta = glm::sqrt(1 - r1 * r1); 
    float phi = 2 * M_PI * r2; 
    float x = sinTheta * glm::cos(phi); 
    float z = sinTheta * glm::sin(phi); 

    return glm::vec3(x, r1, z); 
}

void save_ppm(std::vector<uint8_t> &pixels) {
	std::ofstream file;
	file.open("out.ppm", std::ios::binary | std::ios::out);
	std::string header = "P6 ";
	header += std::to_string(WINDOW_WIDTH) + ' ' + std::to_string(WINDOW_HEIGHT) + ' ';
	header += "255\n";
	file.write(header.c_str(), header.size());

	for(size_t i = 0; i < pixels.size(); i += 4) {
		uint8_t *p = &pixels[i]; // ARGB

		file.write((char *)p + 1, 3);
	}
}


double now() {
	return std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1'000'000'000.0;
}

int main(int argc, char **)
{
	if(argc != 1)
	{
		printf("Usage: tracer");
		return -1;
	}

	SDL_Renderer *renderer;
	SDL_Window *window;

	SDL_Init(SDL_INIT_VIDEO);
	
	SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer);

	SDL_SetRelativeMouseMode(SDL_TRUE);

	SDL_RendererInfo info;
	SDL_GetRendererInfo(renderer, &info);
	
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH, RENDER_HEIGHT);

	std::vector<Shape> shapes;

	Box box = {
		Box(
			Material(color::from_RGB(0x4f, 0x12, 0x13), 1.0f),
			{ 10, 20, -110 },
			{ 20, 12, 20 }
	   )
	};
	auto box_t = box.to_triangles();
	std::transform(box_t.begin(), box_t.end(), std::back_inserter(shapes), [](Triangle &t) { return Shape(t); });

	Plane groundPlane = Plane(
		Material(color::from_RGB( 0xDF, 0x2F, 0x00 ), 0.0f),
		{ 0, 0, 0 },
		{ 0, 1, 0 }
	);
	shapes.push_back(groundPlane);

	for(size_t i = 0; i < 30; i++)
	{
		auto sphere = Sphere(
			Material(color::from_RGB(rand() % 256, rand() % 256, rand() % 256), (rand() % 1000) / 1000.f),
			{ rand() % 300, rand() % 20 + 15, rand() % 300 },
			(rand() % 1000) / 100.f + 5.f
		);

		if (rand() % 3 == 0) {
			sphere.material.smoothness = 1.0;
		}

		shapes.push_back(std::move(sphere));
	}

	Camera camera = { { 0, 10, 0 }, { glm::pi<float>(), -0.6f, glm::pi<float>() } };

	float aspectRatio = static_cast<float>(RENDER_WIDTH) / RENDER_HEIGHT;
	float fieldOfView = glm::pi<float>() / 2.f; // 90 degrees

	float fieldOfViewScale = glm::tan(fieldOfView / 2.f);

	glm::mat4 cameraToWorld;

	Tracer tracer(RENDER_WIDTH, RENDER_HEIGHT);

	Tracer::RenderData options(RENDER_WIDTH, RENDER_HEIGHT);

	std::vector<uint8_t> pixels(RENDER_WIDTH * RENDER_HEIGHT * 4);

	bool running = true;

	struct
	{
		bool forward, left, right, backwards, up, down;
	} movementKeys = { false, false, false, false, false, false };

	int tick = 0;
	cl_uint time_not_moved = 1;
	double average = 0.;
	bool save = false;

	double program_start = now();

	SDL_Event event;
	while(running)
	{
		double start = now();

		while(SDL_PollEvent(&event) != 0)
		{
			switch(event.type)
			{
				case SDL_QUIT:
					running = false;
					break;
				case SDL_KEYDOWN:
					switch(event.key.keysym.sym)
					{
						case SDLK_w:
							movementKeys.forward = true;
							break;
						case SDLK_s:
							movementKeys.backwards = true;
							break;
						case SDLK_d:
							movementKeys.right = true;
							break;
						case SDLK_a:
							movementKeys.left = true;
							break;
						case SDLK_c:
							movementKeys.down = true;
							break;
						case SDLK_SPACE:
							movementKeys.up = true;
							break;
						case SDLK_p:
							save = true;
							break;
					}
					break;
				case SDL_KEYUP:
					switch(event.key.keysym.sym)
					{
						case SDLK_h:
							camera.rotation.y -= glm::pi<float>() / 25.f;
							break;
						case SDLK_j:
							camera.rotation.x += glm::pi<float>() / 25.f;
							break;
						case SDLK_k:
							camera.rotation.x -= glm::pi<float>() / 25.f;
							break;
						case SDLK_l:
							camera.rotation.y += glm::pi<float>() / 25.f;
							break;
						case SDLK_w:
							movementKeys.forward = false;
							break;
						case SDLK_s:
							movementKeys.backwards = false;
							break;
						case SDLK_d:
							movementKeys.right = false;
							break;
						case SDLK_a:
							movementKeys.left = false;
							break;
						case SDLK_c:
							movementKeys.down = false;
							break;
						case SDLK_SPACE:
							movementKeys.up = false;
							break;
					}
					break;
				case SDL_MOUSEWHEEL:
					if(event.wheel.y > 0) {
						fieldOfView += glm::pi<float>() / 180.f; // 1 degree
					}
					else if(event.wheel.y < 0) {
						fieldOfView -= glm::pi<float>() / 180.f; // 1 degree
					}

					fieldOfViewScale = glm::tan(fieldOfView / 2.f);
					time_not_moved = 1;
					break;
				case SDL_MOUSEMOTION:
					if(event.motion.xrel != 0) {
						camera.rotation.y += glm::pi<float>() * event.motion.xrel / 1000.f * fieldOfViewScale;
					}

					if(event.motion.yrel != 0) {
						camera.rotation.x += glm::pi<float>() * event.motion.yrel / 1000.f * fieldOfViewScale;
					}
					time_not_moved = 1;
					break;
				default:
					break;
			}
		}

		// for(auto &s : shapes) {
		// 	if(s.type == SHAPE_SPHERE) {
		// 		auto &sphere = s.shape.sphere;
		// 		sphere.position.y += glm::sin(loopStartSec + sphere.position.x + sphere.position.z)*.2f;
		// 	}
		// }
		// lightSource = glm::rotateZ(lightSource, 0.01f);

		{
			const glm::vec3 movement = glm::normalize(glm::vec3(cameraToWorld * glm::vec4(movementKeys.right - movementKeys.left, 0, movementKeys.forward - movementKeys.backwards, 0)) + glm::vec3(0, movementKeys.up - movementKeys.down, 0)); // 0 at the end nullify's translation

			if(!glm::all(glm::isnan(movement)) && !glm::isNull(movement, glm::epsilon<float>())) {
				camera.position += movement;
				time_not_moved = 1;
			}
		}

		cameraToWorld = view_matrix(camera);

		if(time_not_moved == 1) {
			tracer.clear_canvas();
			options.numSamples = 3;
		} else {
			options.numSamples = 6;
		}

		tracer.update_scene(shapes, glm::vec3(0.0, 1.0, 0.0));

		options.aspectRatio = aspectRatio;
		options.fieldOfViewScale = fieldOfViewScale;
		options.set_matrix(cameraToWorld);
		options.time = start * 1000;
		options.tick = tick;

		tracer.render(time_not_moved, options, pixels);

		if(save) {
			save_ppm(pixels);
			save = false;
		}

		SDL_UpdateTexture(texture, NULL, pixels.data(), RENDER_WIDTH * 4);

		const SDL_Rect dstRect = { .x = 0, .y = 0, .w = WINDOW_WIDTH, .h = WINDOW_HEIGHT };
		SDL_RenderCopy(renderer, texture, NULL, &dstRect);
		SDL_RenderPresent(renderer);

		average += now() - start;
		tick++;
		time_not_moved++;

		if(tick == 60) {
			std::cout << "Average time: " << average * 1000.0 / 60.0 << " ms\n";
			tick = 0;
			average = 0.;
		}

		// Limit framerate 
		double loop_duration = (now() - start);
		if(loop_duration < 1./FPS) SDL_Delay((1./FPS - loop_duration) * 1000);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();

	return EXIT_SUCCESS;
}
