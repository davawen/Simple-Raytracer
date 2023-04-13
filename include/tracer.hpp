#include <iostream>
#include <vector>
#include <stdexcept>

#define CL_TARGET_OPENCL_VERSION 300
#include <boost/compute/core.hpp>
#include <boost/compute/utility.hpp>
#include <boost/compute/buffer.hpp>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include <glm/gtx/rotate_vector.hpp>

#include "shape.hpp"

namespace compute = boost::compute;

#define VEC3TOCL(v) (cl_float3({{ (v).x, (v).y, (v).z }}))
#define VEC4TOCL(v) (cl_float4({{ (v).x, (v).y, (v).z, (v).w }}))

class Tracer {
private:
    compute::device device;
	compute::context context;

	compute::program program;
	compute::kernel kernel;
	compute::kernel average_kernel;

	compute::command_queue queue;

	compute::buffer renderCanvas;
	compute::buffer renderOutput;

	struct SceneData {
		cl_int numShapes;

		cl_float3 lightSource;
	} sceneData;

	struct {
		compute::buffer bufferShapes;

		std::vector<Triangle> triangles;
	} shapes;
public:

	struct RenderData
	{
		cl_int width, height;
		cl_int numSamples;
		cl_float aspectRatio, fieldOfViewScale;

		cl_float4 cameraToWorldMatrix[4];

		cl_uint time;
		cl_uint tick;

		RenderData(int width, int height) {
			this->width = width;
			this->height = height;
		}

		void set_matrix(const glm::mat4 &cameraToWorld) {
			for(size_t i = 0; i < 4; i++) {
				//cameraToWorldMatrix[i] = {{ cameraToWorld[i].x, cameraToWorld[i].y, cameraToWorld[i].z, cameraToWorld[i].w }};
				cameraToWorldMatrix[i] = VEC4TOCL(cameraToWorld[i]);
			}
		}
	};

	Tracer(const int width, const int height);

	void update_scene(const std::vector<Shape> &inputShape, const glm::vec3 &lightSource);

	void clear_canvas();
	void render(cl_uint ticks_stopped, RenderData &renderData, std::vector<uint8_t> &output);
};
