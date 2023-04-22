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
	float metallic;
	float specular;
	float emission_strength;
	float transmittance;
	float refraction_index;

	float3 color;
	float3 emission;
} Material;

typedef struct {
	float3 position;
	float radius;
} Sphere;

typedef struct {
	float3 position;
	float3 normal;
} Plane;

typedef struct {
	float3 normal;
	float3 pos;
} Vertex;

typedef struct {
	union {
		struct {
			Vertex v0;
			Vertex v1;
			Vertex v2;
		};
		Vertex vertices[3];
	};
} Triangle;

typedef struct {
	uint triangle_index;
	uint num_triangles;
	float3 bounding_min;
	float3 bounding_max;
	float3 position;
	float3 scale;
	/// quaternion
	float4 orientation;
} Model;

typedef enum {
	SHAPE_SPHERE,
	SHAPE_PLANE,
	SHAPE_MODEL
} ShapeType;

typedef struct {
	ShapeType type;
	int material;
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
	float aspect_ratio;
	float fov_scale;
	/// For some reason, open cl refuses bools in kernel parameters?
	char show_normals;

	float4 camera_to_world[4];

	uint time;
	uint tick;
} RenderData;

typedef struct {
	int num_shapes;
	float sun_focus;
	float sun_intensity;

	float3 horizon_color;
	float3 zenith_color;
	float3 ground_color;

	float3 sun_color;
	float3 sun_direction;
} SceneData;

typedef struct {
	const SceneData *data;
	__global const Shape *shapes;
	__global const Triangle *triangles;
	__global const Material *materials;
} Scene;

float4 matrix_by_vector(__generic const float4 *m, const float4 v) {
	return (float4
	)(m[0].x * v.x + m[1].x * v.y + m[2].x * v.z + m[3].x * v.w,
	  m[0].y * v.x + m[1].y * v.y + m[2].y * v.z + m[3].y * v.w,
	  m[0].z * v.x + m[1].z * v.y + m[2].z * v.z + m[3].z * v.w,
	  m[0].w * v.x + m[1].w * v.y + m[2].w * v.z + m[3].w * v.w);
}

/// Rotates the vector `v` by the quaterion `q`
inline float3 rotate(float3 v, float4 q) {
	float3 q_vec = q.xyz;
	float3 uv = cross(q_vec, v);
	float3 uuv = cross(q_vec, uv);

	return v + ((uv * q.w) + uuv) * 2.0f;
}

inline float3 transform(float3 p, float3 translation, float3 scale, float4 orientation) {
	return rotate(p*scale, orientation) + translation;
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

bool intersect_sphere(__generic const Sphere *sphere, const Ray *ray, float *t) {
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

bool intersect_plane(__generic const Plane *plane, const Ray *ray, float *t) {
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

float3 barycentric_weights(__generic Triangle *triangle, float3 p) {
	float3 v0 = triangle->v1.pos - triangle->v0.pos;
	float3 v1 = triangle->v2.pos - triangle->v0.pos;
	float3 v2 = p - triangle->v0.pos;

    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;

    float w0 = (d11 * d20 - d01 * d21) / denom;
    float w1 = (d00 * d21 - d01 * d20) / denom;
    float w2 = 1.0f - w0 - w1;

	// don't ask me WHY but I have to shift the weights
	return (float3)(w2, w0, w1);
}

float intersect_triangle(__generic Triangle *triangle, const Ray *ray, float *t) {
	float3 edge1, edge2, h, s, q;
	float a, f, u, v;

	edge1 = triangle->v1.pos - triangle->v0.pos;
	edge2 = triangle->v2.pos - triangle->v0.pos;

	h = cross(ray->direction, edge2);
	a = dot(edge1, h);

	if (a == 0)
		return false;

	f = 1.f / a;
	s = ray->origin - triangle->v0.pos;
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

/// Returns the material index of the closest intersection
int closest_intersection(const Scene *scene, const Ray *ray, Intersection *rayhit) {
	int closest = -1;
	float tmin = INFINITY;

	float3 inv_dir = 1.0f / ray->direction;

	for (int i = 0; i < scene->data->num_shapes; i++) {
		__generic const Shape *shape = &scene->shapes[i];
		if (shape->type == SHAPE_SPHERE) {
			__generic const Sphere *sphere = &shape->shape.sphere;

			float t_i;
			if (intersect_sphere(sphere, ray, &t_i)) {
				if (t_i < tmin) {
					tmin = t_i;
					closest = shape->material;

					if (rayhit != NULL) {
						rayhit->position = ray->origin + ray->direction * tmin;
						rayhit->normal = normalize(rayhit->position - sphere->position);
						rayhit->front =
							length_squared(ray->origin - sphere->position) > sphere->radius * fabs(sphere->radius);
					}
				}
			}
		} else if (shape->type == SHAPE_MODEL) {
			__generic const Model *model = &shape->shape.model;
			// Test bounding box first
			if (!intersection_aabb(model->bounding_min, model->bounding_max, ray, inv_dir, tmin)) {
				continue;
			}

			// Test every triangle in the model
			for (size_t i = 0; i < model->num_triangles; i++) {
				Triangle triangle = scene->triangles[model->triangle_index + i];
				for (size_t j = 0; j <= 2; j++) {
					triangle.vertices[j].pos = transform(triangle.vertices[j].pos, model->position, model->scale, model->orientation);
				}

				float t_i;
				if (intersect_triangle(&triangle, ray, &t_i)) {
					if (t_i < tmin) {
						tmin = t_i;
						closest = shape->material;

						if (rayhit != NULL) {
							rayhit->position = ray->origin + ray->direction * tmin;

							// Smooth shading
							float3 weights = barycentric_weights(&triangle, rayhit->position);
							rayhit->normal = normalize(triangle.v0.normal*weights.x + triangle.v1.normal*weights.y + triangle.v2.normal*weights.z);
							rayhit->normal = rotate(rayhit->normal, model->orientation);

							// Assume couter-clockwise vertex order (-Z main axis)
							float3 computed_normal = cross(triangle.v1.pos - triangle.v0.pos, triangle.v2.pos - triangle.v0.pos);

							rayhit->front = dot(computed_normal, ray->direction) < 0.0f;
						}
					}
				}
			}
		} else if (shape->type == SHAPE_PLANE) {
			__generic const Plane *plane = &shape->shape.plane;

			float t_i;
			if (intersect_plane(plane, ray, &t_i)) {
				if (t_i < tmin) {
					tmin = t_i;
					closest = shape->material;

					if (rayhit != NULL) {
						rayhit->normal = plane->normal;
						rayhit->front = dot(plane->normal, ray->direction) < 0.0f;
						rayhit->position = ray->origin + ray->direction * tmin;
					}
				}
			}
		}
	}

	if (tmin == FLT_MAX)
		return -1;

	if (rayhit != NULL) {
		rayhit->normal *= rayhit->front ? 1.0f : -1.0f; // Reflect normal to always face the camera
	}

	return closest;
}

float3 sky_box(Ray ray, const Scene *scene, image2d_t skybox, sampler_t sampler) {
	// float sky_gradient_t = pow(smoothstep(0.0f, 0.4f, ray.direction.y), 0.35f);
	// float3 sky_gradient = mix(scene->data->horizon_color, scene->data->zenith_color, sky_gradient_t);
	float3 sun = pow(max(dot(ray.direction, -scene->data->sun_direction), 0.0f), scene->data->sun_focus)
		* scene->data->sun_color * scene->data->sun_intensity;

	// float ground_to_sky = smoothstep(-0.01f, 0.0f, ray.direction.y); // 0 -> 1 step function
	// float sun_mask = ground_to_sky >= 1;

	// return mix(scene->data->ground_color, sky_gradient, ground_to_sky) + sun * sun_mask;
	float u = atan2pi(ray.direction.z, ray.direction.x)*0.5f + 0.5f;
	float v = ray.direction.y*0.5f + 0.5f;

	return read_imagef(skybox, sampler, (float2)(u, v)).xyz + sun;
}

float3 trace(const RenderData *render, const Scene *scene, Ray *camray, uint seed, image2d_t skybox, sampler_t sampler) {
	float3 color = (float3)(0.f);
	float3 mask = (float3)(1.f);

	Ray ray = *camray;
	Intersection rayhit;

	for (int i = 0; i < render->num_bounces; i++) {
		int material_index = closest_intersection(scene, &ray, &rayhit);

		if (material_index >= 0) {
			if (render->show_normals) {
				color = rayhit.normal*0.5f + 0.5f;
				break;
			}

			__generic const Material *material = &scene->materials[material_index];
			color += mask * material->emission * material->emission_strength;

			if (i == render->num_bounces - 1)
				break; // Don't compute new bounce if it's the last one

			ray.origin = rayhit.position;

			// cosine weighted distribution
			float3 random_dir = normalize(rayhit.normal + random_direction_hemisphere(rayhit.normal, &seed));
			float3 reflected_dir = reflect(ray.direction, rayhit.normal);

			bool is_metallic = material->metallic > random_float(&seed);
			bool is_specular = material->specular > random_float(&seed);

			float3 rough_dir = mix(random_dir, reflected_dir, material->smoothness);

			bool is_transparent = material->transmittance > random_float(&seed);

			if (!is_transparent) {
				ray.direction = mix(random_dir, rough_dir, is_metallic || is_specular);

				// diffuse reflection and metal reflection = color with object's albedo
				// specular refection = white reflection
				mask *= mix(material->color, (float3)(1.0f), is_specular);
			} else {
				// roughness affects refraction
				// this gives `ray.direction` for a perfectly smooth surface
				float3 in_dir = reflect(rough_dir, rayhit.normal);

				float mu = rayhit.front ? 1.0f / material->refraction_index : material->refraction_index;
				float cos_theta = min(1.0f, dot(in_dir, -rayhit.normal));
				float sin_theta = sqrt(1.0f - cos_theta * cos_theta);

				bool transparency_reflected = mu * sin_theta > 1.0f // total internal reflection
					|| shlick_reflectance(mu, cos_theta) > random_float(&seed);

				if (transparency_reflected) {
					ray.direction = rough_dir;
				} else {
					float3 out_perp = mu * (in_dir + cos_theta * rayhit.normal);
					float3 out_parallel = -sqrt(fabs(1.0f - length_squared(out_perp))) * rayhit.normal;
					float3 refracted_dir = out_perp + out_parallel;

					ray.direction = refracted_dir;
					mask *= material->color;
				}
			} 

			ray.direction = normalize(ray.direction);
			ray.origin += rayhit.normal * sign(dot(rayhit.normal, ray.direction)) * 0.001f; // avoid shadow acne
		} else { // No collision -- Sky
			mask *= sky_box(ray, scene, skybox, sampler);
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
	__global const Triangle *triangles, __global const Material *materials,
	image2d_t skybox, sampler_t sampler
) {
	const uint id = get_global_id(0);

	Scene scene = {.data = &sceneData, .shapes = shapes, .triangles = triangles, .materials = materials};
	float2 windowPos = (float2)(id % data.width, (uint)(id / data.width)); // Raster space coordinates

	// output[id] += read_imagef()

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

		color += trace(&data, &scene, &ray, seed, skybox, sampler);
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
