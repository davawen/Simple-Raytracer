#include "tracer.hpp"

static void rebuild_if_too_small(compute::buffer &buffer, size_t size) {
	if(buffer.size() < size) {
		buffer = compute::buffer(buffer.get_context(), size);
	}
}

Tracer::Tracer(const int width, const int height) {
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
	average_kernel = compute::kernel(program, "average");

	// Create command queue
	queue = compute::command_queue(context, device);

	shapes.bufferShapes = compute::buffer(context, 0);

	renderCanvas = compute::buffer(context, sizeof(cl_float3) * width * height);
	renderOutput = compute::buffer(context, sizeof(cl_uchar4) * width * height);

	// Set arguments
	kernel.set_arg(2, renderCanvas);
	kernel.set_arg(3, shapes.bufferShapes);

	average_kernel.set_arg(1, renderCanvas);
	average_kernel.set_arg(2, renderOutput);
}

void Tracer::update_scene(const std::vector<Shape> &inputShapes, const glm::vec3 &lightSource) {
	if(inputShapes.size() > 0) {
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

void Tracer::clear_canvas() {
	float pattern = 0.f;
	queue.enqueue_fill_buffer(renderCanvas, &pattern, sizeof(float), 0, renderCanvas.size());
}

void Tracer::render(cl_uint ticks_stopped, RenderData &renderData, std::vector<uint8_t> &output) {
	// Raytrace to canvas
	kernel.set_arg(0, sizeof(RenderData), &renderData);
	queue.enqueue_1d_range_kernel(kernel, 0, renderData.width * renderData.height, 0);

	// Average with the last samples
	average_kernel.set_arg(0, sizeof(cl_uint), &ticks_stopped);
	queue.enqueue_1d_range_kernel(average_kernel, 0, renderData.width * renderData.height, 0);

	// Transfer result from gpu buffer to array
	queue.enqueue_read_buffer(renderOutput, 0, sizeof(cl_uchar4) * renderData.width * renderData.height, output.data());
}
