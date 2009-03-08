Setup:

Windows:

* Requirements
 - Visual Studio 2008

1) Install python 2.5 
  - http://www.python.org/ftp/python/2.5.4/python-2.5.4.msi

2) Install python setup tools 0.6c9 for python 2.5 
  - http://pypi.python.org/packages/2.5/s/setuptools/setuptools-0.6c9.win32-py2.5.exe#md5=602d06054ec1165e995ae54ac30884d7

3) DYNAMO_EXT directory
  - Create a DYNAMO_EXT directory, eg c:/tmp/dynamo_ext
  - *NOTE* Use *FORWARD* slash for DYNAMO_EXT

4) Environment variables (My Computer/Properties/Advanced/Environment Variables):
  - Add python to PATH 
    - C:/Python2.5
    - C:/Python2.5/Scripts
    - Set DYNAMO_EXT environment variable
    - Set USER to linux username

5) Log out/in

6) SSH keys (to login to linux server without password)
  - Start Git Bash shell

  - On remote machine:
    - Log in to overrated.dyndns.org
    - ssh YOUR_USER_NAME@overrated.dyndns.org
    - ssh-keygen
     - Press <enter> on all questions
    - cd .ssh
    - cp id_rsa.pub authorized_keys
    - exit (logout)

  - On local machine:
    - cd
    - mkdir .ssh
    - scp YOUR_USER_NAME@overrated.dyndns.org:.ssh/id_rsa .
    - You can now use overrated.dyndns.org without password! :-)

7) Bootstrap
  - Start Git Bash shell
  - git clone YOUR_USER_NAME@overrated.dyndns.org:/home/chmu/dynamo
  - Bootstrap/update DYNAMO_EXT
    - sh ./dynamo/bin/update-ext.sh

