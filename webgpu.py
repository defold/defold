import sys, os

if __name__ == '__main__':

    path = "webgpu.log"
    with open(path, 'rt') as f:
        lines = f.readlines()

    count = 0
    for line in lines:
        count = count + 1
        if line.startswith('. '):
            last = count
        elif 'webgpu.h' in line:
            print(last-1, ":", lines[last-1])
            print(count, ":", line)
