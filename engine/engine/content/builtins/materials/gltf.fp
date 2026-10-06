#version 140

#include "/builtins/materials/gltf_sampling.glsl"
#include "/builtins/materials/gltf_lighting.glsl"

out vec4 out_fragColor;

void main()
{
    PBRMaterial inputs = pbr_sample_material(var_texcoord0, var_color);
    PBRSurface surface = pbr_sample_surface(inputs);

    // Sample all material textures, including the normal map, before discarding.
    // Implicit texture derivatives need neighboring fragments at cutout edges.
    if ((!inputs.doubleSided && !gl_FrontFacing) || pbr_should_discard_alpha(inputs))
    {
        discard;
    }

    // Modify inputs here before deriving dependent BRDF properties.
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
