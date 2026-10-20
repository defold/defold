#version 140

#include "/builtins/materials/gltf_sampling.glsl"
#include "/builtins/materials/gltf_lighting.glsl"
#include "/builtins/materials/gltf_transmission.glsl"

out vec4 out_fragColor;

void main()
{
    PBRMaterial inputs = pbr_sample_material(var_texcoord0, var_color);

    // Edit sampled material properties here, before sidedness, alpha testing
    // and BRDF derivation. A custom material sampler can replace the call above.
    PBRSurface surface = pbr_sample_surface(inputs, var_texcoord0);
    vec2 scene_pixel_size = pbr_transmission_pixel_size(surface.position, mtx_projection);

    // Edit view-space surface fields here (normals are already facing the correct
    // side), then call pbr_finalize_surface(surface, var_view) to refresh world*.
    // Procedural geometry/normals can use pbr_create_surface() instead of sampling.

    // Sample all material textures, including the normal map, before discarding.
    // Implicit texture derivatives need neighboring fragments at cutout edges.
    if ((!inputs.doubleSided && !gl_FrontFacing) || pbr_should_discard_alpha(inputs))
    {
        discard;
    }

    // Derive BRDF properties after material edits. Rebuild if the inputs change.
    MaterialInfo material = pbr_create_material_info(inputs);
    PBRLighting lighting = pbr_create_lighting();

    if (!inputs.unlit)
    {
        lighting.direct = pbr_evaluate_direct_lighting(material, surface, var_view);

        // Replace this with an IBL/GI contribution in an extension shader.
        lighting.indirect = pbr_evaluate_ambient_lighting(material, ambient_light());
    }

    vec3 color = pbr_compose_lighting(inputs, lighting, inputs.occlusion, inputs.occlusion);
    float transmission = pbr_saturate(inputs.transmission);
    if (transmission_params.x > 0.5 && !inputs.unlit && transmission > 0.0 && material.metallic < 1.0)
    {
        vec3 ray = pbr_volume_ray(inputs, surface, var_world_to_local);
        vec3 exit_position = surface.position + mat3(var_view) * ray;
        vec4 clip = mtx_projection * vec4(exit_position, 1.0);
        vec2 scene_uv = clip.xy / clip.w * 0.5 + 0.5;
        // Opaque capture uses the same output conversion as this shader.
        vec3 background = pbr_sample_transmission(scene_uv, scene_pixel_size, inputs.roughness, inputs.ior);
        color = pbr_compose_transmission(inputs, material, surface, lighting, transmission, background, length(ray), inputs.occlusion, inputs.occlusion);
    }

    // HDR renderers can write linear color here and convert in their final pass.
    out_fragColor = vec4(pbr_linear_to_srgb(color), pbr_get_alpha(inputs));
}
