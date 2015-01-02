precision highp float; 
uniform vec2 resolution;
uniform float time;
const float pi = 3.141592653589793;
float smin(in float a, in float b, in float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float map(in vec3 p) {
    vec3 q = mod(p + 2.0, 4.0) - 2.0;
    float d1 = length(q) - 1.0;
    d1 += 0.1 * sin(p.x * 10.0) * sin(p.y * 10.0 + time) * sin(p.z * 10.0);
    float d2 = p.y + 1.0;
    float d = smin(d2, d1, 1.0);
    return d;
}

vec3 calcNormal(in vec3 p) {
    vec2 e = vec2(1.0, -1.0) * 0.001;
    return normalize(vec3(
        e.xyy * map(p + e.xyy) +
        e.yxy * map(p + e.yxy) +
        e.yyx * map(p + e.yyx) +
        e.xxx * map(p + e.xxx)
    ));
}

void main() { 
    vec2 p = gl_FragCoord.xy / resolution;
    p = 2.0 * p - 1.0;
    p.x *= resolution.x / resolution.y;
    vec3 ro = vec3(0.0, 1.4, 2.0 - time * 0.3);
    vec3 rd = normalize(vec3(p, -1.0));
    float maxt = 20.0;
    float h = 0.002;
    float t = 0.0;
    vec3 col = vec3(0.0);
    for(int i = 0; i < 60; i++) {
        if(h < 0.001 || t > maxt) continue;
        h = map(ro + rd * t);
        t += h;
    }
    
    if(t < maxt) {
        vec3 pos = ro + rd * t;
        vec3 nor = calcNormal(pos);
        vec3 lig = normalize(vec3(1.0));
        float d = clamp(dot(nor, lig), 0.0, 1.0);
        float s = pow(clamp(dot(reflect(rd, nor), lig), 0.0, 1.0), 32.0);
        col = 0.75 * vec3(1.0, nor.y, 0.3 * nor.z) + vec3(d + s) * clamp(1.0 + dot(rd, nor), 0.0, 1.0);
        col *= exp(-t * 0.1);
    }
    gl_FragColor = vec4(col, 1.0);
}