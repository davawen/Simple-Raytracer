#if 1

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

#include <boost/bind/bind.hpp>
#include <boost/asio.hpp>

#include <sys/time.h>

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

#endif

#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 562

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


long now()
{
	timeval curr;
	gettimeofday(&curr, 0);

	return curr.tv_sec * 1000000 + curr.tv_usec;
}

int main(int argc, char **argv)
{
	if(argc != 2)
	{
		printf("Usage: tracer [path/to/render.cl]");
		return -1;
	}

	std::string file = argv[1];

	SDL_Renderer *renderer;
	SDL_Window *window;

	SDL_Init(SDL_INIT_VIDEO);
	
	SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer);
	SDL_SetRelativeMouseMode(SDL_TRUE);

	SDL_RendererInfo info;
	SDL_GetRendererInfo(renderer, &info);
	
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);

	struct
	{
		std::vector<Sphere> spheres/* = {
			Sphere(
					Material(color::gray, color::white, 10.f),
					{ 50, 15, -50 },
					10.f
				  ),
			Sphere(
					Material(color::from_RGB( 0x34, 0x7D, 0xD0 ), color::white, 10.f),
					{ 100, 7, -50 },
					7.f
				  ),
			Sphere(
					Material(color::from_RGB( 0xFF, 0x7D, 0xFF ), color::white, 10.f),
					{ 80, 15, -70 },
					14.f
				  )
		}*/;

		std::vector<Plane> planes = {
			Plane(
					Material(color::from_RGB( 0xbf, 0x5e, 0x22 ), color::black, 0.f),
					{ 0, 0, 0 },
					{ 0, 1, 0 }
				 )
		};
	} shapes;

	for(size_t i = 0; i < 100; i++)
	{
		shapes.spheres.push_back(
			Sphere(
				Material(color::from_hex(0x6d16e0), color::white, 10.f),
				{ (i % 10) * 24., 15, (i / 10) * -24. },
				10.f
			)
		);
	}

	Camera camera = { { 0, 10, 0 }, { glm::pi<float>(), -0.6f, glm::pi<float>() } };

	glm::vec3 lightSource(0, 100, 0);

	float aspectRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
	float fieldOfView = glm::pi<float>() / 2.f; // 90 degrees

	float fieldOfViewScale = glm::tan(fieldOfView / 2.f);

	glm::mat4 cameraToWorld;

	Tracer tracer(WINDOW_WIDTH, WINDOW_HEIGHT, file);
	tracer.update_scene(shapes.spheres, shapes.planes, lightSource);

	Tracer::CL_RenderData options(WINDOW_WIDTH, WINDOW_HEIGHT);

	std::vector<uint8_t> pixels(WINDOW_WIDTH * WINDOW_HEIGHT * 4);

	bool running = true;

#if 0
	std::atomic<size_t> doneWorkers = 0; // hack
	auto renderFrame = [&doneWorkers, &pixels, &lightSource, &shapes, &cameraToWorld, &aspectRatio, &fieldOfViewScale](const int workerIndex, const int numWorker) -> void
	{
	 	std::mt19937 generator;

	 	int j = workerIndex;

	 	while(j < WINDOW_HEIGHT)
	 	{
	 		for(int i = 0; i < WINDOW_WIDTH; i++)
	 		{

	 			glm::vec2 windowPos(i, j); // Raster space coordinates
	 			glm::vec2 ndcPos((windowPos.x + .5f) / WINDOW_WIDTH, (windowPos.y + .5f) / WINDOW_HEIGHT); // Normalized coordinates

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
	 			size_t idx = j*WINDOW_WIDTH*4 + i*4;
	 			
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

		for(auto &sphere : shapes.spheres) sphere.position.y += glm::sin(loopStartSec + sphere.position.x)*.2f;
		// lightSource = glm::rotateZ(lightSource, 0.01f);

		{
			const glm::vec3 movement = glm::normalize(glm::vec3(cameraToWorld * glm::vec4(movementKeys.right - movementKeys.left, movementKeys.up - movementKeys.down, movementKeys.forward - movementKeys.backwards, 0))); // 0 at the end nullify's translation

			if(!glm::all(glm::isnan(movement))) camera.position += movement;
		}

		cameraToWorld = view_matrix(camera);

		// for(size_t i = 0; i < numThreads; i++)
		// {
		// 	boost::asio::post(pool, boost::bind<void>(renderFrame, i, numThreads));
		// }

		// while(doneWorkers < numThreads){}
		
		double start__ = now();
		tracer.update_scene(shapes.spheres, shapes.planes, lightSource);
		average += now() - start__;
		tick++;

		if(tick == 60)
		{
			std::cout << "Average time: " << average/60. << " µs\n";
			tick = 0;
			average = 0.;
		}

		options.aspectRatio = aspectRatio;
		options.fieldOfViewScale = fieldOfViewScale;
		options.set_matrix(cameraToWorld);

		tracer.render(options, pixels);

		//doneWorkers = 0;

		SDL_UpdateTexture(texture, NULL, pixels.data(), WINDOW_WIDTH * 4);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);

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
