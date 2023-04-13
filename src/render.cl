#define MAX_BOUNCE 10
#ifndef NULL
#define NULL 0
#endif

struct Ray
{
	float3 origin;
	float3 direction;
};
typedef struct Ray Ray;

struct Intersection
{
	float3 position;
	float3 normal;
};
typedef struct Intersection Intersection;

enum MaterialType
{
	MATERIAL_DIFFUSE,
	MATERIAL_REFLECTIVE
};

struct Material
{
	enum MaterialType type;

	float3 color;
	float3 specular;
	float specularExponent;

	float3 emission;
};
typedef struct Material Material;

struct Sphere
{
	Material material;
	float3 position;
	float radius;
};
typedef struct Sphere Sphere;

struct Plane
{
	Material material;
	float3 position;
	float3 normal;
};
typedef struct Plane Plane;

struct Triangle
{
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

struct RenderData
{
	int width, height;
	int numSamples;
	float aspectRatio, fieldOfViewScale;

	float4 cameraToWorldMatrix[4];

	uint time;
	uint tick;
};
typedef struct RenderData RenderData;

struct SceneData
{
	int numShapes;
	float3 lightSource;
};
typedef struct SceneData SceneData;

struct Scene
{
	const SceneData *data;

	__global const Shape *shapes;
};
typedef struct Scene Scene;

float4 matrixByVector(const float4 m[4], const float4 v)
{
	return (float4)(
		m[0].x*v.x + m[1].x*v.y + m[2].x*v.z + m[3].x*v.w,
		m[0].y*v.x + m[1].y*v.y + m[2].y*v.z + m[3].y*v.w,
		m[0].z*v.x + m[1].z*v.y + m[2].z*v.z + m[3].z*v.w,
		m[0].w*v.x + m[1].w*v.y + m[2].w*v.z + m[3].w*v.w
	);
}

float3 reflect(const float3 v, const float3 n)
{
	return v - 2.0f*dot(v, n)*n;
}

uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

bool intersect_sphere(__global const Sphere *sphere, const struct Ray *ray, float *t)
{
	float3 rayToCenter = sphere->position - ray->origin;
	
	/* calculate coefficients a, b, c from quadratic equation */

	/* float a = dot(ray->dir, ray->dir); // ray direction is normalised, dotproduct simplifies to 1 */ 
	float b = dot(rayToCenter, ray->direction);
	float c = dot(rayToCenter, rayToCenter) - sphere->radius*sphere->radius;
	float disc = b * b - c; /* discriminant of quadratic formula */

	/* solve for t (distance to hitpoint along ray) */

	if(disc < 0.0f) return false;
	else *t = b - sqrt(disc);

	if(*t < 0.0f)
	{
		*t = b + sqrt(disc);
		if (*t < 0.0f) return false; 
	}
	
	return true;
}

bool intersect_plane(__global const Plane *plane, const struct Ray *ray, float *t)
{
	float denom = dot(plane->normal, ray->direction);

	if(fabs(denom) == 0.f) return false;

	float tmp = dot(plane->normal, plane->position - ray->origin ) / denom;

	// Backwards intersection
	if(tmp < 0.f) return false;

	*t = tmp;
	
	return true;
}

float intersect_triangle(__global const Triangle *triangle, const Ray *ray, float *t)
{
	float3 edge1, edge2, h, s, q;
	float a, f, u, v;

	edge1 = triangle->vertices[1] - triangle->vertices[0];
	edge2 = triangle->vertices[2] - triangle->vertices[0];

	h = cross(ray->direction, edge2);
	a = dot(edge1, h);

	if(a == 0) return false;

	f = 1.f / a;
	s = ray->origin - triangle->vertices[0];
	u = f * dot(s, h);
	
	if(u < 0.f || u > 1.f) return false;

	q =	cross(s, edge1);
	v = f * dot(ray->direction, q);

	if(v < 0.f || u + v > 1.f) return false;

	*t = f * dot(edge2, q);
	if(*t > 0.f)
	{
		return true;
	}

	return false;
}

__global const Material *closest_intersection(const Scene *scene, const Ray *ray, Intersection *rayhit)
{
	__global const Material *closest = NULL;
	float tmin = FLT_MAX;

	for(int i = 0; i < scene->data->numShapes; i++) {
		__global const Shape *shape = &scene->shapes[i];
		if(shape->type == SHAPE_SPHERE) {
			__global const Sphere *sphere = &shape->shape.sphere;

			float t_i;
			if(intersect_sphere(sphere, ray, &t_i))
			{
				if(t_i < tmin)
				{
					tmin = t_i;
					closest = &sphere->material;
		
					if(rayhit != NULL)
					{ 
						rayhit->position = ray->origin + ray->direction * tmin;
						rayhit->normal = normalize(rayhit->position - sphere->position);
						rayhit->position += rayhit->normal * 0.001f;
					}
				}
			}
		}
		else if(shape->type == SHAPE_TRIANGLE) {
			__global const Triangle *triangle = &shape->shape.triangle;

			float t_i;
			if(intersect_triangle(triangle, ray, &t_i))
			{
				if(t_i < tmin)
				{
					tmin = t_i;
					closest = &triangle->material;

					if(rayhit != NULL)
					{
						float3 A = triangle->vertices[1] - triangle->vertices[0];
						float3 B = triangle->vertices[2] - triangle->vertices[0];

						rayhit->normal = normalize((float3)(A.y * B.z - A.z * B.y, A.z * B.x - A.x * B.z, A.x * B.y - A.y * B.x));
						rayhit->normal = dot(rayhit->normal, ray->direction) < 0.0f ? rayhit->normal : -rayhit->normal; // Reflect normal to face camera
						rayhit->position = ray->origin + ray->direction * tmin + rayhit->normal * .001f;
					}
				}
			}
		}
		else if(shape->type == SHAPE_PLANE) {
			__global const Plane *plane = &shape->shape.plane;

			float t_i;
			if(intersect_plane(plane, ray, &t_i))
			{
				if(t_i < tmin)
				{
					tmin = t_i;
					closest = &plane->material;
		
					if(rayhit != NULL)
					{ 
						rayhit->normal = dot(plane->normal, ray->direction) < 0.0f ? plane->normal : -plane->normal; // Reflect normal to face camera
						rayhit->position = ray->origin + ray->direction * tmin + rayhit->normal * 0.001f;
					}
				}
			}
		}
	}

	if(tmin == FLT_MAX) return NULL;
	
	return closest;
}

float3 trace(const struct Scene *scene, struct Ray *camray, uint seed)
{
	const float metallic = 0.3f;
	
	float3 color = (float3)(0.f);
	float3 mask = (float3)(1.f);

	Ray ray = *camray;
	Intersection rayhit;

	for(size_t i = 0; i < MAX_BOUNCE; i++)
	{
		__global const Material *material = closest_intersection(scene, &ray, &rayhit);
	
		if(material != NULL)
		{
			if(material->type == MATERIAL_REFLECTIVE)
			{
				float3 reflected = reflect(ray.direction, rayhit.normal);
				
				ray.direction = reflected;
				
				mask *= (float3)(1 - metallic) + material->color*(metallic);
			}
			else
			{ 
				float3 toLightsource = normalize(scene->data->lightSource - rayhit.position);

				/* Ray toLight = { .origin = rayhit.position, .direction = toLightsource }; */

				/* float3 reflected = reflect(-toLightsource, rayhit.normal); */
				
				/* if(closest_intersection(scene, &toLight, NULL) == NULL) // if not in shadow */
				/* {  */
				/* 	float3 phong = ( */
				/* 		material->color * max(dot(toLightsource, rayhit.normal), 0.f) + */
				/* 		material->specular * pow(max(dot(reflected, -ray.direction), 0.f), material->specularExponent) */
				/* 	); */
				/* 	color += mask*phong; */
				/* } */
				
				mask *= material->color;

				if(i == MAX_BOUNCE) break; // Don't compute new bounce if it's the last one
				
				// Generate new diffuse rays
				seed = wang_hash(seed);
				float rand1 = (float)seed / (float)UINT_MAX * 2 * M_PI_F;
				seed = wang_hash(seed);
				float rand2 = (float)seed / (float)UINT_MAX;
				float rand2s = sqrt(rand2);

				// Create local orthogonal plane centered at hitpoint
				float3 w = rayhit.normal;
				float3 axis = fabs(w.x) > 0.1f ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
				float3 u = normalize(cross(axis, w));
				float3 v = cross(w, u);

				// Use random numbers and coordinate frame to generate a new ray in a half circle
				ray.origin = rayhit.position;
				ray.direction = normalize(u * cos(rand1)*rand2s + v*sin(rand1)*rand2s + w*sqrt(1.0f - rand2));

				mask *= dot(ray.direction, rayhit.normal);
			}
		}
		else // No collision -- Sky
		{
			const float intensity = 1000.0f;

			if(dot(normalize(scene->data->lightSource - ray.origin), ray.direction) < 0.95f) // If not in the sun
				mask *= (float3)(.3f, .65f, 1.f); // Sky color
			else
				mask *= (float3)(intensity);

			color += mask;
			break;
		}
	}
	
	return clamp(color, (float3)(0.f), (float3)(1.f));
}

float3 aces(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;

    return (x*(x*a + b))/(x*(x*c + d) + e);
}

float random_float(uint *seed) {
	*seed = wang_hash(*seed);
	return ((float)*seed) / (float)UINT_MAX;
}

__kernel void render(const struct RenderData data, const struct SceneData sceneData, __global float3 *output, __global const Shape *shapes)
{
	const uint id = get_global_id(0);

	Scene scene = { .data = &sceneData, .shapes = shapes };
	float2 windowPos = (float2)(id % data.width, id / data.width); // Raster space coordinates
	
	float3 color = (float3)(0.f);
	for(int sample = 0; sample < data.numSamples; sample++)
	{ 
		uint seed = (sample + id*data.numSamples)*data.time*5304;

		float2 ndcPos = (float2)((windowPos.x + random_float(&seed)) / data.width, (windowPos.y + random_float(&seed)) / data.height); // Normalized coordinates
		float2 screenPos = (float2)((2.f * ndcPos.x - 1.f) * data.aspectRatio * data.fieldOfViewScale, (1.f - 2.f * ndcPos.y) * data.fieldOfViewScale); // Screen space coordinates (invert y axis)
		float3 cameraPos = (float3)(screenPos.x, screenPos.y, 1);

		Ray ray;
		// 1 0 0 x 
		// 0 1 0 y
		// 0 0 1 z
		// 0 0 0 1
		// Get translation part
		ray.origin = data.cameraToWorldMatrix[3].xyz;

		// vec4 with 0 at the end is only affected by rotation, not translation
		// Only normalize 3d components
		ray.direction = normalize(
			matrixByVector(data.cameraToWorldMatrix, (float4)(cameraPos.xyz, 0)).xyz
		); 

		color += trace(&scene, &ray, seed/* + (data.time<<3)*/);
	}
	color /= data.numSamples;

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
