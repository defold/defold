#version 140

#include "/builtins/materials/gltf_sampling.glsl"
#include "/builtins/materials/gltf_lights.glsl"

out vec4 out_fragColor;

void main()
{
    PBRMaterial inputs = sample_pbr_material(var_texcoord0, var_color);
    if ((!inputs.doubleSided && !gl_FrontFacing) || pbr_alpha_discard(inputs))
        discard;

    // Modify inputs here before deriving dependent BRDF properties.
    MaterialInfo material = get_material_info(inputs);
    PBRSurface surface = get_pbr_surface(inputs);
    PBRLighting lighting = empty_pbr_lighting();
    if (!inputs.unlit)
    {
        lighting.direct = evaluate_pbr_direct(material, surface, var_view);
        // Replace this with an IBL/GI contribution in an extension shader.
        lighting.indirect = evaluate_pbr_ambient(material, get_pbr_ambient());
    }
    vec3 color = composite_pbr_lighting(inputs, lighting, inputs.occlusion, inputs.occlusion);
    // HDR renderers can write linear color here and convert in their final pass.
    out_fragColor = vec4(to_output(color), get_pbr_alpha(inputs));
}
