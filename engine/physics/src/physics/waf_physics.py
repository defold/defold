#! /usr/bin/env python

# Copyright 2020-2025 The Defold Foundation
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
        Logs.info("physics disabled")
        conf.env['STLIB_PHYSICS'] = ['physics_null']
    else:
        conf.env['STLIB_PHYSICS'] = ['physics', 'BulletDynamics', 'BulletCollision', 'LinearMath', 'Box2D']
