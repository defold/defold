import os
import sys
import shutil

print("HELLO WORLD!!!!!!!!!!")

def extract_res(package_name, dir, output_dir):
    res_path = os.path.join(dir, "res")
    if os.path.exists(res_path):
        if os.listdir(res_path):
            print("Copying " + package_name)
            output_package_dir = os.path.join(output_dir, package_name)
            shutil.copytree(res_path, output_package_dir)


def iterate_and_extract(dir, output_dir):
    for package_name in os.listdir(dir):
        path = os.path.join(dir, package_name)
        if os.path.isdir(path):
            extract_res(package_name, path, output_dir)

# python extract_res.py build/result/packed/aar /output_directory/
def main():
    iterate_and_extract(sys.argv[1], sys.argv[2])

if __name__ == "__main__":
    main()
