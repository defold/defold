@fragment fn main(@location(0) color: vec3f) -> @location(0) vec4f {
    return vec4f(color, 1.0);
}
