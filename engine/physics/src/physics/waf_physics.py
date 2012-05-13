import Task, TaskGen
from TaskGen import extension

def configure(conf):
    conf.env['STATICLIB_PHYSICS'] = ['physics', 'BulletDynamics', 'BulletCollision', 'LinearMath', 'Box2D']
