#include <iostream>
#include <thread>
#include <vector>
#include <optional>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_common.hpp>

#define GLM_GTX_intersect
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <SDL2/SDL.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 600

#define MAX_BOUNCE 3

typedef glm::vec<4, uint8_t, glm::defaultp> Color;

int SDL_SetRenderDrawColor(SDL_Renderer *renderer, const Color &color)
{
	return SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

struct Material
{
	Color color;
	float shine;

	Material()
	{
		this->color = glm::vec4(0);
		this->shine = 0.f;
	}

	Material(glm::vec4 color, float shine)
	{
		this->color = color;
		this->shine = shine;
	}
};

struct Sphere
{
	Material material;
	glm::vec3 position;
	float radius;

	Sphere(Material material, glm::vec3 position, float radius)
	{
		this->material = material;
		this->position = position;
		this->radius = radius;
	}
};

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

std::optional<Color> castRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, const std::vector<Sphere> &shapes, size_t numBounce = 1)
{
	glm::vec3 point, normal;
	for(auto &sphere : shapes)
	{
		if(glm::intersectRaySphere(rayOrigin, rayDirection, sphere.position, sphere.radius, point, normal))
		{
			Color color = sphere.material.color;
			
			if(numBounce < MAX_BOUNCE && sphere.material.shine != 0.f)
			{
				glm::vec3 reflected = glm::reflect(rayDirection, normal);
				std::optional<Color> newColor = castRay(point + reflected, reflected, shapes, numBounce + 1 ); // Adding reflected to prevent interference from current sphere

				if(newColor.has_value())
				{
					Color &value = newColor.value();

					// Weighted average of new color and current color
					float weight = (1.f - sphere.material.shine) / numBounce;

					color = Color(
						glm::sqrt(color.r*color.r*weight + value.r*value.r*(1.f - weight)),
						glm::sqrt(color.g*color.g*weight + value.g*value.g*(1.f - weight)),
						glm::sqrt(color.b*color.b*weight + value.b*value.b*(1.f - weight)),
						0xFF
					);
				}
			}

			// Lightness
			float dot = glm::dot(normal, glm::vec3(0, 1, 0));

			if(dot < 0.f)
			{
				color.r *= 1.f + dot;
				color.g *= 1.f + dot;
				color.b *= 1.f + dot;
			}

			return color;
		}
	}

	return std::nullopt;
}

int main(void)
{
	SDL_Renderer *renderer;
	SDL_Window *window;

	SDL_Init(SDL_INIT_VIDEO);
	SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	std::vector<Sphere> spheres = {
		Sphere(
			Material({ 0xFF, 0xFF, 0xFF, 0xFF }, 0.7f),
			{ 50, 15, -50 },
			10.f
		),
		Sphere(
			Material({ 0x34, 0x7D, 0xD0, 0xFF }, 0.2f),
			{ 100, 7, -50 },
			7.f
		),
		Sphere(
			Material({ 0xFF, 0x7D, 0xFF, 0xFF }, 1.f),
			{ 80, 15, -70 },
			14.f
		)
	};

	Camera camera = { { 0, 10, 0 }, { glm::pi<float>(), -0.6f, glm::pi<float>() } };

	float aspectRatio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
	float fieldOfView = glm::pi<float>() / 2.f; // 90 degrees

	float fieldOfViewScale = glm::tan(fieldOfView / 2.f);

	glm::mat4 cameraToWorld;

	int j = 0;

	bool running = true, changed = true;
	SDL_Event event;
	while(running)
	{
		while(SDL_PollEvent(&event) != 0)
		{
			switch(event.type)
			{
				case SDL_QUIT:
					running = false;
					break;
				case SDL_KEYUP:
					switch(event.key.keysym.sym)
					{
						case SDLK_h:
							camera.rotation.y -= glm::pi<float>() / 25.f;

							changed = true;
							j = 0;
							break;
						case SDLK_j:
							camera.rotation.x += glm::pi<float>() / 25.f;

							changed = true;
							j = 0;
							break;
						case SDLK_k:
							camera.rotation.x -= glm::pi<float>() / 25.f;

							changed = true;
							j = 0;
							break;
						case SDLK_l:
							camera.rotation.y += glm::pi<float>() / 25.f;

							changed = true;
							j = 0;
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
					changed = true;
					j = 0;
					break;
				default:
					break;
			}
		}

		if(changed)
		{
			cameraToWorld = view_matrix(camera);

			if(j < WINDOW_HEIGHT)
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

					SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);

					float distance = 100;

					if(glm::intersectRayPlane(rayOrigin, rayDirection, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), distance))
					{
						std::optional<Color> shadow = castRay(rayOrigin + rayDirection*distance, glm::vec3(0, 1, 0), spheres);

						if(shadow.has_value())
						{
							shadow.value() *= .2f;
							SDL_SetRenderDrawColor(renderer, shadow.value());
						}
					 	else SDL_SetRenderDrawColor(renderer, 0x50, 0x8E, 0xAA, 0xFF); // Plane color
					}

					std::optional<Color> output = castRay(rayOrigin, rayDirection, spheres);

					if(output.has_value())
					{
						Color &value = output.value();
						SDL_SetRenderDrawColor(renderer, value);
					}

					SDL_RenderDrawPoint(renderer, windowPos.x, windowPos.y);
				}

				j++;
			}
			else
			{
				changed = false;
				SDL_RenderPresent(renderer);
			}

		}
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();

	return EXIT_SUCCESS;
}
