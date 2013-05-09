#!/usr/bin/env python

import optparse
from cStringIO import StringIO
import os, re, sys
from jinja2 import FileSystemLoader, ChoiceLoader, Environment
import script_doc_ddf_pb2
from google.protobuf import text_format

class AsciiDocLoader(FileSystemLoader):
    """Custom FileSystemLoader for asciidoc-files that extracts the payload from the generated file"""
    def get_source(self, environment, template):
        res = list(FileSystemLoader.get_source(self, environment, template))
        # Extract the "payload" of the generated asciidoc-file
        reg_exp = "[\\s\\S]*?<body[\\s\\S]*?>([\\s\\S]*?)</body>[\\s\\S]*?"
        match = re.match(reg_exp, res[0], re.DOTALL)
        res[0] = match.groups()[0]
        return res

class BlogPost(object):
    def __init__(self, id, title, summary, content, date):
        self.id = id
        self.title = unicode(title, 'utf-8')
        self.summary = unicode(summary, 'utf-8')
        self.content = unicode(content, 'utf-8')
        self.date = unicode(date, 'utf-8')

    def __str__(self):
        return '%s\n\n%s' % (self.title, self.summary)

class Desite(object):
    def __init__(self, doc_root, templates, output):
        self.env = Environment(loader=ChoiceLoader([AsciiDocLoader([doc_root]), FileSystemLoader([templates])]))
        # Special env for loading file as-is, i.e. without the asciidoc-loader
        self.fs_env = Environment(loader = FileSystemLoader([doc_root, templates]))
        self.output = output
        self.indexMap = []

    def _progress(self, msg):
        print >>sys.stderr, msg

    def render_reference(self, docs, title, output, **variables):
        elements = []
        for d in docs:
            msg = script_doc_ddf_pb2.Document()
            with open(d, 'r') as f:
                msg.MergeFromString(f.read())
                elements += msg.elements

        for e in elements:
            e.examples = e.examples.replace('<pre>', '<pre class="prettyprint linenums lang-lua">')
            e.description = e.description.replace('<table>', '<table class="table table-striped table-bordered">')

        functions = filter(lambda e: e.type == script_doc_ddf_pb2.FUNCTION, elements)
        messages = filter(lambda e: e.type == script_doc_ddf_pb2.MESSAGE, elements)
        constants = filter(lambda e: e.type == script_doc_ddf_pb2.VARIABLE, elements)
        properties = filter(lambda e: e.type == script_doc_ddf_pb2.PROPERTY, elements)
        package = filter(lambda e: e.type == script_doc_ddf_pb2.PACKAGE, elements)
        if len(package) == 0:
            package = None
        elif len(package) == 1:
            package = package[0]
        else:
            print >>sys.stderr, "WARNING: Multiple packages specified for %s" % title
            package = package[0]

        self.render('ref.html', output,
                    package = package,
                    functions = functions,
                    messages = messages,
                    constants = constants,
                    properties = properties,
                    title = title,
                    **variables)

    def _split_blog(self, str):
        found_start = False
        acc = None
        lst = []
        for line in str.split('\n'):
            # NOTE: This parsing code is probably a bit fragile
            # and might break in future versions of asciidoc.
            # A better solution would be to output the metadata directly
            # from asciidoc or perhaps output docbook-format and parse the xml
            line += '\n'
            if '<div class="sect1">' in line:
                if acc:
                    content = acc.getvalue()
                    id, title = re.match('.*?<h2 id="(.*?)">(.*?)</h2>', content, re.DOTALL).groups()
                    id = id[1:]
                    summary = re.match('.*?</em>.*?<p.*?>(.*?)</p>', content, re.DOTALL).groups()[0]
                    date = re.match('.*?<em>(.*?)</em>', content, re.DOTALL).groups()[0]
                    # Some basic transformation
                    # escaped & to and
                    id = id.replace('_amp_', '_and_')
                    lst.append(BlogPost(id, title, summary, content, date))
                acc = StringIO()
                acc.write(line)
            elif acc:
                acc.write(line)
        return lst

    def render_blog(self, blog, output_dir):
        blog_html = self.env.get_template(blog).render().encode('UTF-8')
        posts = self._split_blog(blog_html)
        for p in posts:
            output = '%s/%s.html' % (output_dir, p.id)
            self.render('blog_template.html', output, post = p, active_page = 'blog', disqus = True)

        self.render('blog_index.html', '%s/sindex.html' % output_dir, posts = posts, active_page = 'blog')

    def render_asciidoc(self, asciidoc, output = None, **variables):
        """Renders a asciidoc generated html to a static file
        using a jinja2 template file"""
        if not output:
            output = asciidoc
        asciidoc_html = self.fs_env.get_template(asciidoc).render().encode('UTF-8')
        title = re.match('.*?<title>(.*?)</title>.*?', asciidoc_html, re.DOTALL).groups()[0]
        return self.render('asciidoc.html', output, asciidoc = asciidoc, title = title, **variables)

    def render(self, file, output = None, skip_index = False, **variables):
        """Renders a jinja2 template"""
        template = self.env.get_template(file)
        if output:
            out = os.path.join(self.output, output)
        else:
            out = os.path.join(self.output, file)
        out_dir = os.path.dirname(out)
        if not os.path.exists(out_dir): os.makedirs(out_dir)

        root = os.path.relpath(self.output, out_dir) + '/'
        context = { }
        if not 'root' in variables:
            context['root'] = root

        context.update(variables)
        doc = template.render(**context).encode('utf-8')
        with open(out, 'w') as f:
            f.write(doc)
        self._progress('Writing %s' % out)

        if out.endswith('.html') and not skip_index:
            self.indexMap.append('/' + os.path.relpath(out, self.output))

if __name__ == '__main__':
    usage = '''usage: %prog [options] script'''
    parser = optparse.OptionParser(usage)

    dynamo_home = os.environ.get('DYNAMO_HOME')
    doc_root = os.path.join(dynamo_home, 'share')

    parser.add_option('--doc-root', dest='doc_root',
                      default = doc_root,
                      help = 'Asciidoc templates root where asciidoc generated html-files resides, default "%default"')

    parser.add_option('--templates', dest='templates',
                      default = 'templates',
                      help = 'Templates directory, default "%default"')

    parser.add_option('--output', dest='output',
                      default = 'war',
                      help = 'Output directory, default "%default"')

    options, args = parser.parse_args()

    if len(args) == 0:
        parser.error('No script specified')

    ds = Desite(options.doc_root, options.templates, options.output)

    globals = { 'dynamo_home' : dynamo_home,
                'asciidoc' : ds.render_asciidoc,
                'render' : ds.render,
                'blog' : ds.render_blog,
                'ref' : ds.render_reference }
    variables = {}
    execfile(args[0], globals, variables)

    with open(os.path.join(ds.output, 'index_map.txt'), 'w') as f:
        f.write('\n'.join(ds.indexMap))
        f.write('\n')
