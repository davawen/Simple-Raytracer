#include <iostream>
#include <vector>
#include <stdexcept>

#define CL_TARGET_OPENCL_VERSION 300
#include <boost/compute/core.hpp>
#include <boost/compute/utility.hpp>

#include "shapes.hpp"

namespace compute = boost::compute;

class Tracer
{
private:
    compute::device device;
	compute::context context;

	compute::program program;
	compute::kernel kernel;
	compute::command_queue queue;

	compute::buffer bufferScene;

	compute::buffer bufferOutput;

	struct CL_Sphere
	{
		cl_float3 position;
		cl_float radius;

		CL_Sphere(glm::vec3 pos, float radius)
		{
			this->position.x = pos.x;
			this->position.y = pos.y;
			this->position.z = pos.z;

			this->radius = radius;
		}
	};

	struct CL_SceneData
	{
		cl_int numSpheres;
	} sceneData;
public:

	struct CL_RenderData
	{
		cl_int width, height;
		cl_float aspectRatio, fieldOfViewScale;

		cl_float4 cameraToWorldMatrix[4];

		CL_RenderData(int width, int height)
		{
			this->width = width;
			this->height = height;
		}

		void set_matrix(const glm::mat4 &cameraToWorld)
		{
			for(size_t i = 0; i < 4; i++)
			{
				cameraToWorldMatrix[i] = {{ cameraToWorld[i].x, cameraToWorld[i].y, cameraToWorld[i].z, cameraToWorld[i].w }};
			}
		}
	};

	Tracer(const int width, const int height, const std::string &kernelFile)
	{
		// Get the default device
		device = compute::system::default_device();

		// Create a context from the device
		context = compute::context(device);

		// Creates the program from source
		program = compute::program::create_with_source_file(kernelFile, context);
		program.build();


		// Creates the kernel
		kernel = compute::kernel(program, "render");

		// Create command queue
		queue = compute::command_queue(context, device);

		bufferScene = compute::buffer(context, 0);
		bufferOutput = compute::buffer(context, sizeof(cl_uchar4) * width * height);

		// Set arguments
		kernel.set_arg(2, bufferOutput);
	}

	void update_scene(const std::vector<Shape *> &shapes)
	{
		std::vector<CL_Sphere> positions;//(shapes.size());

		for(size_t i = 0; i < shapes.size(); i++)
		{
			Sphere *curr = reinterpret_cast<Sphere *>(shapes[i]);
			positions.push_back(CL_Sphere(curr->position, curr->radius));
		}
		
		// Only create new buffer if previous one is too small
		if(bufferScene.size() < sizeof(CL_Sphere) * positions.size())
			bufferScene = compute::buffer(context, sizeof(CL_Sphere) * positions.size()); // NOTE: Assigning releases old buffer, so this does not cause a memory leak

		queue.enqueue_write_buffer(bufferScene, 0, sizeof(CL_Sphere) * positions.size(), positions.data());
		kernel.set_arg(3, bufferScene); // Point to new buffer

		sceneData.numSpheres = positions.size();
		kernel.set_arg(1, sizeof(CL_SceneData), &sceneData);
	}

	void render(CL_RenderData &renderData, std::vector<uint8_t> &output)
	{
		// if(bufferScene.get_host_ptr() == NULL)
		// {
		// 	std::cout << bufferScene.get_host_ptr() << "\n" << bufferOutput.get_host_ptr() << "\n";
		// 	throw std::runtime_error("Scene is not initialized");
		// }

		// kernel.set_arg(4, sizeof(Square), &square);
		kernel.set_arg(0, sizeof(CL_RenderData), &renderData);

		// Run kernel
		queue.enqueue_1d_range_kernel(kernel, 0, renderData.width * renderData.height, 0);

		// Transfer result from gpu buffer to array
		queue.enqueue_read_buffer(bufferOutput, 0, sizeof(cl_uchar4) * renderData.width * renderData.height, output.data());
	}
};
