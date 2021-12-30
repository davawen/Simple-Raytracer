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

static void rebuild_if_too_small(compute::buffer &buffer, size_t size)
{
	if(buffer.size() < size)
	{
		buffer = compute::buffer(buffer.get_context(), size);
	}
}

class Tracer
{
private:
    compute::device device;
	compute::context context;

	compute::program program;
	compute::kernel kernel;
	compute::command_queue queue;

	compute::buffer bufferOutput;

	struct CL_Material
	{
		cl_float3 color;
		cl_float3 specular;
		cl_float specularExponent;

		CL_Material()
		{
			this->color = {{ 0.f, 0.f, 0.f }};
			this->specular = {{ 0.f }};
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

		CL_Sphere()
		{
			this->material = CL_Material();
			this->position = {{ 0.f }};
			this->radius = 0.f;
		}

		CL_Sphere(const Material &material, const glm::vec3 &pos, const float radius)
		{
			this->material = CL_Material(material);

			this->position = VEC3TOCL(pos);
			this->radius = radius;
		}

		CL_Sphere(const Sphere &sphere)
		{
			this->material = CL_Material(sphere.material);

			this->position = VEC3TOCL(sphere.position);
			this->radius = sphere.radius;
		}
	};

	struct CL_Plane
	{
		CL_Material material;
		cl_float3 position;
		cl_float3 normal;

		CL_Plane()
		{
			this->material = CL_Material();
			this->position = {{ 0.f }};
			this->normal = {{ 0.f }};
		}

		CL_Plane(const Material &material, const glm::vec3 &pos, const glm::vec3 &normal)
		{
			this->material = CL_Material(material);

			this->position = VEC3TOCL(pos);
			this->normal = VEC3TOCL(normal);
		}

		CL_Plane(const Plane &plane)
		{
			this->material = CL_Material(plane.material);

			this->position = VEC3TOCL(plane.position);
			this->normal = VEC3TOCL(plane.normal);
		}
	};

	struct CL_Triangle
	{
		CL_Material material;
		cl_float3 vertices[3];

		CL_Triangle()
		{
			this->material = CL_Material();
			for(auto &v : vertices){ v = {{ 0.f }}; };
		}

		CL_Triangle(const Material &material, const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2)
		{
			this->material = CL_Material(material);
			this->vertices[0] = VEC3TOCL(v0);
			this->vertices[1] = VEC3TOCL(v1);
			this->vertices[2] = VEC3TOCL(v2);
		}
	};

	struct CL_SceneData
	{
		cl_int numSpheres;
		cl_int numTriangles;

		cl_float3 lightSource;
	} sceneData;

	struct
	{
		compute::buffer bufferGroundPlane;

		compute::buffer bufferSpheres;
		compute::buffer bufferTriangles;

		std::vector<CL_Sphere> spheres;
		std::vector<CL_Triangle> triangles;
	} shapes;
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

		shapes.bufferGroundPlane = compute::buffer(context, sizeof(CL_Plane));

		shapes.bufferSpheres = compute::buffer(context, 0);
		shapes.bufferTriangles = compute::buffer(context, 0);

		bufferOutput = compute::buffer(context, sizeof(cl_uchar4) * width * height);

		// Set arguments
		kernel.set_arg(2, bufferOutput);
		kernel.set_arg(3, shapes.bufferGroundPlane);
	}

	void update_scene(const std::vector<Sphere> &inputSpheres, const Plane &groundPlane, const std::vector<Box> &inputBoxes, const glm::vec3 &lightSource)
	{
		shapes.spheres.resize(inputSpheres.size());

		shapes.triangles.clear();

		for(size_t i = 0; i < inputSpheres.size(); i++) shapes.spheres[i] = CL_Sphere(inputSpheres[i]);
		for(auto &box : inputBoxes)
		{
			// o---o
			// |\   \
			// | o---o
			// \ |   |
			//  \o---o
			//z ↑y
			//\ →x

			shapes.triangles.resize(shapes.triangles.size() + 2*6); // 2 triangles per face
			
			glm::vec3 minCorner = box.position - glm::rotateY(box.size/2.f, 2.f);
			glm::vec3 maxCorner = box.position + glm::rotateY(box.size/2.f, 2.f);

			shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { maxCorner.x, minCorner.yz() }, { maxCorner.x, minCorner.y, maxCorner.z }));
			shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { minCorner.xy(), maxCorner.z }, { maxCorner.x, minCorner.y, maxCorner.z }));

			shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { maxCorner.x, minCorner.yz() }, { maxCorner.xy(), minCorner.z }));
			shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { minCorner.x, maxCorner.y, minCorner.z }, { maxCorner.xy(), minCorner.z }));

			shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { minCorner.xy(), maxCorner.z }, { minCorner.x, maxCorner.yz() }));
			shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { minCorner.x, maxCorner.y, minCorner.z }, { minCorner.x, maxCorner.yz() }));

			// shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { maxCorner.x, minCorner.yz() }, { maxCorner.x, minCorner.y, maxCorner.z }));
			// shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { maxCorner.x, minCorner.yz() }, { maxCorner.x, minCorner.y, maxCorner.z }));

			// shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { maxCorner.x, minCorner.yz() }, { maxCorner.x, minCorner.y, maxCorner.z }));
			// shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { maxCorner.x, minCorner.yz() }, { maxCorner.x, minCorner.y, maxCorner.z }));

			// shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { maxCorner.x, minCorner.yz() }, { maxCorner.x, minCorner.y, maxCorner.z }));
			// shapes.triangles.push_back(CL_Triangle(box.material, minCorner, { maxCorner.x, minCorner.yz() }, { maxCorner.x, minCorner.y, maxCorner.z }));
		}

		// Only create new buffer if previous one is too small
		rebuild_if_too_small(shapes.bufferSpheres, sizeof(CL_Sphere) * shapes.spheres.size());
		rebuild_if_too_small(shapes.bufferTriangles, sizeof(CL_Triangle) * shapes.triangles.size());
		
		CL_Plane tmpPlane(groundPlane);
		queue.enqueue_write_buffer(shapes.bufferGroundPlane, 0, sizeof(CL_Plane), &tmpPlane);

		if(shapes.spheres.size() > 0) queue.enqueue_write_buffer(shapes.bufferSpheres, 0, sizeof(CL_Sphere) * shapes.spheres.size(), shapes.spheres.data());
		if(shapes.triangles.size() > 0) queue.enqueue_write_buffer(shapes.bufferTriangles, 0, sizeof(CL_Triangle) * shapes.triangles.size(), shapes.triangles.data());

		kernel.set_arg(4, shapes.bufferSpheres); // Point to new buffer
		kernel.set_arg(5, shapes.bufferTriangles);

		sceneData.numSpheres = shapes.spheres.size();
		sceneData.numTriangles = shapes.triangles.size();

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
