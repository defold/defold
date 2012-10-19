varying vec4 position;
varying vec2 var_texcoord0;
varying vec4 var_color;

uniform sampler2D DIFFUSE_TEXTURE;
uniform vec4 tint;

void main()
{
    gl_FragColor = texture2D(DIFFUSE_TEXTURE, var_texcoord0.xy) * var_color * tint;
}
