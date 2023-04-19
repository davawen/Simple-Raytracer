#include "tracer.hpp"

static void rebuild_if_too_small(compute::buffer &buffer, size_t size) {
	if (buffer.size() < size) {
		buffer = compute::buffer(buffer.get_context(), size);
	}
}

Tracer::Tracer(const int width, const int height) : options(width, height) {
	// Get the default device
	device = compute::system::default_device();
	std::cout << device.name() << " on " << device.vendor() << '\n';

	// Create a context from the device
	context = compute::context(device);

	// Creates the program from source
	try {
		program = compute::program::create_with_source_file("src/render.cl", context);
		program.build("-cl-std=CL2.0");
	} catch (compute::program_build_failure &e) {
		std::cerr << e.build_log() << '\n' << e.error_string() << '\n';
		throw e;
	}

	// Creates the kernel
	kernel = compute::kernel(program, "render");
	average_kernel = compute::kernel(program, "average");

	// Create command queue
	queue = compute::command_queue(context, device);

	buffer_shapes = compute::buffer(context, 0);
	buffer_triangles = compute::buffer(context, 0);
	buffer_materials = compute::buffer(context, 0);

	render_canvas = compute::buffer(context, sizeof(cl_float3) * width * height);
	render_output = compute::buffer(context, sizeof(cl_uchar4) * width * height);

	// Set arguments
	kernel.set_arg(2, render_canvas);
	kernel.set_arg(3, buffer_shapes);
	kernel.set_arg(4, buffer_triangles);
	kernel.set_arg(5, buffer_materials);

	average_kernel.set_arg(1, render_canvas);
	average_kernel.set_arg(2, render_output);
}

void Tracer::update_scene(
	const std::vector<Shape> &shapes, const std::vector<Triangle> &triangles, const std::vector<Material> &materials
) {
	if (shapes.size() > 0) {
		auto size = sizeof(Shape) * shapes.size();
		rebuild_if_too_small(buffer_shapes, size);
		queue.enqueue_write_buffer(buffer_shapes, 0, size, shapes.data());
	}
	if (triangles.size() > 0) {
		auto size = sizeof(Triangle) * triangles.size();
		rebuild_if_too_small(buffer_triangles, size);
		queue.enqueue_write_buffer(buffer_triangles, 0, size, triangles.data());
	}
	if (materials.size() > 0) {
		auto size = sizeof(Material) * materials.size();
		rebuild_if_too_small(buffer_materials, size);
		queue.enqueue_write_buffer(buffer_materials, 0, size, materials.data());
	}

	// Point to new buffers
	kernel.set_arg(3, buffer_shapes);
	kernel.set_arg(4, buffer_triangles);
	kernel.set_arg(5, buffer_materials);

	scene_data.num_shapes = shapes.size();
	kernel.set_arg(1, sizeof(SceneData), &scene_data);
}

void Tracer::clear_canvas() {
	float pattern = 0.f;
	queue.enqueue_fill_buffer(render_canvas, &pattern, sizeof(float), 0, render_canvas.size());
}

void Tracer::render(cl_uint ticks_stopped, std::vector<uint8_t> &output) {
	// Raytrace to canvas
	kernel.set_arg(0, sizeof(RenderData), &options);
	queue.enqueue_1d_range_kernel(kernel, 0, options.width * options.height, 0);

	// Average with the last samples
	average_kernel.set_arg(0, sizeof(cl_uint), &ticks_stopped);
	queue.enqueue_1d_range_kernel(average_kernel, 0, options.width * options.height, 0);

	// Transfer result from gpu buffer to array
	queue.enqueue_read_buffer(render_output, 0, sizeof(cl_uchar4) * options.width * options.height, output.data());
}
