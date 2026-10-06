struct Output { @builtin(position) position: vec4f, @location(0) color: vec3f }
@vertex fn main(@location(0) position: vec3f, @location(1) color: vec3f) -> Output {
    return Output(vec4f(position, 1.0), color);
}

// Match ShaderCompilePipeline's offscreen variant and its adapter-selection marker.
// defold-webgpu-flipped-entry-point: _defold_webgpu_main_flipped
@vertex fn _defold_webgpu_main_flipped(@location(0) position: vec3f, @location(1) color: vec3f) -> Output {
    return Output(vec4f(position.x, -position.y, position.z, 1.0), color);
}
