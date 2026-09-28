@group(0) @binding(0) var test_texture: texture_2d<f32>;
@group(0) @binding(1) var test_sampler: sampler;
@fragment fn main(@location(0) direction: vec3f) -> @location(0) vec4f {
    return vec4f(textureSampleLevel(test_texture, test_sampler, direction.xy, 0.0).rgb, 1.0);
}
