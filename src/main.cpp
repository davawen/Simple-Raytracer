#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#define CL_TARGET_OPENCL_VERSION 300

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include <glm/ext/matrix_common.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

#include <SDL2/SDL.h>

#include "color.hpp"
#include "shape.hpp"
#include "tracer.hpp"

#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 562

#define RENDER_WIDTH (WINDOW_WIDTH)
#define RENDER_HEIGHT (WINDOW_HEIGHT)

#define FPS 30

struct Camera {
    glm::vec3 position;
    glm::vec3 rotation;
};

struct Movement {
    bool forward, left, right, backwards, up, down;
};

glm::mat4 view_matrix(const Camera &camera) {
    glm::mat4 view = glm::translate(glm::mat4(1.f), camera.position);
    view *= glm::eulerAngleYZX(camera.rotation.y, camera.rotation.z, camera.rotation.x);

    return view;
}

void handle_input(SDL_Event event, bool &running, Movement &movement_keys, Camera &camera, bool &save, float &fov,
                  float &fov_scale, cl_uint &time_not_moved) {
    switch (event.type) {
    case SDL_QUIT:
        running = false;
        break;
    case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_w:
            movement_keys.forward = true;
            break;
        case SDLK_s:
            movement_keys.backwards = true;
            break;
        case SDLK_d:
            movement_keys.right = true;
            break;
        case SDLK_a:
            movement_keys.left = true;
            break;
        case SDLK_c:
            movement_keys.down = true;
            break;
        case SDLK_SPACE:
            movement_keys.up = true;
            break;
        case SDLK_p:
            save = true;
            break;
        }
        break;
    case SDL_KEYUP:
        switch (event.key.keysym.sym) {
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
            movement_keys.forward = false;
            break;
        case SDLK_s:
            movement_keys.backwards = false;
            break;
        case SDLK_d:
            movement_keys.right = false;
            break;
        case SDLK_a:
            movement_keys.left = false;
            break;
        case SDLK_c:
            movement_keys.down = false;
            break;
        case SDLK_SPACE:
            movement_keys.up = false;
            break;
        }
        break;
    case SDL_MOUSEWHEEL:
        if (event.wheel.y > 0) {
            fov += glm::pi<float>() / 180.f; // 1 degree
        } else if (event.wheel.y < 0) {
            fov -= glm::pi<float>() / 180.f; // 1 degree
        }

        fov_scale = glm::tan(fov / 2.f);
        time_not_moved = 1;
        break;
    case SDL_MOUSEMOTION:
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

    SDL_Init(SDL_INIT_VIDEO);

    SDL_CreateWindowAndRenderer(WINDOW_WIDTH, WINDOW_HEIGHT, 0, &window, &renderer);

    SDL_SetRelativeMouseMode(SDL_TRUE);

    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    SDL_Texture *texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, RENDER_WIDTH, RENDER_HEIGHT);

    std::vector<Shape> shapes;

    Box box = {Box(Material(color::from_RGB(0x4f, 0x12, 0x13), 0.9f), {10, 20, -90.0f}, {20, 12, 20})};
    auto box_t = box.to_triangles();
    std::transform(box_t.begin(), box_t.end(), std::back_inserter(shapes), [](Triangle &t) { return Shape(t); });

    Plane ground_plane = Plane(Material(color::from_RGB(0xDF, 0x2F, 0x00), 0.0f), {0, 0, 0}, {0, 1, 0});
    shapes.push_back(ground_plane);

    for (size_t i = 0; i < 12; i++) {
        auto sphere = Sphere(Material(color::from_RGB(rand() % 256, rand() % 256, rand() % 256), (float)i / 11.0f),
                             {i * 18.0f, 20.0f, -50.0f}, 8.0f);

        shapes.push_back(std::move(sphere));
    }

    Camera camera = {{0, 10, 0}, {glm::pi<float>(), -0.6f, glm::pi<float>()}};

    float aspect_ratio = static_cast<float>(RENDER_WIDTH) / RENDER_HEIGHT;

    float fov = glm::pi<float>() / 2.f; // 90 degrees
    float fov_scale = glm::tan(fov / 2.f);

    glm::mat4 camera_to_world;

    Tracer tracer(RENDER_WIDTH, RENDER_HEIGHT);
    Tracer::RenderData options(RENDER_WIDTH, RENDER_HEIGHT);

	tracer.scene_data.horizon_color = VEC3TOCL(color::from_hex(0x91c8f2));
	tracer.scene_data.zenith_color = VEC3TOCL(color::from_hex(0x40aff9));
	tracer.scene_data.ground_color = VEC3TOCL(color::from_hex(0x777777));
	tracer.scene_data.sun_focus = 25.0f;
	tracer.scene_data.sun_color = VEC3TOCL(color::from_hex(0xddff00) * 3.0f);
	tracer.scene_data.sun_direction = VEC3TOCL(glm::normalize(glm::vec3(1.0, -1.0, 0.0)));

    std::vector<uint8_t> pixels(RENDER_WIDTH * RENDER_HEIGHT * 4);

    bool running = true;
    Movement movement_keys{};

    int tick = 0;
    cl_uint time_not_moved = 1;
    double average = 0.;
    bool save = false;

    SDL_Event event;
    while (running) {
        double start = now();

        while (SDL_PollEvent(&event) != 0) {
            handle_input(event, running, movement_keys, camera, save, fov, fov_scale, time_not_moved);
        }

        {
            const glm::vec3 movement = glm::normalize(
                glm::vec3(camera_to_world * glm::vec4(movement_keys.right - movement_keys.left, 0,
                                                      movement_keys.forward - movement_keys.backwards, 0)) +
                glm::vec3(0, movement_keys.up - movement_keys.down,
                          0)); // 0 at the end nullify's translation

            if (!glm::all(glm::isnan(movement)) && !glm::isNull(movement, glm::epsilon<float>())) {
                camera.position += movement;
                time_not_moved = 1;
            }
        }

        camera_to_world = view_matrix(camera);

        if (time_not_moved == 1) {
            tracer.clear_canvas();
            options.num_samples = 3;
        } else {
            options.num_samples = 6;
        }

        tracer.update_scene(shapes);

        options.aspect_ratio = aspect_ratio;
        options.fov_scale = fov_scale;
        options.set_matrix(camera_to_world);
        options.time = start * 1000;
        options.tick = tick;

        tracer.render(time_not_moved, options, pixels);

        if (save) {
            save_ppm(pixels);
            save = false;
        }

        SDL_UpdateTexture(texture, NULL, pixels.data(), RENDER_WIDTH * 4);

        const SDL_Rect dstRect = {.x = 0, .y = 0, .w = WINDOW_WIDTH, .h = WINDOW_HEIGHT};
        SDL_RenderCopy(renderer, texture, NULL, &dstRect);
        SDL_RenderPresent(renderer);

        average += now() - start;
        tick++;
        time_not_moved++;

        if (tick == 60) {
            std::cout << "Average time: " << average * 1000.0 / 60.0 << " ms\n";
            tick = 0;
            average = 0.;
        }

        // Limit framerate
        double loop_duration = (now() - start);
        if (loop_duration < 1. / FPS)
            SDL_Delay((1. / FPS - loop_duration) * 1000);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return EXIT_SUCCESS;
}
