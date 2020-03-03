#! /usr/bin/env python

import Logs, Options
from waf_dynamo import platform_supports_feature

def configure(conf):
    if 'physics' in Options.options.disable_features:
        Logs.info("physics disabled")
        conf.env['STATICLIB_PHYSICS'] = ['physics_null']
    else:

        box2d = platform_supports_feature(platform, 'box2d', {})
        bullet3d = platform_supports_feature(platform, 'bullet3d', {})

        if bullet3d and box2d:
            conf.env['STATICLIB_PHYSICS'] = ['physics', 'BulletDynamics', 'BulletCollision', 'LinearMath', 'Box2D']
        elif box2d:
            conf.env['STATICLIB_PHYSICS'] = ['physics_2d', 'Box2D']
        else:
            conf.env['STATICLIB_PHYSICS'] = ['physics_3d', 'BulletDynamics', 'BulletCollision', 'LinearMath']
