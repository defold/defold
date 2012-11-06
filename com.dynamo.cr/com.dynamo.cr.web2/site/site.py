# Static site description. Run this file with desite.py
import os

for d, title in [('engine', 'Game Engine Reference Documentation'),
                 ('gamesys', 'Gamesystem Reference Documentation'),
                 ('go', 'Game Object Reference Documentation'),
                 ('gui',  'Gui Reference Documentation'),
                 ('render',  'Render Reference Documentation'),
                 ('script', 'Script Reference Documentation')]:
    ref(os.path.join(dynamo_home, 'share', 'doc', '%s_doc.sdoc' % d), title, 'ref/%s.html' % d,
        active_page = 'documentation')

blog('doc/blog.html', 'blog')

render('unsubscribe.html')

render('404.html', 'error/404.html', skip_index = True, root='/')
render('500.html', 'error/500.html', skip_index = True, root='/')

render('landing.html', 'index.html')
render('plans.html', 'plans/index.html', active_page = 'plans')
render('signup.html', 'signup/index.html')

docs = ['game_project.html',
        'hud.html',
        'input.html',
        'introduction.html',
        'ios.html',
        'level_complete.html',
        'main_menu.html',
        'message_passing.html',
        'parallax.html',
        'physics.html',
        'platformer.html',
        'platformer_script.html',
        'reload.html',
        'scene_editing.html',
        'script_properties.html',
        'scripting_debugging.html',
        'side_scroller.html',
        'tiles.html',
        'particlefx.html']

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
