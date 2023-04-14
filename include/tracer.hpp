#include <iostream>
#include <stdexcept>
#include <vector>

#define CL_TARGET_OPENCL_VERSION 300
#include <boost/compute/buffer.hpp>
#include <boost/compute/core.hpp>
#include <boost/compute/utility.hpp>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include <glm/gtx/rotate_vector.hpp>

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

    struct {
        compute::buffer buffer_shapes;

        std::vector<Triangle> triangles;
    } shapes;

  public:
    struct RenderData {
        cl_int width, height;
        cl_int num_samples;
        cl_int num_bounces;
        cl_float aspect_ratio, fov_scale;

        cl_float4 camera_to_world[4];

        cl_uint time;
        cl_uint tick;

        RenderData(int width, int height) {
            this->width = width;
            this->height = height;
			this->num_samples = 4;
			this->num_bounces = 10;
        }

        void set_matrix(const glm::mat4 &camera_to_world_matrix) {
            for (size_t i = 0; i < 4; i++) {
                camera_to_world[i] = VEC4TOCL(camera_to_world_matrix[i]);
            }
        }
    };

    struct SceneData {
        cl_int num_shapes;

		cl_float3 horizon_color;
		cl_float3 zenith_color;
		cl_float3 ground_color;
		cl_float sun_focus;
		cl_float3 sun_color;
        cl_float3 sun_direction;
    } scene_data;

    Tracer(const int width, const int height);

    void update_scene(const std::vector<Shape> &inputShape);

    void clear_canvas();
    void render(cl_uint ticks_stopped, RenderData &renderData, std::vector<uint8_t> &output);
};
