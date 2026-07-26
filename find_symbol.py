import os, sys

def find(path, symbol):
    name = os.path.basename(path)
    os.system(f'echo {name} && nm {path} | grep {symbol}')


if __name__ == '__main__':

    path = sys.argv[1]
    symbol = sys.argv[2]
    print(f"Searching {path} for symbol '{symbol}'")

    for root, dirs, files in os.walk(path):
        for f in files:
            _, ext = os.path.splitext(f)
            if ext not in ['.a', '.o']:
                continue
            find(os.path.join(root, f), symbol)
