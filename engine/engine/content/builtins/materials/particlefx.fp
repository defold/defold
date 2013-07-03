varying vec4 position;
varying vec2 var_texcoord0;
varying vec4 var_color;

uniform sampler2D DIFFUSE_TEXTURE;
uniform vec4 tint;

void main()
{
    // Pre-multiply alpha since all runtime textures already are
    tint.xyz *= tint.w;
    // var_color is vertex color from the particle system, already pre-multiplied
    gl_FragColor = texture2D(DIFFUSE_TEXTURE, var_texcoord0.xy) * var_color * tint;
}
