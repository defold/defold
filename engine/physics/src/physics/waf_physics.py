import Task, TaskGen
import Options
from TaskGen import extension
import Logs

def configure(conf):
    if 'physics' in Options.options.disable_features:
        Logs.info("physics disabled")
        conf.env['STATICLIB_PHYSICS'] = ['physics_null']
    else:
        conf.env['STATICLIB_PHYSICS'] = ['physics', 'BulletDynamics', 'BulletCollision', 'LinearMath', 'Box2D']
