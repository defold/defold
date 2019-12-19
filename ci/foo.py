import sys
import subprocess
import os


def exec_command(args):
    print('[EXEC] %s' % args)
    process = subprocess.Popen(args, stdout = subprocess.PIPE, stderr = subprocess.STDOUT, shell = False)

    output = ''
    while True:
        line = process.stdout.readline()
        if line != '':
            output += line
            print line.rstrip()
            sys.stdout.flush()
            sys.stderr.flush()
        else:
            break

    if process.wait() != 0:
        sys.exit(process.returncode)

    return output

def mac_certificate():
    print("mac_certificate()")
    # This certificate must be installed on the computer performing the operation
    certificate = 'Developer ID Application: Midasplayer Technology AB (ATT58V7T33)'
    if exec_command(['security', 'find-identity', '-p', 'codesigning', '-v']).find(certificate) >= 0:
        return certificate
    else:
        return None

def sign_files(bundle_dir):
    print("sign_files()")
    certificate = mac_certificate()
    print("sign_files() certificate", certificate)
    if certificate == None:
        print("Warning: Codesigning certificate not found, files will not be signed")
    else:
        print("sign_files() calling codesign")
        exec_command(['codesign', '--deep', '-s', certificate, bundle_dir])
        print("sign_files() codesign done")


def main(argv):
    sign_files("foo")

if __name__ == "__main__":
    main(sys.argv[1:])
