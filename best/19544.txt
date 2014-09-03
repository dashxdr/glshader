// Understanding RayMarchers

#ifdef GL_ES
precision mediump float;
#endif
 
uniform float time;
uniform vec2 resolution;
uniform vec2 mouse;
 
#define EPSILON 0.001
#define INFINITY 999999999.0

#define GEOMETRY_TYPE_SPHERE 0
#define GEOMETRY_TYPE_PLANE 1
#define GEOMETRY_TYPE_TORUS 2

 
#define MATERIAL_FUNCTION_NONE 0
#define MATERIAL_FUNCTION_CHECKERBOARD 1

#define GEOMETRY_COUNT 5
#define LIGHT_COUNT 2

struct Light {
        vec3 direction;
        vec3 color;
        float power;
        bool castsShadows;
};
 
struct Ray {
        vec3 position;
        vec3 direction;
};
 
struct Material {
		vec3 ambientColor;
        vec3 diffuseColor;
        vec3 specularColor;
        int type;
};
 
struct Hit {
        vec3 point;
        vec3 normal;
        Material material;
        vec3 direction;
        bool didHitObject;
};
 
struct Geometry {
        vec3 position;
        Material material;
        int geometryType;
        vec4 info1;
        vec4 info2;
};
 
// Global data
 
Geometry geometries[GEOMETRY_COUNT];
Light lights[LIGHT_COUNT];
 
// Intitializers
 
Material createSimpleMaterial(vec3 color) {
        return Material(color,color,vec3(1.0),MATERIAL_FUNCTION_NONE);
}

vec3 checkerBoard(vec3 point) {
	bool m1 = mod(point.x,1.0) > 0.5;
	bool m2 = mod(point.z,1.0) > 0.5;

	if (m1 ^^ m2) {
		return vec3(1.0,1.0,1.0);
	} else {
		return vec3(0.1,0.1,0.1);
	}
	
}
vec3 diffuseColorForMaterial(Material material,vec3 point) {
	if (material.type == MATERIAL_FUNCTION_NONE) {
		return material.diffuseColor;
	}
	if (material.type == MATERIAL_FUNCTION_CHECKERBOARD) {
		return checkerBoard(point);
	}

	return vec3(0.0);
}

vec3 ambientColorForMaterial(Material material, vec3 point) {
	return diffuseColorForMaterial(material,point);
}


 
Geometry createSphere(vec3 point,float radius,Material material) {
    Geometry sphere;
    sphere.geometryType = GEOMETRY_TYPE_SPHERE;
    sphere.position = point;
    sphere.info1.x = radius;
    sphere.material = material;
    
	return sphere;
}

Geometry createPlane(vec3 normal, float offset,Material material) {
	Geometry plane;
	plane.geometryType = GEOMETRY_TYPE_PLANE;
	plane.info1.xyz = normal;
	plane.info1.w = offset;
	plane.material = material;

	return plane;
}

Geometry createTorus(vec3 position,float a,float b,Material material) {
	Geometry torus;
	torus.geometryType = GEOMETRY_TYPE_TORUS;
	torus.position = position;
	torus.info1.x = a;
	torus.info1.y = b;
	torus.material = material;
	
	
	return torus;
}
 
// Distance Functions
 
float sphere(vec3 point, Geometry sphere) {
 	return length(point - sphere.position) - sphere.info1.x;
}
 
float plane(vec3 point, Geometry plane) {
	 return dot(point,plane.info1.xyz) + plane.info1.w;
}

float torus(vec3 point, Geometry torus) {
	point -= torus.position;
	vec2 q = vec2(length(point.xz)-torus.info1.x,point.y);
  	return length(q)-torus.info1.y;
}
 
float distanceForGeometry(vec3 point,Geometry geometry) {
        if (geometry.geometryType == GEOMETRY_TYPE_SPHERE) {
                return sphere(point,geometry);
        }
 
        if (geometry.geometryType == GEOMETRY_TYPE_PLANE) {
                return plane(point,geometry);
        }
	
	if (geometry.geometryType == GEOMETRY_TYPE_TORUS) {
		return torus(point,geometry);
	}
 
        return 0.0;
}

vec3 normalForGeometry(vec3 point,Geometry geometry) {
	const vec2 helper = vec2(EPSILON,0.0);

	return normalize(vec3(distanceForGeometry(point + helper.xyy,geometry) - distanceForGeometry(point - helper.xyy,geometry),
			      distanceForGeometry(point + helper.yxy,geometry) - distanceForGeometry(point - helper.yxy,geometry),
			      distanceForGeometry(point + helper.yyx,geometry) - distanceForGeometry(point - helper.yyx,geometry))
			);
}
 
Ray rayFromCamera(vec3 cameraOrigin,vec3 cameraTarget,vec2 position) {
	vec3 upDirection = vec3(0.0, 1.0, 0.0);
 
	vec3 cameraDir = normalize(cameraTarget - cameraOrigin);
	
	vec3 cameraRight = normalize(cross(cameraDir, upDirection));
	vec3 cameraUp = cross(cameraRight,cameraDir);
	
	vec3 rayDir = normalize(cameraRight * position.x + cameraUp * position.y + 1.5 * cameraDir);


	return Ray(cameraOrigin,rayDir);
}

Geometry closestGeometry(vec3 point,out float dist) {
	float bestDistance = INFINITY;
	Geometry bestGeometry;
	for (int j = 0; j < GEOMETRY_COUNT; ++j) {
		Geometry geometry = geometries[j];
		float d = distanceForGeometry(point,geometry);

		if (d < bestDistance) {
			bestDistance = d;
			bestGeometry = geometry;
		}
	}

	dist = bestDistance;
	return bestGeometry;
}

Hit rayMarch(Ray ray) {
	const int iterationCount = 128;
	const float maxDistance = 600.0;

	float dist = EPSILON;
	float totalDistance = 0.0;
	Geometry geometry;

	for (int i = 0; i < iterationCount; ++i) {
		ray.position += dist * ray.direction;
		geometry = closestGeometry(ray.position,dist);
		totalDistance += dist;
		if (totalDistance > maxDistance || dist < EPSILON) {
			break;
		}

	}

	Hit hit;

	if (dist < EPSILON) {
		hit.didHitObject = true;
		hit.material = geometry.material;
		hit.point = ray.position; 
		hit.normal = normalForGeometry(hit.point, geometry);
		hit.direction = ray.direction;

		return hit;
	}

	hit.didHitObject = false;
	return hit;
}

float softshadow( in vec3 ro, in vec3 rd, float mint, float maxt, float k )
{
    float res = 1.0;
    float dt = 0.1;
    float t = mint;
    for( int i=0; i<30; i++ )
    {
    	float h;
        closestGeometry(ro + rd*t,h);
        if( h>0.001 )
            res = min( res, k*h/t );
        else
            res = 0.0;
        t += dt;
    }
    return res;
}


vec3 colorForHit(Hit hit) {
	// Compute shading

	const float ambientFactor = 0.2;
	const float diffuseFactor = 1.0;
	const float specularFactor = 0.3;


	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);


	vec3 reflected = reflect(hit.direction,hit.normal);

	float shadow = 1.0;
	
	for (int i = 0; i < LIGHT_COUNT; ++i) {
		Light light = lights[i];
		diffuse += light.power * light.color * clamp(dot(light.direction,hit.normal),0.0,1.0);

		specular += light.power * light.color * pow(clamp(dot(light.direction,reflected),0.0,1.0),32.0);
		
		if (light.castsShadows) {
			shadow += softshadow(hit.point, light.direction, 0.06, 4.0, 4.0 );
		}
		
	}

	
	vec3 ambientColor = ambientColorForMaterial(hit.material,hit.point);
	vec3 diffuseColor = diffuseColorForMaterial(hit.material,hit.point);

	vec3 ambient = ambientFactor * ambientColor;
	diffuse = diffuseFactor * diffuseColor * diffuse;
	specular = specularFactor * hit.material.specularColor * specular;
	
	float sh = shadow * 0.5;
	diffuse *= vec3(sh, (sh+sh*sh)*0.5, sh*sh );
	
	vec3 color = ambient + diffuse + specular;
	
	return color;
}

vec2 worldPosition() {
    float aspectRatio = resolution.x / resolution.y;
    vec2 position = gl_FragCoord.xy / resolution.xy;
	
    position *= 2.0;
    position -= 1.0;
    position.x *= aspectRatio;
	
    return position;
}

vec3 recursiveRayMarch(Ray ray) {
	const int iterationCount = 3;
	float e = 1.0;
	vec3 result = vec3(0.0);
	for (int i = 0; i < iterationCount; ++i) {
		Hit hit = rayMarch(ray);
		if (hit.didHitObject) {

			result += e * colorForHit(hit);
			e /= 3.0;
			
			ray.position = hit.point + hit.normal * 0.01;
			ray.direction = normalize(reflect(ray.direction, hit.normal));
		} else {
			break;
		}
	}
	return result;
	
}

void main(void) {
	
    vec2 position = worldPosition();

    Material redMaterial = createSimpleMaterial(vec3(1.0,0.0,0.0));
    Material greenMaterial = createSimpleMaterial(vec3(0.0,1.0,0.0));
    Material pinkMaterial = createSimpleMaterial(vec3(0.8,0.1,0.6));
    Material grayMaterial = createSimpleMaterial(vec3(0.8,0.8,0.8));
    Material yellowMaterial = createSimpleMaterial(vec3(1.0,1.0,0.0));
    grayMaterial.type = MATERIAL_FUNCTION_CHECKERBOARD;
	
    geometries[0] = createSphere(vec3(0.0),0.4,redMaterial);
    geometries[1] = createSphere(vec3(1.0,0.0,0.0),0.3,greenMaterial);
    geometries[2] = createSphere(vec3(-1.5,0.0,-1.0),0.5,pinkMaterial);
    geometries[3] = createPlane(vec3(0.0,1.0,0.0),1.0,grayMaterial);
    geometries[4] = createTorus(vec3(2.1,0.3,0.0),0.5,0.2,yellowMaterial);
	
    vec2 polar = vec2(5.0 - mouse.y * 3.0,mouse.x * 6.28);
   	
    vec3 cameraOrigin = vec3(polar.x * cos(polar.y),1.0,polar.x * sin(polar.y));
    vec3 cameraTarget = vec3(0.0,0.0,0.0);

    lights[0] = Light(normalize(vec3(1.0,1.0,1.0)),vec3(1.0),1.0,true);
    lights[1] = Light(normalize(vec3(-1.0,1.0,1.0)),vec3(0.0,0.0,0.7),0.5,false);
	
    Ray ray = rayFromCamera(cameraOrigin,cameraTarget,position);
    
    vec3 color = recursiveRayMarch(ray);
    gl_FragColor = vec4(color, 1.0);
}