import sys
from youtube import YouTube

print ""
yt = YouTube()
yt.sync(sys.argv[1])
