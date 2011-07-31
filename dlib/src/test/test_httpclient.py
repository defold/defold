import shutil, os
shutil.rmtree('tmp/cache', True)
shutil.rmtree('tmp/http_files', True)
os.mkdir('tmp/http_files')

def http_file(name, content):
    f = open('tmp/http_files/%s' % name, 'wb')
    f.write(content)
    f.close()

http_file('a.txt', 'You will find this data in a.txt and d.txt')
http_file('b.txt', 'Some data in file b')
http_file('c.txt', 'Some data in file c')
http_file('d.txt', 'You will find this data in a.txt and d.txt')

