#! /usr/bin/env python

# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import waflib.Logs, waflib.Options
from waf_dynamo import platform_supports_feature

def configure(conf):
    if 'physics' in waflib.Options.options.disable_features:
        waflib.Logs.info("physics disabled")
        conf.env['STLIB_PHYSICS'] = ['physics_null']
    else:
        box2d_lib = 'box2d_defold'
        physics_lib = 'physics'
        if 'box2dv3' in waflib.Options.options.enable_features:
            physics_lib = 'physics_2d'
            if 'simd' in waflib.Options.options.enable_features:
                waflib.Logs.info("box2dv3 selected, simd enabled")
                box2d_lib = 'box2d'
            else:
                waflib.Logs.info("box2dv3 selected, simd disabled")
                box2d_lib = 'box2d_nosimd'
        else:
            waflib.Logs.info("box2dv2 selected")

        conf.env['STLIB_PHYSICS'] = [physics_lib, 'BulletDynamics', 'BulletCollision', 'LinearMath', box2d_lib]
