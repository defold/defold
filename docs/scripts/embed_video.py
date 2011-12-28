import sys, re, logging
from youtube import YouTube

"""
Replaces links in asciidoc generated html files with the following asciidoc form:
link:/youtube_embed/filename.ext[My Description]

Example:
link:/youtube_embed/test_rec.ivf[Test Rec]
"""

yt = YouTube()

def repl(m):
    file_name = m.groups()[0]
    title = YouTube.filename_to_title(file_name)
    e = yt.find_latest_video(title)
    if e:
        ret = '''<iframe width="560" height="315" src="http://www.youtube.com/embed/%s?hd=1" frameborder="0" allowfullscreen></iframe>'''
        id = yt.get_video_id(e)
        return ret % id
    else:
        logging.error('No video with title "%s" found on YouTube' % title)
        sys.exit(5)
        return None

for file_name in sys.argv[1:]:
    f = open(file_name, 'rb')
    replaced = re.sub('<a href="/youtube_embed/(.+?)">.*</a>', repl, f.read())
    f.close()

    f = open(file_name, 'wb')
    f.write(replaced)
    f.close()
