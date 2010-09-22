import dlib, random, gzip, os

f = gzip.GzipFile(os.path.join(os.path.dirname(__file__), 'american-english.gz'))
words = [ line.strip() for line in f ]
f.close()

def run(function):

    hash_table = {}
    i = 0

    n_collisions = 0
    tot_tested = 0
    for word in words:
        for i in range(30):
            sent = '%s%d' % (word, i)
            h = function(sent)
            if h in hash_table and hash_table[h] != sent:
                n_collisions += 1
            else:
                hash_table[h] = sent
            tot_tested += 1

    print "Collisions %d/%d (%f %%)" % (n_collisions, tot_tested, float(n_collisions) / tot_tested)

print('dmHashBuffer32')
run(dlib.dmHashBuffer32)

print('dmHashBuffer64')
run(dlib.dmHashBuffer64)
