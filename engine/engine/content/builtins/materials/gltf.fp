#version 140

#include "/builtins/materials/gltf_sampling.glsl"
#include "/builtins/materials/gltf_lighting.glsl"

out vec4 out_fragColor;

void main()
{
    PBRMaterial inputs = sample_pbr_material(var_texcoord0, var_color);
    PBRSurface surface = get_pbr_surface(inputs);

    // Sample all material textures, including the normal map, before discarding.
    // Implicit texture derivatives need neighboring fragments at cutout edges.
    if ((!inputs.doubleSided && !gl_FrontFacing) || pbr_alpha_discard(inputs))
    {
        discard;
    }

    // Modify inputs here before deriving dependent BRDF properties.
    MaterialInfo material = get_material_info(inputs);
    PBRLighting lighting = empty_pbr_lighting();

    if (!inputs.unlit)
    {
        lighting.direct = evaluate_pbr_direct(material, surface, var_view);

        // Replace this with an IBL/GI contribution in an extension shader.
        lighting.indirect = evaluate_pbr_ambient(material, ambient_light());
    }

    vec3 color = composite_pbr_lighting(inputs, lighting, inputs.occlusion, inputs.occlusion);

    // HDR renderers can write linear color here and convert in their final pass.
    out_fragColor = vec4(to_output(color), get_pbr_alpha(inputs));
}
