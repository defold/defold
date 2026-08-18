layout(local_size_x=64,local_size_y=1,local_size_z=1) in;
uniform highp mat4 view_matrix;
uniform highp vec4 cluster_grid;
uniform highp vec4 cluster_limits;
struct Light{vec4 position;vec4 color;vec4 direction_range;vec4 params;};
layout(std430) readonly buffer LightBuffer{vec4 light_info;Light lights[];};
struct ClusterBounds{vec4 minimum;vec4 maximum;};
layout(std430) readonly buffer ClusterBoundsBuffer{ClusterBounds cluster_bounds[];};
layout(std430) buffer ClusterMetadataBuffer{uvec2 cluster_metadata[];};
layout(std430) buffer ClusterLightIndicesBuffer{uint cluster_light_indices[];};
layout(std430) buffer ClusterCountersBuffer{uvec4 cluster_counters;};
layout(std430) buffer ClusterOverflowBuffer{uint cluster_overflow[];};
shared uint accepted_count; shared uint accepted_indices[256];
bool sphere_aabb(vec3 p,float r,vec3 mn,vec3 mx){vec3 q=p-clamp(p,mn,mx);return dot(q,q)<=r*r;}
bool intersects(Light l,ClusterBounds b){
 uint t=uint(l.params.x+0.5);if(t==0u)return true;vec3 p=(view_matrix*vec4(l.position.xyz,1.0)).xyz;float r=l.direction_range.w;
 if(!sphere_aabb(p,r,b.minimum.xyz,b.maximum.xyz))return false;
 if(t==2u){vec3 dir=normalize(mat3(view_matrix)*l.direction_range.xyz),c=(b.minimum.xyz+b.maximum.xyz)*0.5,v=c-p;float cr=length(b.maximum.xyz-c),dist=length(v);if(dist>cr&&dot(dir,v/dist)<cos(l.params.w+asin(clamp(cr/dist,0.0,1.0))))return false;}return true;
}
void main(){
 uint ci=gl_WorkGroupID.x,cc=uint(cluster_grid.x*cluster_grid.y*cluster_grid.z);if(ci>=cc)return;if(gl_LocalInvocationIndex==0u)accepted_count=0u;barrier();
 uint lc=uint(light_info.w),limit=min(uint(cluster_limits.x),256u);for(uint li=gl_LocalInvocationIndex;li<lc;li+=gl_WorkGroupSize.x)if(intersects(lights[li],cluster_bounds[ci])){uint s=atomicAdd(accepted_count,1u);if(s<limit)accepted_indices[s]=li;}barrier();
 if(gl_LocalInvocationIndex==0u){uint n=min(accepted_count,limit),drop=accepted_count-n,off=atomicAdd(cluster_counters.x,n),cap=uint(cluster_limits.y),write=off<cap?min(n,cap-off):0u;drop+=n-write;cluster_metadata[ci]=uvec2(off,write);cluster_overflow[ci]=drop;atomicAdd(cluster_counters.y,drop);if(drop!=0u)atomicAdd(cluster_counters.z,1u);atomicMax(cluster_counters.w,accepted_count);accepted_count=write;}barrier();
 uint off=cluster_metadata[ci].x;for(uint i=gl_LocalInvocationIndex;i<accepted_count;i+=gl_WorkGroupSize.x)cluster_light_indices[off+i]=accepted_indices[i];
}
