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

render('404.html', 'error/404.html', root='/')
render('500.html', 'error/500.html', root='/')

render('landing.html', 'landing.html')
render('plans.html', 'plans/plans.html', active_page = 'plans')

docs = ['script_properties.html',
        'eula.html',
        'game_project.html',
        'getting_started.html',
        'hud.html',
        'introduction.html',
        'level_complete.html',
        'main_menu.html',
        'message_passing.html',
        'parallax.html',
        'physics.html',
        'reload.html',
        'script_properties.html',
        'scripting_debugging.html',
        'side_scroller.html',
        'story.html',
        'tiles.html']

for d in docs:
    asciidoc('doc/%s' % d, active_page = 'documentation', disqus = True)

render('doc_index.html', 'doc/index.html', active_page = 'documentation')

about = ['eula.html',
         'story.html']
for d in about:
    asciidoc('doc/%s' % d, 'about/%s' % d)

render('search.jsp', 'search/search.jsp', root='/')
