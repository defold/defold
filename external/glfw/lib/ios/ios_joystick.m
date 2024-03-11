// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "internal.h"

int _glfwPlatformGetJoystickParam( int joy, int param )
{
    return 0;
}
int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes )
{
    return 0;
}
int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons )
{
    return 0;
}
int _glfwPlatformGetJoystickHats( int joy, unsigned char *hats, int numhats )
{
    return 0;
}
int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
    (void) joy;
    (void) device_id;
    return GL_FALSE;
}
