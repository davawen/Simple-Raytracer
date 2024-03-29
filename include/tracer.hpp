#pragma once

#include <vector>

#define CL_TARGET_OPENCL_VERSION 200

#include <boost/compute/buffer.hpp>
#include <boost/compute/core.hpp>
#include <boost/compute/utility.hpp>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stb_image.h>

#include "color.hpp"
#include "material.hpp"
#include "shape.hpp"

namespace compute = boost::compute;

#define VEC3TOCL(v) (cl_float3({{(v).x, (v).y, (v).z}}))
#define VEC4TOCL(v) (cl_float4({{(v).x, (v).y, (v).z, (v).w}}))

class Tracer {
  private:
    compute::device device;
    compute::context context;

    compute::program program;
    compute::kernel kernel;
    compute::kernel average_kernel;

    compute::command_queue queue;

    compute::buffer render_canvas;
    compute::buffer render_output;

	compute::buffer buffer_shapes;
	compute::buffer buffer_triangles;
	compute::buffer buffer_materials;

	compute::image2d skybox;
	compute::image_sampler sampler;

  public:
    struct RenderData {
        cl_int width, height;
        cl_int num_samples;
        cl_int num_bounces;
        cl_float aspect_ratio;
		cl_float fov_scale;
		bool show_normals;

		alignas(cl_float4) glm::mat4 camera_to_world;

        cl_uint time;
        cl_uint tick;

        RenderData(int width, int height) {
            this->width = width;
            this->height = height;
			this->num_samples = 4;
			this->num_bounces = 10;
        }
    } options;

    struct SceneData {
        cl_int num_shapes;
		cl_float sun_focus;
		cl_float sun_intensity;

		alignas(cl_float3) Color horizon_color;
		alignas(cl_float3) Color zenith_color;
		alignas(cl_float3) Color ground_color;
		alignas(cl_float3) Color sun_color;

        cl_float3 sun_direction;
    } scene_data;

    Tracer(const int width, const int height);

    void update_scene(const std::vector<Shape> &shapes, const std::vector<Triangle> &triangles, const std::vector<Material> &materials);

    void clear_canvas();
    void render(cl_uint ticks_stopped, std::vector<uint8_t> &output);
};
