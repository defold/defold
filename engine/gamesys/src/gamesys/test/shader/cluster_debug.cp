layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(rgba32f) uniform image2D texture_out;
uniform highp vec4 debug_view;
uniform highp vec4 cluster_grid;
layout(std430) readonly buffer ClusterMetadataBuffer{uvec2 cluster_metadata[];};
layout(std430) readonly buffer ClusterOverflowBuffer{uint cluster_overflow[];};
vec3 heat(float t){return clamp(vec3(1.5-abs(4.0*t-3.0),1.5-abs(4.0*t-2.0),1.5-abs(4.0*t-1.0)),0.0,1.0);}
void main(){uvec2 p=gl_GlobalInvocationID.xy;if(any(greaterThanEqual(p,uvec2(debug_view.xy))))return;uvec3 d=uvec3(cluster_grid.xyz);uvec2 tile=min(uvec2(vec2(p)/debug_view.xy*vec2(d.xy)),d.xy-1u);uint z=min(uint(debug_view.z),d.z-1u),i=tile.x+d.x*(tile.y+d.y*z);vec3 c=heat(clamp(float(cluster_metadata[i].y)/max(debug_view.w,1.0),0.0,1.0));if(cluster_overflow[i]!=0u)c=mix(c,vec3(1.0,0.0,1.0),0.75);imageStore(texture_out,ivec2(p),vec4(c,1.0));}
