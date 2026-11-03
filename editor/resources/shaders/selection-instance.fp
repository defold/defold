#version 140

in mediump vec2 var_texcoord0;
in lowp vec4 var_id_color;
in lowp float var_alpha;

uniform mediump sampler2D texture_sampler;
// X: glTF alpha mode (-1 for legacy picking), Y: cutoff,
// Z: base-color factor alpha, W: base-color texture presence.
uniform fragment_uniforms {
    mediump vec4 alpha_parameters;
};

out vec4 out_color;

void main() {
    if (alpha_parameters.x < 0.0) {
        // Preserve the existing selection threshold for non-glTF materials.
        if (texture(texture_sampler, var_texcoord0).a <= 0.05) {
            discard;
        }
    } else if (alpha_parameters.x > 0.5) {
        float alpha = alpha_parameters.z * var_alpha;
        if (alpha_parameters.w > 0.5) {
            alpha *= texture(texture_sampler, var_texcoord0).a;
        }
        // MASK uses exactly the visible pass's combined alpha and cutoff.
        // BLEND retains the editor's threshold for selecting translucent pixels.
        if (alpha_parameters.x < 1.5 ? alpha < alpha_parameters.y : alpha <= 0.05) {
            discard;
        }
    }
    // OPAQUE ignores alpha, including fully transparent base-color texels.
    out_color = var_id_color;
}
