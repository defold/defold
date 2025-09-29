
uniform vec4 user_var1; // to test render constants
uniform vec4 user_var2; // to test render constants

varying vec2 var_uv;
varying vec3 var_normal;

void main()
{
    gl_FragColor = user_var1 + user_var2 + vec4(var_uv, 0.0, 0.0) + vec4(var_normal, 0.0);
}
