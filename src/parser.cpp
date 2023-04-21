#include "parser.hpp"
#include <glm/gtx/string_cast.hpp>

void save_ppm(const fs::path &filename, const std::vector<uint8_t> &pixels, int width, int height) {
	std::ofstream file;
	file.open(filename, std::ios::binary | std::ios::out);
	file << "P6 ";
	file << std::to_string(width) << ' ' << std::to_string(height) << " 255\n";

	for (size_t i = 0; i < pixels.size(); i += 4) {
		const uint8_t *p = &pixels[i]; // ARGB

		file.write((char *)p + 1, 3);
	}
}

std::optional<ModelPair> load_stl_model(const fs::path &filename, std::vector<Triangle> &triangles) {
	std::ifstream file;
	file.open(filename, std::ios::binary | std::ios::in);
	if (file.fail()) {
		return std::nullopt;
	}

	struct StlHeader {
		uint8_t header[80];
		uint32_t num_triangles;
	};

	struct __attribute__((packed)) StlTriangle {
		float normal[3];
		float v1[3];
		float v2[3];
		float v3[3];
		uint16_t attribute;
	};

	StlHeader header;
	file.read((char *)&header, sizeof(StlHeader));

	size_t model_index = triangles.size();

	for (size_t i = 0; i < header.num_triangles; i++) {
		StlTriangle t;
		file.read((char *)&t, sizeof(StlTriangle));

#define ARRAY_TO_VEC3(x) (glm::vec3(x[0], x[1], x[2]))
		triangles.push_back(
			Triangle(ARRAY_TO_VEC3(t.normal), ARRAY_TO_VEC3(t.v1), ARRAY_TO_VEC3(t.v2), ARRAY_TO_VEC3(t.v3))
		);
	}

	return {{model_index, header.num_triangles}};
}

std::optional<ModelPair> load_obj_model(const std::filesystem::path filename, std::vector<Triangle> &triangles) {
	std::ifstream file;
	file.open(filename, std::ios::in);
	if (file.fail()) {
		return std::nullopt;
	}

	struct Face {
		int vertices[3];
		int normals[3];
	};

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<Face> faces;

	std::string line, mode;
	while (std::getline(file, line)) {
		std::istringstream stream(line);

		std::getline(stream, mode, ' ');
		if (mode.starts_with('#')) { // comment
			continue;                // skip line
		} else if (mode == "v") {    // vertex
			float x, y, z;
			stream >> x >> y >> z;
			vertices.push_back({x, y, z});
		} else if (mode == "vn") { // normal
			float x, y, z;
			stream >> x >> y >> z;
			normals.push_back(glm::normalize(glm::vec3(x, y, z)));
		} else if (mode == "f") { // face
			Face face;
			auto extract_index = [&stream](int &vertex, int &normal) {
				stream >> vertex;
				if (stream.get() == '/') {
					if (stream.peek() != '/') {
						int uv;
						stream >> uv;
					}

					if (stream.get() == '/') {
						stream >> normal;
						assert(stream.get() == ' ');
					}
				}
			};
			extract_index(face.vertices[0], face.normals[0]);
			extract_index(face.vertices[1], face.normals[1]);
			extract_index(face.vertices[2], face.normals[2]);

			faces.push_back(face);
		} else if (mode == "s") {
			continue; // ignore smooth shading
		}
	}

	size_t index = triangles.size();
	size_t len = faces.size();
	for (auto &face : faces) {
		auto adjust = [](int &index, int len) {
			if (index < 0) { // negative indices specify the end of the list
				index = len - index + 1;
			}
			index -= 1; // indices are 1-based
		};

		Triangle triangle;
		for(int i = 0; i < 3; i++) {
			adjust(face.vertices[i], vertices.size());
			adjust(face.normals[i], normals.size());

			triangle.vertices[i].pos = vertices[face.vertices[i]];
			triangle.vertices[i].normal = normals[face.normals[i]];
		}

		triangles.push_back(triangle);
	}

	return {{ index, len }};
}
