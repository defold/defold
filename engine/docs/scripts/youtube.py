import re, os, sys
import binascii
import string
import gdata.youtube
import gdata.youtube.service
import gdata.service
from gdata.media import YOUTUBE_NAMESPACE
from iso8601 import parse
import time
import threading

CATEGORIES_SCHEME = "http://gdata.youtube.com/schemas/2007/categories.cat"

class YouTube(object):
    DEVELOPER_KEY = 'AI39si497iB2cNY-lw-KNCyjL_GMpw_3x9HO7Rro9_KksfUvyvnlm9fR-gR3ASCcbGF7zHqFPJQAGiJLni0R3AL8sXdHDMaFvw'
    PASSWORD = 'tudOoradNed3'
    EMAIL='youtube@defold.se'
    USERID='defoldvideos'
    CHANNELID='UCrEYXG15n3V0Y8eK0RbCaLQ'
    EXTENSIONS = '.ivf'.split()

    def __init__(self):
        self.title_to_video = {}
        self.sign_in()
        self.fetch_uploaded_videos()

    def sign_in(self):
        self.service = gdata.youtube.service.YouTubeService()
        self.service.ssl = False
        self.service.developer_key = YouTube.DEVELOPER_KEY
        self.service.email = YouTube.EMAIL
        self.service.password = YouTube.PASSWORD
        try:
            self.service.ProgrammaticLogin()
        except gdata.service.CaptchaRequired:
            print "run single thread and uncomment the code below, we need captcha"
#            print self.service._GetCaptchaURL()
#            response = raw_input('Captcha:')
#            print response
#            self.service.ProgrammaticLogin(self.service._GetCaptchaToken(), response)

    @staticmethod
    def print_entry(entry):
        date = parse(entry.published.text)
        print '%s (%s) (%s)' % (entry.media.title.text, date, entry.media.keywords.text)
        if entry.control:
            for i, e in enumerate(entry.control.extension_elements):
                if e.tag == 'state':
                    print 'state: %s' % (e.attributes['name'])

    @staticmethod
    def get_video_id(entry):
        m = re.search('v=(.*)', entry.link[0].href)
        return m.groups()[0]

    def print_uploaded_videos(self):
        print "Uploaded Videos:"
        for entry in self.all_entries:
            self.print_entry(entry)
            print ""
        if len(self.all_entries) == 0:
            print 'No videos uploaded'

    def fetch_uploaded_videos(self):
        # As of April 2015, Google dropped support for the old way we used
        # to access the video uploads list (YouTube API V2 in favor of V3).
        # See: https://youtube.com/devicesupport
        uri = 'http://www.youtube.com/feeds/videos.xml?channel_id=%s' % YouTube.CHANNELID
        feed = self.service.GetYouTubePlaylistVideoFeed(uri)
        for entry in feed.entry:
            title = entry.media.title.text
            lst = self.title_to_video.get(title, [])
            lst.append(entry)
            self.title_to_video[title] = lst

        self.all_entries = feed.entry

    @staticmethod
    def filename_to_title(file_name):
        title =  os.path.splitext(os.path.basename(file_name))[0]
        title_lst = title.split('_')
        title_lst = map(lambda x: x[0].upper() + x[1:], title_lst)
        title_nice = ' '.join(title_lst)
        return title_nice

    def sync(self, top):
        print "Synchronizing:"
        videos = self._find_upload_candidates(top)
        for file_name in videos:
            title = self.filename_to_title(file_name)
            f = open(file_name, 'rb')
            crc = binascii.crc32(f.read()) & 0xffffffff
            crc = '%x' % crc
            f.close()

            entry = self.find_video(title, crc)
            if entry:
                print 'Skipping "%s". Already uploaded (version=%s)' % (file_name, crc)
            else:
                print 'Uploading "%s" (version=%s)' % (file_name, crc)
                self._upload(file_name, crc)

    def _create_video_entry(self, title, description, category, keywords=None,
                            location=None, private=False, unlisted=False):
        media_group = gdata.media.Group(
            title=gdata.media.Title(text=title),
            description=gdata.media.Description(description_type='plain', text=description),
            keywords=gdata.media.Keywords(text=keywords),
            category=gdata.media.Category(
                text=category,
                label=category,
                scheme=CATEGORIES_SCHEME),
            private=(gdata.media.Private() if private else None),
            player=None)
        if location:
            where = gdata.geo.Where()
            where.set_location(location)
        else:
            where = None
        kwargs = {
          "namespace": YOUTUBE_NAMESPACE,
          "attributes": {'action': 'list', 'permission': 'denied'},
        }
        extension = ([ExtensionElement('accessControl', **kwargs)] if unlisted else None)
        return gdata.youtube.YouTubeVideoEntry(media=media_group, geo=where, extension_elements=extension)

    def _upload(self, file_name, crc):
        title = self.filename_to_title(file_name)
        entry = self._create_video_entry(title, title, "Games", keywords = 'version=%s,games,game engine,defold,tutorial,example' % crc)
        entry2 = self.service.InsertVideoEntry(entry, file_name)

    def find_latest_video(self, title):
        lst = self.title_to_video.get(title, [])
        def c(x, y):
            date_x = parse(x.published.text)
            date_y = parse(y.published.text)
            return cmp(date_x, date_y)
        lst.sort(cmp = c)

        if len(lst) > 0:
            return lst[-1]
        else:
            return None

    def find_video(self, title, crc):
        for entry in self.title_to_video.get(title, []):
            tags = entry.media.keywords.text.split(',')
            tags = map(string.strip, tags)
            for t in tags:
                m = re.match('version=(.*)', t)
                if m:
                    if m.groups()[0] == crc:
                        return entry

        return None

    def _find_upload_candidates(self, top):
        ret = []
        for root, dirs, files in os.walk(top):
            for f in files:
                tmp, ext = os.path.splitext(f)
                if ext in YouTube.EXTENSIONS:
                    ret.append(os.path.join(root, f))
        return ret
