#include "tracer.hpp"

static void rebuild_if_too_small(compute::buffer &buffer, size_t size)
{
	if(buffer.size() < size)
	{
		buffer = compute::buffer(buffer.get_context(), size);
	}
}

Tracer::Tracer(const int width, const int height)
{
	// Get the default device
	device = compute::system::default_device();

	// Create a context from the device
	context = compute::context(device);

	// Creates the program from source
	try {
		program = compute::program::create_with_source_file("kernel.cl", context);
		program.build();
	}
	catch(compute::program_build_failure &e) {
		std::cerr << e.build_log() << '\n' << e.error_string() << '\n';
		throw e;
	}


	// Creates the kernel
	kernel = compute::kernel(program, "render");

	// Create command queue
	queue = compute::command_queue(context, device);

	shapes.bufferShapes = compute::buffer(context, 0);

	renderOutput = compute::buffer(context, sizeof(cl_uchar4) * width * height);

	// Set arguments
	kernel.set_arg(2, renderOutput);
	kernel.set_arg(3, shapes.bufferShapes);
}

void Tracer::update_scene(const std::vector<Shape> &inputShapes, const glm::vec3 &lightSource)
{
	if(inputShapes.size() > 0)
	{
		rebuild_if_too_small(shapes.bufferShapes, sizeof(Shape) * inputShapes.size());
		queue.enqueue_write_buffer(shapes.bufferShapes, 0, sizeof(Shape) * inputShapes.size(), inputShapes.data());
	}

	// Generate triangles
	// shapes.triangles.clear();

	// for(auto &box : inputBoxes)
	// {
	// }

	kernel.set_arg(3, shapes.bufferShapes); // Point to new buffer

	sceneData.numShapes = inputShapes.size();

	sceneData.lightSource = VEC3TOCL(lightSource);
	kernel.set_arg(1, sizeof(SceneData), &sceneData);
}

void Tracer::render(RenderData &renderData, std::vector<uint8_t> &output)
{
	// if(bufferScene.get_host_ptr() == NULL)
	// {
	// 	std::cout << bufferScene.get_host_ptr() << "\n" << bufferOutput.get_host_ptr() << "\n";
	// 	throw std::runtime_error("Scene is not initialized");
	// }

	// kernel.set_arg(4, sizeof(Square), &square);
	kernel.set_arg(0, sizeof(RenderData), &renderData);

	// Run kernel
	queue.enqueue_1d_range_kernel(kernel, 0, renderData.width * renderData.height, 0); // Traces to tracerOutput

	// Transfer result from gpu buffer to array
	queue.enqueue_read_buffer(renderOutput, 0, sizeof(cl_uchar4) * renderData.width * renderData.height, output.data());
}
