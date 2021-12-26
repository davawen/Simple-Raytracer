struct RenderData
{
	int width, height;
	float aspectRatio, fieldOfViewScale;

	float4 cameraToWorldMatrix[4];
};

struct SceneData
{
	int numSpheres;
	
	float3 lightSource;
};

struct Ray
{
	float3 origin;
	float3 direction;
};

struct Intersection
{
	float3 position;
	float3 normal;
};

struct Material
{
	float3 color;
	float3 specular;
	float specularExponent;
};

struct Sphere
{
	struct Material material;
	float3 position;
	float radius;
};

struct Plane
{
	struct Material material;
	float3 position;
	float3 normal;
};

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

bool intersect_sphere(constant struct Sphere *sphere, const struct Ray *ray, float *t)
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

bool intersect_plane(constant struct Plane *plane, const struct Ray *ray, float *t)
{
	float denom = dot(plane->normal, ray->direction);
	if(denom > 0.0f) return false;
	*t = -dot(plane->normal, ray->origin - plane->position) / denom;
	
	return true;
}

constant struct Sphere *closest_intersection(const struct SceneData *scene, constant struct Sphere *spheres, const struct Ray *ray, struct Intersection *rayhit)
{
	constant struct Sphere *closest = NULL;
	float tmin = FLT_MAX;

	for(size_t i = 0; i < scene->numSpheres; i++)
	{
		float t_i;
		if(intersect_sphere(&spheres[i], ray, &t_i))
		{
			if(t_i < tmin)
			{
				tmin = t_i;
				closest = &spheres[i];
			}
		}
	}

	if(tmin == FLT_MAX) return NULL;
	
	if(rayhit != NULL)
	{ 
		rayhit->position = ray->origin + ray->direction * tmin;
		rayhit->normal = normalize(rayhit->position - closest->position);
		rayhit->position += rayhit->normal * 0.001f;
	}
	
	return closest;
}

float3 trace(const struct SceneData *scene, global const struct Sphere *spheres, const struct Ray *camray)
{
	struct Intersection rayhit;
	struct Ray ray = *camray;
	
	float3 color = (float3)(0.f);
	float3 mask = (float3)(1.f);
	
	// float seed = (wang_hash((uint)(camray->origin.x*100) + (uint)(camray->origin.y*100)*100) % 1000);
	
	for(size_t i = 1; i <= 1; i++)
	{
		constant struct Sphere *sphere = closest_intersection(scene, spheres, &ray, &rayhit);
	
		if(sphere != NULL)
		{
			float3 toLightsource = normalize(scene->lightSource - rayhit.position);
			
			float3 reflected = reflect(-toLightsource, rayhit.normal);
			
			float3 phong = (sphere->material.color * (float3)(.3f, .65f, 1.f) * .1f) +
				sphere->material.color * max(dot(toLightsource, rayhit.normal), 0.f) +
				sphere->material.specular * pow(max(dot(reflected, -camray->direction), 0.f), sphere->material.specularExponent);
			
			color = phong;
			
			// scene->lightSource
			// mask *= (float3)(0.6f, 0.6f, 0.6f);
			
			// float3 normal = normalize(hitpoint - sphere->position);
			// normal = dot(normal, camray->direction) < 0.0f ? normal : -normal;
			
			// ray.origin = hitpoint + normal * 0.001f;
			// ray.direction = normalize(ray.direction - 2.0f * dot(normal, ray.direction) * normal);
			
			// mask *= dot(ray.direction, normal);
		}
		else
		{
			if(dot(normalize(scene->lightSource - camray->origin), camray->direction) > 0.97f) color = (float3)(1.f);
			else color = (float3)(.3f, .65f, 1.f); // Sky color
			break;
		}
	}
	
	return clamp(color, (float3)(0.f), (float3)(1.f));
}


kernel void render(const struct RenderData data, const struct SceneData scene, global uchar4 *output, constant struct Sphere *spheres)
{
	const uint i = get_global_id(0);

	float2 windowPos = (float2)(i % data.width, i / data.width); // Raster space coordinates

	float2 ndcPos = (float2)((windowPos.x + .5f) / data.width, (windowPos.y + .5f) / data.height); // Normalized coordinates

	float2 screenPos = (float2)((2.f * ndcPos.x - 1.f) * data.aspectRatio * data.fieldOfViewScale, (1.f - 2.f * ndcPos.y) * data.fieldOfViewScale); // Screen space coordinates (invert y axis)

	float3 cameraPos = (float3)(screenPos.x, screenPos.y, 1);

	struct Ray ray;
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

	float3 color = trace(&scene, spheres, &ray);

	// ARGB
	output[i] = (uchar4)(255, color.x*255.f, color.y*255.f, color.z * 255.f);
}

