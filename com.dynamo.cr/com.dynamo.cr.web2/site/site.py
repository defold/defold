# Static site description. Run this file with desite.py

blog('doc/blog.html', 'blog')

render('404.html', 'error/404.html', root='/')
render('500.html', 'error/500.html', root='/')

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
    asciidoc('doc/%s' % d, active_page = 'documentation')

about = ['eula.html',
         'story.html']

for d in about:
    asciidoc('doc/%s' % d, 'about/%s' % d)
