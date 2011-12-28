import sys
from youtube import YouTube

print ""
yt = YouTube()
yt.print_uploaded_videos()
yt.sync(sys.argv[1])
