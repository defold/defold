import glob, os
for path in glob.glob('tmp/cache/*'):
    if len(os.path.basename(path)) == 2:
        for file_path in glob.glob(os.path.join(path, '*')):
            file = open(file_path, 'w')
            file.write('CORRUPTED_FILE')
            file.close()

