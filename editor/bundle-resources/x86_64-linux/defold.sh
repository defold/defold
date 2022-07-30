#!/bin/bash
# for testing purposes
# function glxinfo() {
# cat << EOF
# server glx version string: 1.4
# client glx version string: 1.4
# GLX version: 1.4
# Max core profile version: 4.6
# Max compat profile version: 4.6
# Max GLES1 profile version: 1.1
# Max GLES[23] profile version: 3.2
# OpenGL core profile version string: 4.6 (Core Profile) Mesa 20.2.6
# OpenGL core profile shading language version string: 4.60
# OpenGL version string: 4.6 (Compatibility Profile) Mesa 20.2.6
# OpenGL shading language version string: 4.60
# OpenGL ES profile version string: OpenGL ES 3.2 Mesa 20.2.6
# OpenGL ES profile shading language version string: OpenGL ES GLSL ES 3.20
# GL_EXT_shader_implicit_conversions, GL_EXT_shader_integer_mix,
# EOF
# }


if ! command -v glxinfo &> /dev/null
then
    echo "glxinfo could not be found"
    echo "Install using 'sudo apt-get install mesa-utils'"
    exit
fi

# detect MESA and GL version and set override
export GL_VERSION=$(glxinfo | grep "OpenGL version string" | sed 's/OpenGL version string: \([0-9]*.[0-9]*\).*/\1/')

# export MESA_VERSION=$(glxinfo | grep "OpenGL version string" | sed 's/OpenGL version string: .* Mesa \(.*\)/\1/')
# export MESA_LOADER_DRIVER_OVERRIDE=i965

if (( $(echo "$GL_VERSION >= 3.0" |bc -l) )); then
    export MESA_GL_VERSION_OVERRIDE=${GL_VERSION}
    echo "MESA_GL_VERSION_OVERRIDE is ${MESA_GL_VERSION_OVERRIDE}"
elif
    export LIBGL_ALWAYS_SOFTWARE=1
    echo "Using software rendering since GL version is lower than 3.0"
fi



# detect libffi and add to LD_PRELOAD
LIBFFI6=/usr/lib/x86_64-linux-gnu/libffi.so.6
LIBFFI7=/usr/lib/x86_64-linux-gnu/libffi.so.7

if [ -f "${LIBFFI6}" ]; then
    echo "LIBFFI6 detected"
    export LD_PRELOAD=$LD_PRELOAD:${LIBFFI6}
elif [ -f "${LIBFFI7}" ]; then
    echo "LIBFFI7 detected"
    export LD_PRELOAD=$LD_PRELOAD:${LIBFFI7}
else
    echo "Defold requires libffi.so.6 or libffi.so.7"
    echo "Install using 'wget http://ftp.br.debian.org/debian/pool/main/libf/libffi/libffi7_3.3-5_amd64.deb && sudo dpkg -i libffi7_3.3-5_amd64.deb'"
    exit
fi

# launch!
./Defold