varying vec4 position;
varying vec2 var_texcoord0;

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform vec4 tint;

void main()
{
    gl_FragColor = texture2D(tex0, var_texcoord0.xy) * texture2D(tex1, var_texcoord0.xy) * texture2D(tex2, var_texcoord0.xy);
}
