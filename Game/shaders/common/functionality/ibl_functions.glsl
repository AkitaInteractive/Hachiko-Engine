#ifndef _IBL_FUNCTIONS_INCLUDE_
#define _IBL_FUNCTIONS_INCLUDE_

#define NUM_SAMPLES 100
#define PI 3.14159

float radicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Hammersley a simple low discrepancy (quasi-random numbers that are more evenly distributed) sequence that can be quickly computed 
vec2 hammersley2D(uint i, uint N) 
{
    return vec2(float(i)/float(N), radicalInverse_VdC(i));
}

vec3 hemisphereSampleGGX(float u1, float u2, float roughness)
{ 
    float phi = 2.0 * PI * u1;
    float cos_theta = sqrt((1.0 - u2) / (u2 * (roughness * roughness - 1) + 1));
    float sin_theta = sqrt(1 - cos_theta * cos_theta);

    // spherical to cartesian conversion
    vec3 dir;
    dir.x = cos(phi) * sin_theta;
    dir.y = sin(phi) * sin_theta;
    dir.z = cos_theta;
    return dir;
}

float SmithVSF(in float NdotL, in float NdotV, in float roughness)
{
    float denom = NdotL * (NdotV * (1 - roughness) + roughness) + NdotV * (NdotL * (1 - roughness) + roughness);
    return 0.5 / denom;
}

vec3 hemisphereSample(float u1, float u2)
{
    float phi = u1 * 2.0 * PI;
    float r = sqrt(u2);
    return vec3( r*cos(phi), r*sin(phi), sqrt(1-u2));
}

mat3 computeTangetSpace(in vec3 normal)
{
    vec3 up = abs(normal.y) > 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = cross(normal, right);
    return mat3(right, up, normal);
}

#endif
