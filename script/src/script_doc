#!/usr/bin/env python

import re
import logging
from optparse import OptionParser
import script_doc_ddf_pb2

def _stripCommentStars(str):
    lines = str.split('\n')
    ret = []
    for line in lines:
        line = line.strip()
        index = line.find('*')
        if index != -1:
            line = line[index+1:]
        ret.append(line.strip())
    return '\n'.join(ret)

def _parseComment(str, doc):
    str = _stripCommentStars(str)
    lst = re.findall('@(.*?)[ ]+(.*?)$', str, re.MULTILINE)

    name_found = False
    for (tag, value) in lst:
        value = value.strip()
        if tag == 'name':
            name_found = True

    if not name_found:
        logging.warn('Missing tag name in "%s"' % str)
        return

    function = doc.functions.add()

    desc_end = min(len(str), str.find('@'))
    function.description = str[0:desc_end].strip().replace('\n', ' ')
    function.return_ = ''

    for (tag, value) in lst:
        value = value.strip()
        if tag == 'name':
            function.name = value
        elif tag == 'return':
            function.return_ = value
        elif tag == 'param':
            tmp = value.split(' ', 1)
            if len(tmp) < 2:
                tmp = [tmp[0], '']
            param = function.parameters.add()
            param.name = tmp[0]
            param.doc = tmp[1]

def parseDocument(doc_str):
    doc = script_doc_ddf_pb2.Document()
    lst = re.findall('/\*#(.*?)\*/', doc_str, re.DOTALL)
    for comment_str in lst:
        comment = _parseComment(comment_str, doc)

    return doc

if __name__ == '__main__':
    usage = "usage: %prog [options] INFILE(s) OUTFILE"
    parser = OptionParser(usage = usage)
    (options, args) = parser.parse_args()

    if len(args) < 2:
        parser.error('At least two arguments required')

    doc_str = ''
    for name in args[:-1]:
        f = open(name, 'rb')
        doc_str += f.read()
        f.close()

    output_file = args[-1]
    f = open(output_file, "wb")
    doc = parseDocument(doc_str)
    f.write(doc.SerializeToString())
    f.close()

