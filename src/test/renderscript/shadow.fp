#include "shadow.h"

uniform sampler2D TEXTURE       : TEXUNIT0;
uniform sampler2D SHADOW_MAP    : TEXUNIT1;

uniform float4 DIFFUSE          : C0;

uniform float4 AMBIENT          : C1;

uniform float4 LIGHT_COLOR      : C2;
uniform float3 LIGHT_POSITION   : C3;
uniform float3 LIGHT_DIRECTION  : C4;
uniform float4 LIGHT_PARAMS     : C5;

void main(in Fragment f_in,
          out float4 c_out : COLOR)
{
    float radius = LIGHT_PARAMS.x;
    float cos_cone_angle = LIGHT_PARAMS.y;
    float drop_off = LIGHT_PARAMS.z;
    float map_texel_offset = LIGHT_PARAMS.w;

    float3 N = f_in.normal;
    float3 diff = LIGHT_POSITION - f_in.world_pos;
    float distance = length(diff);
    float3 L = normalize(diff);
    float4 diffuse = tex2D(TEXTURE, f_in.uv.xy);
    diffuse = DIFFUSE;
    float inside = distance < radius;
    float intensity = 1 - drop_off * (1 - (radius - distance) / radius);
    float cone = dot(-L, LIGHT_DIRECTION) > cos_cone_angle;
    diffuse *= saturate(dot(L, N) * (distance < radius) * intensity * cone);

    float4 shadow_uv = f_in.shadow_uv / f_in.shadow_uv.w;
    float center_depth = tex2D(SHADOW_MAP, shadow_uv.xy).x;
    float4 depths = float4(
        tex2D(SHADOW_MAP, shadow_uv.xy + float2(-map_texel_offset, 0)).x,
        tex2D(SHADOW_MAP, shadow_uv.xy + float2(map_texel_offset, 0)).x,
        tex2D(SHADOW_MAP, shadow_uv.xy + float2(0, -map_texel_offset)).x,
        tex2D(SHADOW_MAP, shadow_uv.xy + float2(0, map_texel_offset)).x);

    float shadow = (center_depth > shadow_uv.z) ? 1 : 0;
    shadow += (depths.x > shadow_uv.z) ? 1 : 0;
    shadow += (depths.y > shadow_uv.z) ? 1 : 0;
    shadow += (depths.z > shadow_uv.z) ? 1 : 0;
    shadow += (depths.w > shadow_uv.z) ? 1 : 0;
    shadow *= 0.2;

    c_out = saturate(diffuse * shadow + AMBIENT);
}
