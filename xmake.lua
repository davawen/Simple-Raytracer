add_rules("mode.debug", "mode.release")

add_requires("libsdl", "stb")
add_requires("opencl")
add_requires("imgui", { configs = { sdl2 = true } })

target("main")
	set_kind("binary")
	add_includedirs("include", "lib")
	add_files("src/*.cpp", "lib/*.cpp")
	add_packages("libsdl", "imgui", "opencl", "stb")
	set_languages("gnuxx20")
