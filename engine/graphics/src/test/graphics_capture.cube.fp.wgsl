@group(0) @binding(0) var cubemap: texture_cube<f32>;
@group(0) @binding(1) var cube_sampler: sampler;
@fragment fn main(@location(0) direction: vec3f) -> @location(0) vec4f {
    return textureSampleLevel(cubemap, cube_sampler, direction, 0.0);
}
