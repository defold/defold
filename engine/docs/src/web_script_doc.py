import json, os
from optparse import OptionParser

def transform_refdoc(doc):
    functions = []
    macros = []
    messages = []
    constants = []
    properties = []
    structs = []
    enums = []
    typedefs = []
    methods = []
    for e in doc['elements']:
        if e['type'] == 'FUNCTION':
            functions.append(e)
        if e['type'] == 'MACRO':
            macros.append(e)
        elif e['type'] == 'MESSAGE':
            messages.append(e)
        elif e['type'] == 'VARIABLE':
            constants.append(e)
        elif e['type'] == 'PROPERTY':
            properties.append(e)
        elif e['type'] == 'STRUCT':
            structs.append(e)
        elif e['type'] == 'ENUM':
            enums.append(e)
        elif e['type'] == 'TYPEDEF':
            typedefs.append(e)
        elif e['type'] == 'METHOD':
            methods.append(e)

    t = {}
    t['functions'] = functions
    t['methods'] = methods
    t['macros'] = macros
    t['messages'] = messages
    t['constants'] = constants
    t['properties'] = properties
    t['structs'] = structs
    t['enums'] = enums
    t['typedefs'] = typedefs
    t['info'] = doc['info']
    return t

# Merge doc2 into doc1
def merge_refdocs(doc1, doc2):
    if doc1['info']['name'] == '':
        doc1['info']['name'] = doc2['info']['name']
    if doc1['info']['brief'] == '':
        doc1['info']['brief'] = doc2['info']['brief']
    if doc1['info']['description'] == '':
        doc1['info']['description'] = doc2['info']['description']
    doc1['functions'].extend(doc2['functions'])
    doc1['methods'].extend(doc2['methods'])
    doc1['macros'].extend(doc2['macros'])
    doc1['messages'].extend(doc2['messages'])
    doc1['constants'].extend(doc2['constants'])
    doc1['properties'].extend(doc2['properties'])
    doc1['structs'].extend(doc2['structs'])
    doc1['enums'].extend(doc2['enums'])
    doc1['typedefs'].extend(doc2['typedefs'])
    return doc1


def transform_doc(raw_json, version, jsondata, index):
    # Transform and merge namespaces
    namespace = raw_json['info']['namespace']
    if namespace in jsondata:
        # If namespace is saved - merge
        a = jsondata[namespace]
        b = transform_refdoc(raw_json)
        jsondata[namespace] = merge_refdocs(a, b)
    else:
        jsondata[namespace] = transform_refdoc(raw_json)
        jsondata[namespace]['info']['version'] = version

    # Save index
    for namespace in jsondata:
        index[namespace] = jsondata[namespace]['info']

    return jsondata, index
    # Save individual entries
#    for namespace in jsondata:
#        store_json(sha1, namespace + '.json', jsondata[namespace])
#        if latest:
#            store_json('latest', namespace + '.json', jsondata[namespace])
#

if __name__ == '__main__':
    usage = "usage: %prog [options] INFILE(s) OUTDIR"
    parser = OptionParser(usage = usage)
    parser.add_option("-v", "--version", dest="version",
                      help="Version string", metavar="VERSION", default='latest')

    (options, args) = parser.parse_args()

    if len(args) < 2:
        parser.error('At least one argument required')

    jsondata = {}
    index = {}
    orig_json = ''
    for name in args[:-1]:
        f = open(name, 'rb')
        raw_json = json.loads(f.read())
        jsondata, index = transform_doc(raw_json, options.version, jsondata, index)
        f.close()

    output_dir = args[-1]

    for namespace in jsondata:
        output_file = os.path.join(output_dir, namespace + '.json')
        f = open(output_file, "wb")
        json.dump(jsondata[namespace], f, indent = 2)

    output_file = os.path.join(output_dir, 'index.json')
    f = open(output_file, "wb")
    json.dump(index, f, indent = 2)
