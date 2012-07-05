#!/usr/bin/env python

import optparse
import os, re, sys
from jinja2 import FileSystemLoader, ChoiceLoader, Environment

class AsciiDocLoader(FileSystemLoader):
    """Custom FileSystemLoader for asciidoc-files that extracts the payload from the generated file"""
    def get_source(self, environment, template):
        res = list(FileSystemLoader.get_source(self, environment, template))
        # Extract the "payload" of the generated asciidoc-file
        reg_exp = "[\\s\\S]*?<body[\\s\\S]*?>([\\s\\S]*?)</body>[\\s\\S]*?"
        match = re.match(reg_exp, res[0], re.DOTALL)
        res[0] = match.groups()[0]
        return res

class Desite(object):
    def __init__(self, doc_root, templates, output):
        self.env = Environment(loader=ChoiceLoader([AsciiDocLoader([doc_root]), FileSystemLoader([templates])]))
        self.output = output

    def _progress(self, msg):
        print >>sys.stderr, msg

    def render_asciidoc(self, asciidoc, output = None, **variables):
        """Renders a asciidoc generated html to a static file
        using a jinja2 template file"""
        if not output:
            output = asciidoc
        return self.render('asciidoc.html', output, asciidoc = asciidoc, **variables)

    def render(self, file, output = None, **variables):
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

    globals = { 'asciidoc' : ds.render_asciidoc,
                'render' : ds.render, }
    variables = {}
    execfile(args[0], globals, variables)
