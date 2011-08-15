varying vec4 position;
varying vec2 var_texcoord0;

uniform vec4 diffuse_color;
uniform sampler2D texture;

void main()
{
    vec4 tex = texture2D(texture, var_texcoord0.xy);
    gl_FragColor = tex * diffuse_color;
}
