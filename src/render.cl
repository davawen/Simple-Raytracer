#ifndef NULL
#define NULL 0
#endif

typedef struct {
	float3 origin;
	float3 direction;
} Ray;

typedef struct {
	float3 position;
	float3 normal;
	/// Checks wether the intersection happened outside the model or inside
	bool front;
} Intersection;

typedef struct {
	float smoothness;
	float metalness;
	float emission_strength;
	/// Refraction index of 0 = opaque
	float refraction_index;

	float3 color;
	float3 specular_color;
	float3 emission;
} Material;

typedef struct {
	Material material;
	float3 position;
	float radius;
} Sphere;

typedef struct {
	Material material;
	float3 position;
	float3 normal;
} Plane;

typedef struct {
	float3 vertices[3];
} Triangle;

typedef struct {
	Material material;
	uint triangle_index;
	uint num_triangles;
	float3 bounding_min;
	float3 bounding_max;
	float3 position;
	float3 size;
} Model;

typedef enum {
	SHAPE_SPHERE,
	SHAPE_PLANE,
	SHAPE_MODEL
} ShapeType;

typedef struct {
	ShapeType type;
	union {
		Sphere sphere;
		Plane plane;
		Model model;
	} shape;
} Shape;

typedef struct {
	int width, height;
	int num_samples;
	int num_bounces;
	float aspect_ratio, fov_scale;

	float4 camera_to_world[4];

	uint time;
	uint tick;
} RenderData;

typedef struct {
	int num_shapes;

	float3 horizon_color;
	float3 zenith_color;
	float3 ground_color;

	float sun_focus;
	float3 sun_color;
	float sun_intensity;
	float3 sun_direction;
} SceneData;

typedef struct {
	const SceneData *data;
	__global const Shape *shapes;
	__global const Triangle *triangles;
} Scene;

float4 matrix_by_vector(const float4 m[4], const float4 v) {
	return (float4
	)(m[0].x * v.x + m[1].x * v.y + m[2].x * v.z + m[3].x * v.w,
	  m[0].y * v.x + m[1].y * v.y + m[2].y * v.z + m[3].y * v.w,
	  m[0].z * v.x + m[1].z * v.y + m[2].z * v.z + m[3].z * v.w,
	  m[0].w * v.x + m[1].w * v.y + m[2].w * v.z + m[3].w * v.w);
}

inline float3 reflect(const float3 v, const float3 n) {
	return v - 2.0f * dot(v, n) * n;
}

float random_float(uint *seed) {
	*seed = *seed * 747796405 + 2891336453;
	uint result = ((*seed >> ((*seed >> 28) + 4)) ^ *seed) * 277803737;
	result = (result >> 22) ^ result;
	return (float)result / (float)UINT_MAX;
}

inline float random_float_normal(uint *seed) {
	float theta = 2 * M_PI_F * random_float(seed);
	float rho = sqrt(-2.0f * log(random_float(seed)));
	return rho * cos(theta);
}

inline float3 random_direction(uint *seed) {
	return normalize((float3)(random_float_normal(seed), random_float_normal(seed), random_float_normal(seed)));
}

inline float3 random_direction_hemisphere(float3 normal, uint *seed) {
	float3 dir = random_direction(seed);
	return dir * sign(dot(normal, dir));
}

inline float length_squared(float3 v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

inline float shlick_reflectance(float mu, float cos_theta) {
	float r0 = (1.0 - mu) / (1.0 + mu);
	r0 = r0 * r0;

	return r0 + (1.0 - r0) * pown(1.0 - cos_theta, 5);
}

bool intersect_sphere(__global const Sphere *sphere, const Ray *ray, float *t) {
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

bool intersect_plane(__global const Plane *plane, const Ray *ray, float *t) {
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

float intersect_triangle(Triangle *triangle, const Ray *ray, float *t) {
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

/// @param inv_dir Reciprocal of every component of ray.dir
/// @param tmax Avoid looking for bounding boxes that are too far
bool intersection_aabb(float3 bounds_min, float3 bounds_max, const Ray *ray, float3 inv_dir, float tmax) {
	float tmin = 0.0f;
	for (int d = 0; d < 3; d++) {
		float t1 = (bounds_min[d] - ray->origin[d]) * inv_dir[d];
		float t2 = (bounds_max[d] - ray->origin[d]) * inv_dir[d];

		tmin = max(tmin, min(t1, t2));
		tmax = min(tmax, max(t1, t2));
	}

	return tmin < tmax;
}

__global const Material *closest_intersection(const Scene *scene, const Ray *ray, Intersection *rayhit) {
	__global const Material *closest = NULL;
	float tmin = INFINITY;

	float3 inv_dir = 1.0f / ray->direction;

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
						rayhit->front =
							length_squared(ray->origin - sphere->position) > sphere->radius * fabs(sphere->radius);
						rayhit->normal *= rayhit->front ? 1.0f : -1.0f;
					}
				}
			}
		} else if (shape->type == SHAPE_MODEL) {
			__global const Model *model = &shape->shape.model;
			// Test bounding box first
			if (!intersection_aabb(model->bounding_min, model->bounding_max, ray, inv_dir, tmin)) {
				continue;
			}

			// Test every triangle in the model
			for (size_t i = 0; i < model->num_triangles; i++) {
				Triangle triangle = scene->triangles[model->triangle_index + i];
				for (size_t j = 0; j <= 2; j++) {
					triangle.vertices[j] = triangle.vertices[j] * model->size + model->position;
				}

				float t_i;
				if (intersect_triangle(&triangle, ray, &t_i)) {
					if (t_i < tmin) {
						tmin = t_i;
						closest = &model->material;

						if (rayhit != NULL) {
							float3 A = triangle.vertices[1] - triangle.vertices[0];
							float3 B = triangle.vertices[2] - triangle.vertices[0];

							rayhit->normal = normalize((float3
							)(A.y * B.z - A.z * B.y, A.z * B.x - A.x * B.z, A.x * B.y - A.y * B.x));
							rayhit->front = dot(rayhit->normal, ray->direction) < 0.0f;
							rayhit->normal *= rayhit->front ? 1 : -1; // Reflect normal to face camera
							rayhit->position = ray->origin + ray->direction * tmin;
						}
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
						rayhit->front = dot(plane->normal, ray->direction) < 0.0f;
						rayhit->normal =
							rayhit->front ? plane->normal : -plane->normal; // Reflect normal to face camera
						rayhit->position = ray->origin + ray->direction * tmin;
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
	             scene->data->sun_color * scene->data->sun_intensity;

	float ground_to_sky = smoothstep(-0.01f, 0.0f, ray.direction.y); // 0 -> 1 step function
	float sun_mask = ground_to_sky >= 1;

	return mix(scene->data->ground_color, sky_gradient, ground_to_sky) + sun * sun_mask;
}

float3 trace(int num_bounces, const Scene *scene, Ray *camray, uint seed) {
	float3 color = (float3)(0.f);
	float3 mask = (float3)(1.f);

	Ray ray = *camray;
	Intersection rayhit;

	for (int i = 0; i < num_bounces; i++) {
		__global const Material *material = closest_intersection(scene, &ray, &rayhit);

		if (material != NULL) {
			color += mask * material->emission * material->emission_strength;

			if (i == num_bounces - 1)
				break; // Don't compute new bounce if it's the last one

			ray.origin = rayhit.position;

			// cosine weighted distribution
			float3 random_dir = normalize(rayhit.normal + random_direction_hemisphere(rayhit.normal, &seed));
			float3 reflected_dir = reflect(ray.direction, rayhit.normal);
			bool is_specular = material->metalness > random_float(&seed);

			float3 glossy_dir = mix(random_dir, reflected_dir, material->smoothness * is_specular);

			bool transparent = material->refraction_index != 0.0f;

			float mu = rayhit.front ? 1.0f / material->refraction_index : material->refraction_index;
			float cos_theta = min(1.0f, dot(ray.direction, -rayhit.normal));
			float sin_theta = sqrt(1.0f - cos_theta * cos_theta);

			float3 out_perp = mu * (ray.direction + cos_theta * rayhit.normal);
			float3 out_parallel = -sqrt(fabs(1.0f - length_squared(out_perp))) * rayhit.normal;
			float3 refracted_dir = out_perp + out_parallel;

			bool transparent_reflection = mu * sin_theta > 1.0f // total internal reflection
			                              || shlick_reflectance(mu, cos_theta) > random_float(&seed);

			float3 transparent_dir = transparent_reflection ? glossy_dir : refracted_dir;

			ray.direction = transparent ? transparent_dir : glossy_dir;
			ray.origin += rayhit.normal*sign(dot(rayhit.normal, ray.direction))*0.001f; // avoid shadow acne

			mask *= mix(material->color, material->specular_color, is_specular);
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

__kernel void render(
	const RenderData data, const SceneData sceneData, __global float3 *output, __global const Shape *shapes,
	__global const Triangle *triangles
) {
	const uint id = get_global_id(0);

	Scene scene = {.data = &sceneData, .shapes = shapes, .triangles = triangles};
	float2 windowPos = (float2)(id % data.width, (uint)(id / data.width)); // Raster space coordinates

	float3 color = (float3)(0.f);
	for (int sample = 0; sample < data.num_samples; sample++) {
		uint seed = (sample + id * data.num_samples) * data.time * 5304;

		float2 ndcPos = (float2
		)((windowPos.x + random_float(&seed)) / data.width,
		  (windowPos.y + random_float(&seed)) / data.height); // Normalized coordinates
		float2 screenPos = (float2
		)((2.f * ndcPos.x - 1.f) * data.aspect_ratio * data.fov_scale,
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

		color += trace(data.num_bounces, &scene, &ray, seed /* + (data.time<<3)*/);
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
