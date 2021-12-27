#include <iostream>
#include <vector>
#include <stdexcept>

#define CL_TARGET_OPENCL_VERSION 300
#include <boost/compute/core.hpp>
#include <boost/compute/utility.hpp>
#include <boost/compute/buffer.hpp>

#include "shape.hpp"

namespace compute = boost::compute;

#define VEC3TOCL(v) (cl_float3({{ (v).x, (v).y, (v).z }}))
#define VEC4TOCL(v) (cl_float4({{ (v).x, (v).y, (v).z, (v).w }}))

class Tracer
{
private:
    compute::device device;
	compute::context context;

	compute::program program;
	compute::kernel kernel;
	compute::command_queue queue;

	compute::buffer bufferSpheres;
	compute::buffer bufferPlanes;

	compute::buffer bufferOutput;

	struct CL_Material
	{
		cl_float3 color;
		cl_float3 specular;
		cl_float specularExponent;

		CL_Material()
		{
			this->color = VEC3TOCL(glm::vec3(0.f));
			this->specular = VEC3TOCL(glm::vec3(0.f));
			this->specularExponent = 0.f;
		}

		CL_Material(const Material &material)
		{
			this->color = VEC3TOCL(material.color);
			this->specular = VEC3TOCL(material.specular);
			this->specularExponent = material.specularExponent;
		}
	};

	struct CL_Sphere
	{
		CL_Material material;
		cl_float3 position;
		cl_float radius;

		CL_Sphere(const Material &material, const glm::vec3 &pos, const float radius)
		{
			this->material = CL_Material(material);

			this->position = VEC3TOCL(pos);
			this->radius = radius;
		}
	};

	struct CL_Plane
	{
		CL_Material material;
		cl_float3 position;
		cl_float3 normal;

		CL_Plane(const Material &material, const glm::vec3 &pos, const glm::vec3 &normal)
		{
			this->material = CL_Material(material);

			this->position = VEC3TOCL(pos);
			this->normal = VEC3TOCL(normal);
		}
	};

	struct CL_SceneData
	{
		cl_int numSpheres;
		cl_int numPlanes;
		cl_float3 lightSource;
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
				//cameraToWorldMatrix[i] = {{ cameraToWorld[i].x, cameraToWorld[i].y, cameraToWorld[i].z, cameraToWorld[i].w }};
				cameraToWorldMatrix[i] = VEC4TOCL(cameraToWorld[i]);
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

		bufferSpheres = compute::buffer(context, 0);
		bufferPlanes = compute::buffer(context, 0);

		bufferOutput = compute::buffer(context, sizeof(cl_uchar4) * width * height);

		// Set arguments
		kernel.set_arg(2, bufferOutput);
	}

	void update_scene(const std::vector<Sphere> &inputSpheres, const std::vector<Plane> &inputPlanes, const glm::vec3 &lightSource)
	{
		std::vector<CL_Sphere> spheres;
		spheres.reserve(inputSpheres.size());

		std::vector<CL_Plane> planes;
		planes.reserve(inputPlanes.size());

		for(auto &sphere : inputSpheres) spheres.push_back(CL_Sphere(sphere.material, sphere.position, sphere.radius));
		for(auto &plane : inputPlanes) planes.push_back(CL_Plane(plane.material, plane.position, plane.normal));
		
		// Only create new buffer if previous one is too small
		if(bufferSpheres.size() < sizeof(CL_Sphere) * spheres.size())
			bufferSpheres = compute::buffer(context, sizeof(CL_Sphere) * spheres.size()); // NOTE: Assigning releases old buffer, so this does not cause a memory leak

		if(bufferPlanes.size() < sizeof(CL_Plane) * planes.size())
			bufferPlanes = compute::buffer(context, sizeof(CL_Plane) * planes.size());

		queue.enqueue_write_buffer(bufferSpheres, 0, sizeof(CL_Sphere) * spheres.size(), spheres.data());
		queue.enqueue_write_buffer(bufferPlanes, 0, sizeof(CL_Plane) * planes.size(), planes.data());

		kernel.set_arg(3, bufferSpheres); // Point to new buffer
		kernel.set_arg(4, bufferPlanes);

		sceneData.numSpheres = spheres.size();
		sceneData.numPlanes = planes.size();
		sceneData.lightSource = VEC3TOCL(lightSource);
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
