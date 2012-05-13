varying vec4 position;
varying vec2 var_texcoord0;

uniform vec4 face_color;
uniform vec4 outline_color;
uniform vec4 shadow_color;
uniform sampler2D texture;

void main()
{
    vec4 t = texture2D(texture, var_texcoord0.xy);

    vec4 fc = face_color;
    vec4 oc = outline_color;
    vec4 sc = shadow_color;

    fc *= t.x;
    oc *= t.y;
    sc *= t.z;

    gl_FragColor = fc + oc + sc;
}
