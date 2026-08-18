layout(local_size_x=4,local_size_y=4,local_size_z=4) in;
uniform highp mat4 inverse_projection;
uniform highp vec4 cluster_grid;
uniform highp vec4 cluster_z_params;
struct ClusterBounds { vec4 minimum; vec4 maximum; };
layout(std430) buffer ClusterBoundsBuffer { ClusterBounds cluster_bounds[]; };
vec3 ray(vec2 n){vec4 p=inverse_projection*vec4(n,1.0,1.0);return p.xyz/p.w;}
vec3 at_depth(vec3 r,float d){return r*(-d/r.z);}
void main(){
 uvec3 g=gl_GlobalInvocationID.xyz,d=uvec3(cluster_grid.xyz); if(any(greaterThanEqual(g,d)))return;
 vec2 n0=vec2(g.xy)/vec2(d.xy)*2.0-1.0,n1=vec2(g.xy+uvec2(1))/vec2(d.xy)*2.0-1.0;
 float s0=float(g.z)/float(d.z),s1=float(g.z+1u)/float(d.z),zn=cluster_z_params.x,zf=cluster_z_params.y;
 float z0=zn*pow(zf/zn,s0),z1=zn*pow(zf/zn,s1); vec3 mn=vec3(3.402823e38),mx=-mn;
 for(uint c=0u;c<4u;++c){vec2 n=vec2((c&1u)!=0u?n1.x:n0.x,(c&2u)!=0u?n1.y:n0.y);vec3 r=ray(n),a=at_depth(r,z0),b=at_depth(r,z1);mn=min(mn,min(a,b));mx=max(mx,max(a,b));}
 uint i=g.x+d.x*(g.y+d.y*g.z);cluster_bounds[i].minimum=vec4(mn,z0);cluster_bounds[i].maximum=vec4(mx,z1);
}
