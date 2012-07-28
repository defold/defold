import sys, re, logging
from youtube import YouTube

"""
Replaces links in asciidoc generated html files with one of the following asciidoc forms:
link:/youtube_embed/filename.ext[My Description]
link:/youtube_embed/youtube_id[My Description]

Example:
link:/youtube_embed/test_rec.ivf[Test Rec]
link:/youtube_embed/T_igYdHubqA[Test Rec]
"""

yt = YouTube()

def repl(m):
    ret = '''<div align="center"><iframe width="560" height="315" src="http://www.youtube.com/embed/%s?hd=1" frameborder="0" allowfullscreen></iframe></div>'''
    value = m.groups()[0]
    # Check if the supplied value is a youtube id
    if value.find('.') == -1:
        return ret % value
    file_name = m.groups()[0]
    title = YouTube.filename_to_title(file_name)
    e = yt.find_latest_video(title)
    if e:
        id = yt.get_video_id(e)
        return ret % id
    else:
        logging.error('No video with title "%s" found on YouTube' % title)
        sys.exit(5)
        return None

f = open(sys.argv[1], 'rb')
replaced = re.sub('<a href="/youtube_embed/(.+?)">.*</a>', repl, f.read())
f.close()

f = open(sys.argv[2], 'wb')
f.write(replaced)
f.close()
