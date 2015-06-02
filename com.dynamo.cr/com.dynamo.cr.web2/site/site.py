# Static site description. Run this file with desite.py
import os

for d, title in [('collection_proxy', 'Collection Proxy Component'),
                 ('factory', 'Factory Component'),
                 ('particlefx', 'ParticleFX Component'),
                 ('collision_object', 'Collision Object Component'),
                 ('sound', 'Sound Component'),
                 ('sprite', 'Sprite Component'),
                 ('tilemap', 'Tilemap Component'),

                 ('builtins', 'Script Builtin'),
                 ('msg', 'Script Message'),
                 ('vmath', 'Script Vector Math'),

                 ('go', 'Game Object'),
                 ('gui',  'Gui'),
                 ('render',  'Render'),
                 (['sys', 'engine'], 'System')]:
    if isinstance(d, str):
       d = [d]
    docs = [ os.path.join(dynamo_home, 'share', 'doc', '%s_doc.sdoc' % x) for x in d]
    ref(docs, title, 'ref/%s.html' % d[0],
        active_page = 'documentation')

# blog('doc/blog.html', 'blog')

render('unsubscribe.html')

render('404.html', 'error/404.html', skip_index = True, root='/')
render('500.html', 'error/500.html', skip_index = True, root='/')

render('landing.html', 'index.html')
render('plans.html', 'plans/index.html', active_page = 'plans')
render('signup.html', 'signup/index.html')

docs = ['2dgraphics.html',
        'application_lifecycle.html',
        'collection_proxies.html',
        'example.html',
        'game_project.html',
        'gui.html',
        'hud.html',
        'input.html',
        'introduction.html',
        'ios.html',
        'level_complete.html',
        'main_menu.html',
        'message_passing.html',
        'modules.html',
        'parallax.html',
        'particlefx.html',
        'physics.html',
        'platformer.html',
        'platformer_script.html',
        'properties.html',
        'reload.html',
        'scene_editing.html',
        'script_properties.html',
        'scripting.html',
        'scripting_debugging.html',
        'side_scroller.html',
        'tiles.html']

for d in docs:
    asciidoc('doc/%s' % d, active_page = 'documentation', disqus = True)

render('doc_index.html', 'doc/sindex.html', active_page = 'documentation')

about = ['eula.html',
         'privacy.html',
         'story.html',
         'company.html']
for d in about:
    asciidoc('doc/%s' % d, 'about/%s' % d)

render('search.jsp', 'search/search.jsp', root='/')
