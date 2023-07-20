add_rules("mode.debug", "mode.release")

add_requires("libsdl", "stb")
add_requires("cmake::OpenCL", { alias = "opencl", system = true })
add_requires("imgui", { configs = { sdl2 = true } })

target("main")
	set_kind("binary")
	add_includedirs("include", "tiny-gizmo")
	add_files("src/*.cpp", "tiny-gizmo/tiny-gizmo.cpp")
	add_packages("libsdl", "imgui", "opencl", "stb")
	set_languages("gnuxx20")
