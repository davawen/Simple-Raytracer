add_rules("mode.debug", "mode.release")

add_requires("libsdl")
add_requires("cmake::OpenCL", { alias = "opencl", system = true })
add_requires("imgui", { configs = { sdl2 = true } })

target("main")
	set_kind("binary")
	add_includedirs("include")
	add_files("src/*.cpp")
	add_packages("libsdl", "imgui", "opencl")
	set_languages("gnuxx20")
