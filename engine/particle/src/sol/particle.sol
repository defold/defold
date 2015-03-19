module particle

require io
require math

x_axis: Vector3 = Vector3 {x=1.0f, y=0.0f, z=0.0f }

DRAG_LOCAL_DIR: Vector3 = x_axis

MODIFIER_KEY_MAGNITUDE: int = 0
PROPERTY_SAMPLE_COUNT: int = 64


!export("particle_apply_drag")
function apply_drag(particles: [@Particle], particle_count: int, modifier_properties: [2: @Property], use_direction: bool, rotation: Quat, emitter_t: float, dt: float)
   local direction = rotate(rotation, DRAG_LOCAL_DIR)
   local magnitude_property = modifier_properties[MODIFIER_KEY_MAGNITUDE]
   local segment_index = math.min(int(emitter_t * float(PROPERTY_SAMPLE_COUNT)), PROPERTY_SAMPLE_COUNT - 1)
   local magnitude = SAMPLE_PROP(magnitude_property.segments[segment_index], emitter_t)
   local mag_spread = magnitude_property.spread

   local i = 0
   while i < particle_count do
	  local particle = particles[i]
	  local v = particle.velocity
	  if use_direction then
		 v = projection(Point3(particle.velocity), direction) * direction
	  end
	  -- Applied drag > 1 means the particle would travel in the reverse direction
	  local applied_drag = math.min((magnitude + mag_spread * particle.spread_factor) * dt, 1.0f)
	  --io.println("applied_drag: " .. applied_drag .. ", spread: " .. particle.spread_factor)
	  particle.velocity = particle.velocity - v * applied_drag
	  i = i + 1
   end
end


function * (scalar: float, vec: Vector3): Vector3
    return vec * scalar
end

function * (vec: Vector3, scalar: float): Vector3
    return Vector3 {
	   x = vec.x * scalar,
	   y = vec.y * scalar,
	   z = vec.z * scalar
    }
end

function - (a: Vector3, b: Vector3): Vector3
   return Vector3 { x = a.x - b.x, y = a.y - b.y, z = a.z - b.z }
end


function projection(pnt: Point3, unit_vec: Vector3): float
   local result: float = 0.0f
   result = pnt.x * unit_vec.x;
   result = result + ( pnt.y * unit_vec.y )
   result = result + ( pnt.z * unit_vec.z )
   return result
end


function Point3(v: Vector3): Point3
   return Point3{ x=v.x, y=v.y, z=v.z }
end

function SAMPLE_PROP(segment: LinearSegment, x: float): float
   return (x - segment.x) * segment.k + segment.y
end

function rotate(quat: Quat, vec: Vector3): Vector3
   local tmpX = ( ( quat.w * vec.x ) + ( quat.y * vec.z ) ) - ( quat.z * vec.y )
   local tmpY = ( ( quat.w * vec.y ) + ( quat.z * vec.x ) ) - ( quat.x * vec.z )
   local tmpZ = ( ( quat.w * vec.z ) + ( quat.x * vec.y ) ) - ( quat.y * vec.x )
   local tmpW = ( ( quat.x * vec.x ) + ( quat.y * vec.y ) )+ ( quat.z * vec.z )
   return Vector3 {
	  x = ( ( ( tmpW * quat.x ) + ( tmpX * quat.w ) ) - ( tmpY * quat.z ) ) + ( tmpZ * quat.y ),
	  y = ( ( ( tmpW * quat.y ) + ( tmpY * quat.w ) ) - ( tmpZ * quat.x ) ) + ( tmpX * quat.z ),
	  z = ( ( ( tmpW * quat.z ) + ( tmpZ * quat.w ) ) - ( tmpX * quat.y ) ) + ( tmpY * quat.x )
   }
end


struct Particle
    -- Position, which is defined in emitter space or world space depending on how the emitter which spawned the particles is tweaked.
    position: @Point3
	-- Rotation, which is defined in emitter space or world space depending on how the emitter which spawned the particles is tweaked.
	source_rotation: @Quat
	rotation: @Quat
	-- Velocity of the particle
	velocity: @Vector3
	-- Time left before the particle dies.
	time_left: float
	-- The duration of this particle.
	max_life_time: float
	-- Inverted duration.
	oo_max_life_time: float
	-- Factor used for spread
	spread_factor: float
	-- Particle size
	source_size: float
	size: float
	-- Particle color
	source_color: @Vector4
	color: @Vector4
	-- Sorting
	sort_key: @SortKey
end

!align(16)
struct Point3
    x: float
	y: float
	z: float
	dummy: float
end

!align(16)
struct Quat
    x: float
	y: float
	z: float
	w: float
end

!align(16)
struct Vector3
    x: float
	y: float
	z: float
	dummy: float
end

!align(16)
struct Vector4
    x: float
	y: float
	z: float
	w: float
end

struct SortKey
    key: uint32
end

struct Property
    segments: @[64: @LinearSegment]
	spread: float
end

struct LinearSegment
    x: float
	y: float
	k: float
end
