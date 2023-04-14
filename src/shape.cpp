#include "shape.hpp"

Sphere::Sphere(const Material &material, const glm::vec3 &position, float radius) {
    this->material = material;
    this->position = position;
    this->radius = radius;
}

bool Sphere::intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint,
                          glm::vec3 &intersectionNormal) const {
    if (glm::intersectRaySphere(rayOrigin, rayDirection, this->position, this->radius, intersectionPoint,
                                intersectionNormal)) {
        intersectionPoint += intersectionNormal * .001f; // intersection point shouldn't self collision

        return true;
    }

    return false;
}

Plane::Plane(const Material &material, const glm::vec3 &position, const glm::vec3 &normal) {
    this->material = material;
    this->position = position;
    this->normal = normal;
}

bool Plane::intersectRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection, glm::vec3 &intersectionPoint,
                         glm::vec3 &intersectionNormal) const {
    float distance;

    if (glm::intersectRayPlane(rayOrigin, rayDirection, this->position, this->normal, distance)) {
        intersectionPoint =
            rayOrigin + rayDirection * distance + normal * .001f; // intersection point shouldn't self collision
        intersectionNormal = normal;

        return true;
    }

    return false;
}

Box::Box(const Material &material, const glm::vec3 &position, const glm::vec3 &size) {
    this->material = material;

    this->position = position;
    this->size = size;
}

bool Box::intersectRay(const glm::vec3 &, const glm::vec3 &, glm::vec3 &, glm::vec3 &) const {
    return false;
}

std::vector<Triangle> Box::to_triangles() {
    //
    // 6---7 5
    // |\   \↓
    // 4 2---3
    // \ |   |
    //  \0---1
    //
    //

    const glm::vec3 minCorner = position - size / 2.f;
    const glm::vec3 maxCorner = position + size / 2.f;

    const glm::vec3 vertices[8] = {{minCorner},
                                   {maxCorner.x, minCorner.yz()},
                                   {minCorner.x, maxCorner.y, minCorner.z},
                                   {maxCorner.xy(), minCorner.z},
                                   {minCorner.xy(), maxCorner.z},
                                   {maxCorner.x, minCorner.y, maxCorner.z},
                                   {minCorner.x, maxCorner.y, maxCorner.z},
                                   {maxCorner}};

    // for(auto &vertice : vertices)
    // {
    // 	vertice = glm::rotateZ(vertice, 1.f);
    // 	vertice += box.position;
    // }

    const int table[12][3] = {{0, 1, 3}, {0, 2, 3}, {0, 4, 6}, {0, 2, 6}, {0, 4, 5}, {0, 1, 5},

                              {4, 5, 7}, {4, 6, 7}, {1, 5, 7}, {1, 3, 7}, {2, 6, 7}, {2, 3, 7}};

    std::vector<Triangle> out;
    out.reserve(12);
    for (size_t i = 0; i < 12; i++) {
        out.push_back(Triangle(material, vertices[table[i][0]], vertices[table[i][1]], vertices[table[i][2]]));
    }

    return out;
}

Triangle::Triangle(const Material &material, const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2) {
    this->material = material;

    this->vertices[0].p = v0;
    this->vertices[1].p = v1;
    this->vertices[2].p = v2;
}

bool Triangle::intersectRay(const glm::vec3 &, const glm::vec3 &, glm::vec3 &, glm::vec3 &) const {
    return false;
}
