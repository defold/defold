varying vec2 var_texcoord0;

uniform vec4 uni_face_color;
uniform vec4 uni_outline_color;
uniform sampler2D texture;

void main()
{
    // Face + outline mask
    vec2 t = texture2D(texture, var_texcoord0.xy).rg;
  
    gl_FragColor = vec4(uni_face_color.xyz, 1.0) * t.x * uni_face_color.w + vec4(uni_outline_color.xyz * t.y * uni_outline_color.w, t.y * uni_outline_color.w) * (1.0 - t.x * uni_face_color.w);
    
}
