#version 140

#include "/builtins/materials/gltf_sampling.glsl"
#include "/builtins/materials/gltf_lighting.glsl"

out vec4 out_fragColor;

void main()
{
    PBRMaterial inputs = pbr_sample_material(var_texcoord0, var_color);

    // Edit sampled material properties here, before sidedness, alpha testing
    // and BRDF derivation. A custom material sampler can replace the call above.
    PBRSurface surface = pbr_sample_surface(inputs, var_texcoord0);

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

    // HDR renderers can write linear color here and convert in their final pass.
    out_fragColor = vec4(pbr_linear_to_srgb(color), pbr_get_alpha(inputs));
}
