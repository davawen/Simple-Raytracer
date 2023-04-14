#include "tracer.hpp"

static void rebuild_if_too_small(compute::buffer &buffer, size_t size) {
    if (buffer.size() < size) {
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
    } catch (compute::program_build_failure &e) {
        std::cerr << e.build_log() << '\n' << e.error_string() << '\n';
        throw e;
    }

    // Creates the kernel
    kernel = compute::kernel(program, "render");
    average_kernel = compute::kernel(program, "average");

    // Create command queue
    queue = compute::command_queue(context, device);

    shapes.buffer_shapes = compute::buffer(context, 0);

    render_canvas = compute::buffer(context, sizeof(cl_float3) * width * height);
    render_output = compute::buffer(context, sizeof(cl_uchar4) * width * height);

    // Set arguments
    kernel.set_arg(2, render_canvas);
    kernel.set_arg(3, shapes.buffer_shapes);

    average_kernel.set_arg(1, render_canvas);
    average_kernel.set_arg(2, render_output);
}

void Tracer::update_scene(const std::vector<Shape> &input_shapes) {
    if (input_shapes.size() > 0) {
        rebuild_if_too_small(shapes.buffer_shapes, sizeof(Shape) * input_shapes.size());
        queue.enqueue_write_buffer(shapes.buffer_shapes, 0, sizeof(Shape) * input_shapes.size(), input_shapes.data());
    }

    // Generate triangles
    // shapes.triangles.clear();

    // for(auto &box : inputBoxes)
    // {
    // }

    kernel.set_arg(3, shapes.buffer_shapes); // Point to new buffer

    scene_data.num_shapes = input_shapes.size();
    kernel.set_arg(1, sizeof(SceneData), &scene_data);
}

void Tracer::clear_canvas() {
    float pattern = 0.f;
    queue.enqueue_fill_buffer(render_canvas, &pattern, sizeof(float), 0, render_canvas.size());
}

void Tracer::render(cl_uint ticks_stopped, RenderData &render_data, std::vector<uint8_t> &output) {
    // Raytrace to canvas
    kernel.set_arg(0, sizeof(RenderData), &render_data);
    queue.enqueue_1d_range_kernel(kernel, 0, render_data.width * render_data.height, 0);

    // Average with the last samples
    average_kernel.set_arg(0, sizeof(cl_uint), &ticks_stopped);
    queue.enqueue_1d_range_kernel(average_kernel, 0, render_data.width * render_data.height, 0);

    // Transfer result from gpu buffer to array
    queue.enqueue_read_buffer(render_output, 0, sizeof(cl_uchar4) * render_data.width * render_data.height, output.data());
}
