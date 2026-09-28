struct Output { @builtin(position) position: vec4f, @location(0) color: vec3f }
@vertex fn main(@location(0) position: vec3f, @location(1) color: vec3f) -> Output {
    return Output(vec4f(position, 1.0), color);
}
