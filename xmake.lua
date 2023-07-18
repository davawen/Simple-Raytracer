add_rules("mode.debug", "mode.release")

add_requires("libsdl", "stb")
add_requires("cmake::OpenCL", { alias = "opencl", system = true })
add_requires("imgui", { configs = { sdl2 = true } })

target("main")
	set_kind("binary")
	add_includedirs("include", "im3d")
	add_files("src/*.cpp", "im3d/*.cpp")
	add_packages("libsdl", "imgui", "opencl", "stb")
	set_languages("gnuxx20")
