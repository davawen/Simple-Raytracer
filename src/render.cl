#define MAX_BOUNCE 10
#ifndef NULL
#define NULL 0
#endif

struct Ray {
    float3 origin;
    float3 direction;
};
typedef struct Ray Ray;

struct Intersection {
    float3 position;
    float3 normal;
};
typedef struct Intersection Intersection;

struct Material {
    float3 color;
    float smoothness;

    float3 emission;
};
typedef struct Material Material;

struct Sphere {
    Material material;
    float3 position;
    float radius;
};
typedef struct Sphere Sphere;

struct Plane {
    Material material;
    float3 position;
    float3 normal;
};
typedef struct Plane Plane;

struct Triangle {
    Material material;
    float3 vertices[3];
};
typedef struct Triangle Triangle;

typedef enum {
    SHAPE_SPHERE,
    SHAPE_PLANE,
    SHAPE_TRIANGLE
} ShapeType;

typedef struct {
    ShapeType type;
    union {
        Sphere sphere;
        Plane plane;
        Triangle triangle;
    } shape;
} Shape;

struct RenderData {
    int width, height;
    int num_samples;
    float aspect_ratio, fov_scale;

    float4 camera_to_world[4];

    uint time;
    uint tick;
};
typedef struct RenderData RenderData;

struct SceneData {
    int num_shapes;

    float3 horizon_color;
    float3 zenith_color;
    float3 ground_color;
    float sun_focus;
    float3 sun_color;
    float3 sun_direction;
};
typedef struct SceneData SceneData;

struct Scene {
    const SceneData *data;
    __global const Shape *shapes;
};
typedef struct Scene Scene;

float4 matrix_by_vector(const float4 m[4], const float4 v) {
    return (float4)(m[0].x * v.x + m[1].x * v.y + m[2].x * v.z + m[3].x * v.w,
                    m[0].y * v.x + m[1].y * v.y + m[2].y * v.z + m[3].y * v.w,
                    m[0].z * v.x + m[1].z * v.y + m[2].z * v.z + m[3].z * v.w,
                    m[0].w * v.x + m[1].w * v.y + m[2].w * v.z + m[3].w * v.w);
}

float3 reflect(const float3 v, const float3 n) {
    return v - 2.0f * dot(v, n) * n;
}

float random_float(uint *seed) {
    *seed = *seed * 747796405 + 2891336453;
    uint result = ((*seed >> ((*seed >> 28) + 4)) ^ *seed) * 277803737;
    result = (result >> 22) ^ result;
    return (float)result / (float)UINT_MAX;
}

float random_float_normal(uint *seed) {
    float theta = 2 * M_PI_F * random_float(seed);
    float rho = sqrt(-2.0f * log(random_float(seed)));
    return rho * cos(theta);
}

float3 random_direction(uint *seed) {
    return normalize((float3)(random_float_normal(seed), random_float_normal(seed), random_float_normal(seed)));
}

float3 random_direction_hemisphere(float3 normal, uint *seed) {
    float3 dir = random_direction(seed);
    return dir * sign(dot(normal, dir));
}

bool intersect_sphere(__global const Sphere *sphere, const struct Ray *ray, float *t) {
    float3 rayToCenter = sphere->position - ray->origin;

    /* calculate coefficients a, b, c from quadratic equation */

    /* float a = dot(ray->dir, ray->dir); // ray direction is normalised, dotproduct simplifies to 1 */
    float b = dot(rayToCenter, ray->direction);
    float c = dot(rayToCenter, rayToCenter) - sphere->radius * sphere->radius;
    float disc = b * b - c; /* discriminant of quadratic formula */

    /* solve for t (distance to hitpoint along ray) */

    if (disc < 0.0f)
        return false;
    else
        *t = b - sqrt(disc);

    if (*t < 0.0f) {
        *t = b + sqrt(disc);
        if (*t < 0.0f)
            return false;
    }

    return true;
}

bool intersect_plane(__global const Plane *plane, const struct Ray *ray, float *t) {
    float denom = dot(plane->normal, ray->direction);

    if (fabs(denom) == 0.f)
        return false;

    float tmp = dot(plane->normal, plane->position - ray->origin) / denom;

    // Backwards intersection
    if (tmp < 0.f)
        return false;

    *t = tmp;

    return true;
}

float intersect_triangle(__global const Triangle *triangle, const Ray *ray, float *t) {
    float3 edge1, edge2, h, s, q;
    float a, f, u, v;

    edge1 = triangle->vertices[1] - triangle->vertices[0];
    edge2 = triangle->vertices[2] - triangle->vertices[0];

    h = cross(ray->direction, edge2);
    a = dot(edge1, h);

    if (a == 0)
        return false;

    f = 1.f / a;
    s = ray->origin - triangle->vertices[0];
    u = f * dot(s, h);

    if (u < 0.f || u > 1.f)
        return false;

    q = cross(s, edge1);
    v = f * dot(ray->direction, q);

    if (v < 0.f || u + v > 1.f)
        return false;

    *t = f * dot(edge2, q);
    if (*t > 0.f) {
        return true;
    }

    return false;
}

__global const Material *closest_intersection(const Scene *scene, const Ray *ray, Intersection *rayhit) {
    __global const Material *closest = NULL;
    float tmin = FLT_MAX;

    for (int i = 0; i < scene->data->num_shapes; i++) {
        __global const Shape *shape = &scene->shapes[i];
        if (shape->type == SHAPE_SPHERE) {
            __global const Sphere *sphere = &shape->shape.sphere;

            float t_i;
            if (intersect_sphere(sphere, ray, &t_i)) {
                if (t_i < tmin) {
                    tmin = t_i;
                    closest = &sphere->material;

                    if (rayhit != NULL) {
                        rayhit->position = ray->origin + ray->direction * tmin;
                        rayhit->normal = normalize(rayhit->position - sphere->position);
                        rayhit->position += rayhit->normal * 0.001f;
                    }
                }
            }
        } else if (shape->type == SHAPE_TRIANGLE) {
            __global const Triangle *triangle = &shape->shape.triangle;

            float t_i;
            if (intersect_triangle(triangle, ray, &t_i)) {
                if (t_i < tmin) {
                    tmin = t_i;
                    closest = &triangle->material;

                    if (rayhit != NULL) {
                        float3 A = triangle->vertices[1] - triangle->vertices[0];
                        float3 B = triangle->vertices[2] - triangle->vertices[0];

                        rayhit->normal =
                            normalize((float3)(A.y * B.z - A.z * B.y, A.z * B.x - A.x * B.z, A.x * B.y - A.y * B.x));
                        rayhit->normal = dot(rayhit->normal, ray->direction) < 0.0f
                                             ? rayhit->normal
                                             : -rayhit->normal; // Reflect normal to face camera
                        rayhit->position = ray->origin + ray->direction * tmin + rayhit->normal * .001f;
                    }
                }
            }
        } else if (shape->type == SHAPE_PLANE) {
            __global const Plane *plane = &shape->shape.plane;

            float t_i;
            if (intersect_plane(plane, ray, &t_i)) {
                if (t_i < tmin) {
                    tmin = t_i;
                    closest = &plane->material;

                    if (rayhit != NULL) {
                        rayhit->normal = dot(plane->normal, ray->direction) < 0.0f
                                             ? plane->normal
                                             : -plane->normal; // Reflect normal to face camera
                        rayhit->position = ray->origin + ray->direction * tmin + rayhit->normal * 0.001f;
                    }
                }
            }
        }
    }

    if (tmin == FLT_MAX)
        return NULL;

    return closest;
}

float3 sky_box(Ray ray, const Scene *scene) {
    // const float3 horizon = (float3)(0.251, 0.686, 0.98);
    // const float3 sky_top = (float3)(0.569, 0.784, 0.949);

    float sky_gradient_t = pow(smoothstep(0.0f, 0.4f, ray.direction.y), 0.35f);
    float3 sky_gradient = mix(scene->data->horizon_color, scene->data->zenith_color, sky_gradient_t);
    float3 sun = pow(max(dot(ray.direction, -scene->data->sun_direction), 0.0f), scene->data->sun_focus) *
                 scene->data->sun_color;

    float ground_to_sky = smoothstep(-0.01f, 0.0f, ray.direction.y); // 0 -> 1 step function
    float sun_mask = ground_to_sky >= 1;

    return mix(scene->data->ground_color, sky_gradient, ground_to_sky) + sun * sun_mask;
}

float3 trace(const Scene *scene, Ray *camray, uint seed) {
    float3 color = (float3)(0.f);
    float3 mask = (float3)(1.f);

    Ray ray = *camray;
    Intersection rayhit;

    for (size_t i = 0; i < MAX_BOUNCE; i++) {
        __global const Material *material = closest_intersection(scene, &ray, &rayhit);

        if (material != NULL) {
            mask *= material->color;
            color += mask * material->emission;

            if (i == MAX_BOUNCE)
                break; // Don't compute new bounce if it's the last one

            float3 reflected_dir = reflect(ray.direction, rayhit.normal);
            float3 random_dir = random_direction_hemisphere(rayhit.normal, &seed);

            ray.origin = rayhit.position;
            ray.direction = normalize(mix(random_dir, reflected_dir, material->smoothness));

            mask *= dot(normalize(mix(ray.direction, rayhit.normal, material->smoothness)), rayhit.normal);
        } else { // No collision -- Sky
            mask *= sky_box(ray, scene);
            color += mask;
            break;
        }
    }

    return color;
}

float3 aces(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return clamp((x * (x * a + b)) / (x * (x * c + d) + e), (float3)(0.0f), (float3)(1.0f));
}

__kernel void render(const struct RenderData data, const struct SceneData sceneData, __global float3 *output,
                     __global const Shape *shapes) {
    const uint id = get_global_id(0);

    Scene scene = {.data = &sceneData, .shapes = shapes};
    float2 windowPos = (float2)(id % data.width, (uint)(id / data.width)); // Raster space coordinates

    float3 color = (float3)(0.f);
    for (int sample = 0; sample < data.num_samples; sample++) {
        uint seed = (sample + id * data.num_samples) * data.time * 5304;

        float2 ndcPos = (float2)((windowPos.x + random_float(&seed)) / data.width,
                                 (windowPos.y + random_float(&seed)) / data.height); // Normalized coordinates
        float2 screenPos =
            (float2)((2.f * ndcPos.x - 1.f) * data.aspect_ratio * data.fov_scale,
                     (1.f - 2.f * ndcPos.y) * data.fov_scale); // Screen space coordinates (invert y axis)
        float3 cameraPos = (float3)(screenPos.x, screenPos.y, 1);

        Ray ray;
        // 1 0 0 x
        // 0 1 0 y
        // 0 0 1 z
        // 0 0 0 1
        // Get translation part
        ray.origin = data.camera_to_world[3].xyz;

        // vec4 with 0 at the end is only affected by rotation, not translation
        // Only normalize 3d components
        ray.direction = normalize(matrix_by_vector(data.camera_to_world, (float4)(cameraPos.xyz, 0)).xyz);

        color += trace(&scene, &ray, seed /* + (data.time<<3)*/);
    }
    color /= data.num_samples;

    output[id] += color;
}

__kernel void average(const uint num_steps, __global const float3 *canvas, __global uchar4 *output) {
    const uint id = get_global_id(0);

    float3 color = canvas[id] / num_steps;

    color = aces(color);
    color = sqrt(color);

    // ARGB
    output[id] = (uchar4)(255, color.x * 255.0f, color.y * 255.0f, color.z * 255.0f);
}
