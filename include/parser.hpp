#pragma once

#include <cinttypes>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

#include "shape.hpp"

namespace fs = std::filesystem;

void save_ppm(const fs::path &filename, const std::vector<uint8_t> &pixels, int width, int height);

using ModelPair = std::pair<uint, uint>;

/// Loads the triangles of a model from an STL file.
///
/// Returns the triangle index at which the model starts and its number of triangles.
/// Returns nullopt if the given file does not exist
std::optional<ModelPair> load_stl_model(const fs::path &filename, std::vector<Triangle> &triangles);

/// Loads the triangles of a model from an OBJ wavefront file.
/// Currently does not support smooth shading, texture coordinates and probably more.
/// Returns the triangle index at which the model starts and its number of triangles.
/// Returns nullopt if the given file does not exist
std::optional<ModelPair> load_obj_model(const std::filesystem::path filename, std::vector<Triangle> &triangles); 
