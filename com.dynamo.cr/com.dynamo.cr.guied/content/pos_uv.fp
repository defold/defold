varying vec4 position;
varying vec2 var_texcoord0;

uniform sampler2D texture;
uniform vec4 color;

void main()
{
    gl_FragColor = texture2D(texture, var_texcoord0.xy) * color;
}
