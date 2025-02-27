#pragma once

#include "intersections.h"
#include <math.h>

// CHECKITOUT
/**
 * Computes a cosine-weighted random direction in a hemisphere.
 * Used for diffuse lighting.
 */
__host__ __device__
glm::vec3 calculateRandomDirectionInHemisphere(
        glm::vec3 normal, thrust::default_random_engine &rng) {
    thrust::uniform_real_distribution<float> u01(0, 1);

    float up = sqrt(u01(rng)); // cos(theta)
    float over = sqrt(1 - up * up); // sin(theta)
    float around = u01(rng) * TWO_PI;

    // Find a direction that is not the normal based off of whether or not the
    // normal's components are all equal to sqrt(1/3) or whether or not at
    // least one component is less than sqrt(1/3). Learned this trick from
    // Peter Kutz.

    glm::vec3 directionNotNormal;
    if (abs(normal.x) < SQRT_OF_ONE_THIRD) {
        directionNotNormal = glm::vec3(1, 0, 0);
    } else if (abs(normal.y) < SQRT_OF_ONE_THIRD) {
        directionNotNormal = glm::vec3(0, 1, 0);
    } else {
        directionNotNormal = glm::vec3(0, 0, 1);
    }

    // Use not-normal direction to generate two perpendicular directions
    glm::vec3 perpendicularDirection1 =
        glm::normalize(glm::cross(normal, directionNotNormal));
    glm::vec3 perpendicularDirection2 =
        glm::normalize(glm::cross(normal, perpendicularDirection1));

    return up * normal
        + cos(around) * over * perpendicularDirection1
        + sin(around) * over * perpendicularDirection2;
}

/**
 * Scatter a ray with some probabilities according to the material properties.
 * For example, a diffuse surface scatters in a cosine-weighted hemisphere.
 * A perfect specular surface scatters in the reflected ray direction.
 * In order to apply multiple effects to one surface, probabilistically choose
 * between them.
 *
 * The visual effect you want is to straight-up add the diffuse and specular
 * components. You can do this in a few ways. This logic also applies to
 * combining other types of materias (such as refractive).
 *
 * - Always take an even (50/50) split between a each effect (a diffuse bounce
 *   and a specular bounce), but divide the resulting color of either branch
 *   by its probability (0.5), to counteract the chance (0.5) of the branch
 *   being taken.
 *   - This way is inefficient, but serves as a good starting point - it
 *     converges slowly, especially for pure-diffuse or pure-specular.
 * - Pick the split based on the intensity of each material color, and divide
 *   branch result by that branch's probability (whatever probability you use).
 *
 * This method applies its changes to the Ray parameter `ray` in place.
 * It also modifies the color `color` of the ray in place.
 *
 * You may need to change the parameter list for your purposes!
 */
__host__ __device__
void scatterRay(
        PathSegment & pathSegment,
        glm::vec3 intersect,
        glm::vec3 normal,
        const Material &m, bool outside, float t,
        thrust::default_random_engine &rng) {
    // TODO: implement this.
    // A basic implementation of pure-diffuse shading will just call the
    // calculateRandomDirectionInHemisphere defined above.
    thrust::uniform_real_distribution<float> u01(0, 1); // For non perfect reflection and diffusion
    if (m.hasReflective > 0.99f && m.hasRefractive > 0.99f) {
        float n1 = outside ? 1.f : m.indexOfRefraction;
        float n2 = outside ? m.indexOfRefraction : 1.f;
        float r = pow((n1 - n2) / (n1 + n2), 2.0f);
        float reflectance = r + (1.f - r) * pow(1.f + glm::dot(normal, pathSegment.ray.direction), 5.0f); // + cos instead of - because ray direction is flipped
        if (u01(rng) < reflectance) {
            pathSegment.ray.origin = intersect;
            pathSegment.ray.direction = glm::reflect(pathSegment.ray.direction, normal);
            pathSegment.color *= m.specular.color;
        }
        else {            
            glm::vec3 refracted = glm::refract(pathSegment.ray.direction, normal, n1 / n2);
            if (glm::length(refracted) < 0.01f) {
                pathSegment.ray.origin = intersect;
                pathSegment.ray.direction = glm::reflect(pathSegment.ray.direction, normal);
                pathSegment.color *= m.specular.color;
            }
            else {
                if (outside) {
                    pathSegment.ray.origin = getPointOnRayFurther(pathSegment.ray, t);
                }
                else {
                    pathSegment.ray.origin = intersect;
                }
                pathSegment.ray.direction = refracted;
                pathSegment.color *= m.color;
            }
            
        }
    } 
    else if (u01(rng) < m.hasReflective / (m.hasReflective + m.diffusion + 0.0001f)) { // The epsilon is used to deal with division by zero
        pathSegment.ray.origin = intersect;
        pathSegment.ray.direction = glm::reflect(pathSegment.ray.direction, normal);
        float probability = m.hasReflective / (m.hasReflective + m.diffusion + 0.0001f);
        pathSegment.color *= m.specular.color;
    }
    else {
        pathSegment.ray.origin = intersect;
        pathSegment.ray.direction = calculateRandomDirectionInHemisphere(normal, rng);
        pathSegment.color *= m.color;
    }
}
