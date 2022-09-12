#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
#include <optional>
#include <utility>
#include <queue>
#include <functional>
#include <condition_variable>
#include <random>
#include <fstream>

#include <sys/time.h>

#define CL_TARGET_OPENCL_VERSION 300
#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>

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


long now()
{
	timeval curr;
	gettimeofday(&curr, 0);

	return curr.tv_sec * 1000000 + curr.tv_usec;
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
			Material(color::from_RGB(0x4f, 0x12, 0x13), color::gray, 10.f),
			{ 10, 20, -110 },
			{ 20, 12, 20 }
	   )
	};
	box.material.type = Material::Type::REFLECTIVE;
	auto box_t = box.to_triangles();
	std::transform(box_t.begin(), box_t.end(), std::back_inserter(shapes), [](Triangle &t) { return Shape(t); });

	Plane groundPlane = Plane(
		Material(color::from_RGB( 0xDF, 0x2F, 0x00 ), color::black, 0.f),
		{ 0, 0, 0 },
		{ 0, 1, 0 }
	);
	shapes.push_back(groundPlane);

	for(size_t i = 0; i < 30; i++)
	{
		shapes.push_back(Shape(
			Sphere(
				Material(color::from_RGB(rand() % 256, rand() % 256, rand() % 256), color::white, (rand() % 1000) / 100.f + 2.f),
				{ rand() % 300, rand() % 20 + 15, rand() % 300 },
				(rand() % 1000) / 100.f + 5.f
			)
		));

		if(rand() % 3 == 0) shapes.back().shape.sphere.material.type = Material::Type::REFLECTIVE;
	}

	Camera camera = { { 0, 10, 0 }, { glm::pi<float>(), -0.6f, glm::pi<float>() } };

	glm::vec3 lightSource(0, 10000, 0);

	float aspectRatio = static_cast<float>(RENDER_WIDTH) / RENDER_HEIGHT;
	float fieldOfView = glm::pi<float>() / 2.f; // 90 degrees

	float fieldOfViewScale = glm::tan(fieldOfView / 2.f);

	glm::mat4 cameraToWorld;

	Tracer tracer(RENDER_WIDTH, RENDER_HEIGHT);

	Tracer::RenderData options(RENDER_WIDTH, RENDER_HEIGHT);

	std::vector<uint8_t> pixels(RENDER_WIDTH * RENDER_HEIGHT * 4);

	bool running = true;

#if 0
	std::atomic<size_t> doneWorkers = 0; // hack
	auto renderFrame = [&doneWorkers, &pixels, &lightSource, &shapes, &cameraToWorld, &aspectRatio, &fieldOfViewScale](const int workerIndex, const int numWorker) -> void
	{
	 	std::mt19937 generator;

	 	int j = workerIndex;

	 	while(j < RENDER_HEIGHT)
	 	{
	 		for(int i = 0; i < RENDER_WIDTH; i++)
	 		{

	 			glm::vec2 windowPos(i, j); // Raster space coordinates
	 			glm::vec2 ndcPos((windowPos.x + .5f) / RENDER_WIDTH, (windowPos.y + .5f) / RENDER_HEIGHT); // Normalized coordinates

	 			glm::vec2 screenPos((2.f * ndcPos.x - 1.f) * aspectRatio * fieldOfViewScale, (1.f - 2.f * ndcPos.y) * fieldOfViewScale); // Screen space coordinates (invert y axis)

	 			glm::vec3 cameraPos(screenPos.x, screenPos.y, 1);

	 			glm::vec3 rayOrigin = cameraToWorld * glm::vec4(0, 0, 0, 1);

	 			// vec4 with 0 at the end is only affected by rotation, not translation
	 			// Only normalize 3d components
	 			glm::vec3 rayDirection = glm::normalize((cameraToWorld * glm::vec4(cameraPos, 0)).xyz()); 

	 			// SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
	 			Color color = ray_cast(rayOrigin, rayDirection, shapes/*, lightSource, generator*/);
	 			color *= 255.f; // Cast into 0-255 range

	 			// SDL_RenderDrawPoint(renderer, windowPos.x, windowPos.y);
	 			size_t idx = j*RENDER_WIDTH*4 + i*4;
	 			
	 			// ARGB
	 			pixels[idx] = 0xFF;
	 			pixels[idx + 1] = static_cast<uint8_t>(color.r);
	 			pixels[idx + 2] = static_cast<uint8_t>(color.g);
	 			pixels[idx + 3] = static_cast<uint8_t>(color.b);
	 		}

	 		j += numWorker;
	 	}

	 	doneWorkers++;
	};

	size_t numThreads = glm::min((int)std::thread::hardware_concurrency(), 8);
	boost::asio::thread_pool pool(numThreads);
#endif

	struct
	{
		bool forward, left, right, backwards, up, down;
	} movementKeys = { false, false, false, false, false, false };

	int tick = 0;
	double average = 0.;
	bool save = false;

	SDL_Event event;
	while(running)
	{
		long loopStart = now();
		double loopStartSec = loopStart / 1'000'000.;

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
						case SDLK_z:
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
						case SDLK_z:
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
					if(event.wheel.y > 0)
					{
						fieldOfView += glm::pi<float>() / 180.f; // 1 degree
					}
					else if(event.wheel.y < 0)
					{
						fieldOfView -= glm::pi<float>() / 180.f; // 1 degree
					}

					fieldOfViewScale = glm::tan(fieldOfView / 2.f);
					break;
				case SDL_MOUSEMOTION:
					if(event.motion.xrel != 0)
					{
						camera.rotation.y += glm::pi<float>() * event.motion.xrel / 1000.f * fieldOfViewScale;
					}

					if(event.motion.yrel != 0)
					{
						camera.rotation.x += glm::pi<float>() * event.motion.yrel / 1000.f * fieldOfViewScale;
					}
					break;
				default:
					break;
			}
		}

		for(auto &s : shapes) {
			if(s.type == SHAPE_SPHERE) {
				auto &sphere = s.shape.sphere;
				sphere.position.y += glm::sin(loopStartSec + sphere.position.x + sphere.position.z)*.2f;
			}
		}
		// lightSource = glm::rotateZ(lightSource, 0.01f);

		{
			const glm::vec3 movement = glm::normalize(glm::vec3(cameraToWorld * glm::vec4(movementKeys.right - movementKeys.left, 0, movementKeys.forward - movementKeys.backwards, 0)) + glm::vec3(0, movementKeys.up - movementKeys.down, 0)); // 0 at the end nullify's translation

			if(!glm::all(glm::isnan(movement)))
			{
				camera.position += movement;
			}
		}

		cameraToWorld = view_matrix(camera);

		// for(size_t i = 0; i < numThreads; i++)
		// {
		// 	boost::asio::post(pool, boost::bind<void>(renderFrame, i, numThreads));
		// }

		// while(doneWorkers < numThreads){}

		double start__ = now();

		//if(options.stepsNotMoved >= 0)
		{
			tracer.update_scene(shapes, lightSource);

			if(!save) {
				options.numSamples = 4;
			}
			else {
				options.numSamples = 3048;
			}

			options.aspectRatio = aspectRatio;
			options.fieldOfViewScale = fieldOfViewScale;
			options.set_matrix(cameraToWorld);
			options.time = loopStart / 1000;
			options.tick = tick;

			tracer.render(options, pixels);

			if(save) {
				save_ppm(pixels);
				save = false;
			}

			//doneWorkers = 0;

			SDL_UpdateTexture(texture, NULL, pixels.data(), RENDER_WIDTH * 4);

			const SDL_Rect dstRect = { .x = 0, .y = 0, .w = WINDOW_WIDTH, .h = WINDOW_HEIGHT };
			SDL_RenderCopy(renderer, texture, NULL, &dstRect);
			SDL_RenderPresent(renderer);
		}

		average += now() - start__;
		tick++;

		if(tick == 60)
		{
			std::cout << "Average time: " << average/60. << " µs\n";
			tick = 0;
			average = 0.;
		}

		// Limit framerate 
		long loopDuration = (now() - loopStart);
		if(loopDuration < (1'000'000 / FPS)) SDL_Delay((1'000'000 / FPS - loopDuration) / 1000);
	}

	//pool.join();
	

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();

	return EXIT_SUCCESS;
}
