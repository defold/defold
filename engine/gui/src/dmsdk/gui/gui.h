// Copyright 2020 The Defold Foundation
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

#ifndef DMSDK_GUI_H
#define DMSDK_GUI_H

#include <stdint.h>
#include <dlib/message.h>
#include <ddf/ddf.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/easing.h>
#include <dlib/image.h>
#include <hid/hid.h>
#include <particle/particle.h>
#include <rig/rig.h>

#include <script/script.h>

#include <dmsdk/dlib/vmath.h>

/**
 * Defold GUI system
 *
 * About properties:
 * Previously properties where referred to using the enum Property.
 * When support for animating individual tracks was added a more generic access scheme
 * was introduced (hash of a string).
 * With the hash version of the property scalar components
 * can be referred to with "PROPERTY.COMPONENT", e.g. "position.x".
 * Note that when vector values are animated multiple tracks are created internally - one
 * for each component.
 */
namespace dmGui
{
    /*#
     * A handle to a gui node
     * @name HNode
     * @type typedef
     */
    typedef uint32_t HNode;

    /*#
     * Invalid node handle
     * @name INVALID_HANDLE
     * @type HNode
     */
    const HNode INVALID_HANDLE = 0;
}

#endif
