import argparse
import json
import os

parser = argparse.ArgumentParser(description = 'A script to convert .json BudouX model to C.')
parser.add_argument('model_file', help=  'Path to the BudouX model .json file.')
parser.add_argument('model_name', help=  'Name of the model in the output file.')
parser.add_argument('out_file', help=  'Name of the output file.')
args = parser.parse_args()

print('Loading model %s...' %(args.model_file))

with open(os.path.join(args.model_file), encoding='utf-8') as f:
    model = json.load(f)

def dump_tab(f, name, tab):

    f.write('// Weights for %s\n' %(name) )

    f.write('static uint64_t keys_%s_%s[] = {\n' %(args.model_name, name))
    i = 0
    f.write('    ')
    for v in tab:
        f.write('0x%x, ' %(v[0]))
        i = i+1
        if (i >= 10):
            f.write('\n    ')
            i = 0;
    f.write('};\n')

    f.write('static int16_t values_%s_%s[] = {\n' %(args.model_name, name))
    i = 0
    f.write('    ')
    for v in tab:
        f.write('%d, ' %(v[1]))
        i = i+1
        if (i >= 10):
            f.write('\n    ')
            i = 0;
    f.write('};\n\n')


def dump_unigram_weights(f, name):
    weights = model.get(name)
    if weights == None:
        print('Missing weights %s' %(name))
        return
        
    tab = []
    for key in weights:
        k = ord(key[0])
        tab.append((k, weights[key]))
    tab.sort(key=lambda x:x[0])

    dump_tab(f, name, tab)

    return len(tab)


def dump_bigram_weights(f, name):
    weights = model.get(name)
    if weights == None:
        print('Missing weights %s' %(name))
        return
        
    tab = []
    for key in weights:
        klen = len(key)
        k = ord(key[0])
        if klen > 1:
            k = k | (ord(key[1]) << 20)
        tab.append((k, weights[key]))
    tab.sort(key=lambda x:x[0])

    dump_tab(f, name, tab)

    return len(tab)

def dump_trigram_weights(f, name):
    weights = model.get(name)
    if weights == None:
        print('Missing weights %s' %(name))
        return
        
    tab = []
    for key in weights:
        klen = len(key)
        k = ord(key[0])
        if klen > 1:
            k = k | (ord(key[1]) << 20)
        if klen > 2:
            k = k | (ord(key[2]) << 40)
        tab.append((k, weights[key]))
    tab.sort(key=lambda x:x[0])

    dump_tab(f, name, tab)

    return len(tab)
    
def dump_line(f, name, num):
    f.write('    .%s = { .keys = keys_%s_%s, .values = values_%s_%s, .count = %d, },\n' %(name, args.model_name, name, args.model_name, name, num))
    
#dump_unigram_weights('UW1')

print('Writing model %s...' %(args.out_file))

with open(args.out_file, "w") as f:

    f.write('//\n')
    f.write('// Genereted with convert.py, do not edit.\n')
    f.write('//\n')

    num_UW1 = dump_unigram_weights(f, 'UW1')
    num_UW2 = dump_unigram_weights(f, 'UW2')
    num_UW3 = dump_unigram_weights(f, 'UW3')
    num_UW4 = dump_unigram_weights(f, 'UW4')
    num_UW5 = dump_unigram_weights(f, 'UW5')
    num_UW6 = dump_unigram_weights(f, 'UW6')

    num_BW1 = dump_bigram_weights(f, 'BW1')
    num_BW2 = dump_bigram_weights(f, 'BW2')
    num_BW3 = dump_bigram_weights(f, 'BW3')

    num_TW1 = dump_trigram_weights(f, 'TW1')
    num_TW2 = dump_trigram_weights(f, 'TW2')
    num_TW3 = dump_trigram_weights(f, 'TW3')
    num_TW4 = dump_trigram_weights(f, 'TW4')

    base_score = -sum(sum(g.values()) for g in model.values()) * 0.5

    f.write('budoux_model_t model_%s = {\n' %(args.model_name))
    
    f.write('    .base_score = %d,\n' %base_score)

    dump_line(f, 'UW1', num_UW1)
    dump_line(f, 'UW2', num_UW2)
    dump_line(f, 'UW3', num_UW3)
    dump_line(f, 'UW4', num_UW4)
    dump_line(f, 'UW5', num_UW5)
    dump_line(f, 'UW6', num_UW6)

    dump_line(f, 'BW1', num_BW1)
    dump_line(f, 'BW2', num_BW2)
    dump_line(f, 'BW3', num_BW3)

    dump_line(f, 'TW1', num_TW1)
    dump_line(f, 'TW2', num_TW2)
    dump_line(f, 'TW3', num_TW3)
    dump_line(f, 'TW4', num_TW4)

    f.write('};')


'''
# one codepoint
UW1 = model.get('UW1', [])
UW2 = model.get('UW2', [])
UW3 = model.get('UW3', [])
UW4 = model.get('UW4', [])
UW5 = model.get('UW5', [])
UW6 = model.get('UW6', [])

# two codepoints
BW1 = model.get('BW1', [])
BW2 = model.get('BW2', [])
BW3 = model.get('BW3', [])

# 3 codepoints
TW1 = model.get('TW1', [])
TW2 = model.get('TW2', [])
TW3 = model.get('TW3', [])
TW4 = model.get('TW4', [])
'''

#print(json.dumps(model, indent=2))
