#!/usr/bin/env python

import re
import logging
import sys
import StringIO
from optparse import OptionParser
import script_doc_ddf_pb2

def _strip_comment_stars(str):
    lines = str.split('\n')
    ret = []
    for line in lines:
        line = line.strip()
        index = line.find('*')
        if index != -1:
            line = line[index+1:]
        ret.append(line.strip())
    return '\n'.join(ret)

def _parse_comment(str):
    str = _strip_comment_stars(str)
    lst = re.findall('@([^ ]+)[ ]*(.*?)$', str, re.MULTILINE)

    name_found = False
    is_variable = False
    for (tag, value) in lst:
        tag = tag.strip()
        value = value.strip()
        if tag == 'name':
            name_found = True
        elif tag == 'variable':
            is_variable = True

    if not name_found:
        logging.warn('Missing tag name in "%s"' % str)
        return

    element = script_doc_ddf_pb2.Element()
    if is_variable:
        element.type = script_doc_ddf_pb2.VARIABLE
    else:
        element.type = script_doc_ddf_pb2.FUNCTION

    desc_end = min(len(str), str.find('@'))
    element.description = str[0:desc_end].strip().replace('\n', ' ')
    element.return_ = ''

    for (tag, value) in lst:
        value = value.strip()
        if tag == 'name':
            element.name = value
        elif tag == 'return':
            element.return_ = value
        elif tag == 'param':
            tmp = value.split(' ', 1)
            if len(tmp) < 2:
                tmp = [tmp[0], '']
            param = element.parameters.add()
            param.name = tmp[0]
            param.doc = tmp[1]
    return element

def parse_document(doc_str):
    doc = script_doc_ddf_pb2.Document()
    lst = re.findall('/\*#(.*?)\*/', doc_str, re.DOTALL)
    element_list = []
    for comment_str in lst:
        element = _parse_comment(comment_str)
        if element:
            element_list.append(element)

    element_list.sort(cmp = lambda x,y: cmp(x.name, y.name))
    for i, e in enumerate(element_list):
        doc.elements.add().MergeFrom(e)

    return doc

from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.message import Message
import json

def message_to_dict(message):
    ret = {}
    for field in message.DESCRIPTOR.fields:
        value = getattr(message, field.name)
        if field.label == FieldDescriptor.LABEL_REPEATED:
            lst = []
            for element in value:
                if isinstance(element, Message):
                    lst.append(message_to_dict(element))
                else:
                    lst.append(str(element))
            ret[field.name] = lst
        elif field.type == FieldDescriptor.TYPE_MESSAGE:
            ret[field.name] = message_to_dict(value)
        elif field.type == FieldDescriptor.TYPE_ENUM:
            ret[field.name] = field.enum_type.values_by_number[value].name
        else:
            ret[field.name] = str(value)
    return ret

if __name__ == '__main__':
    usage = "usage: %prog [options] INFILE(s) OUTFILE"
    parser = OptionParser(usage = usage)
    parser.add_option("-t", "--type", dest="type",
                      help="Supported formats: protobuf and json. default is protobuf", metavar="TYPE", default='protobuf')
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
    doc = parse_document(doc_str)

    if options.type == 'protobuf':
        f.write(doc.SerializeToString())
    elif options.type == 'json':
        doc_dict = message_to_dict(doc)
        json.dump(doc_dict, f, indent = None)
    else:
        print 'Unknown type: %s' % options.type
        sys.exit(5)

    f.close()


