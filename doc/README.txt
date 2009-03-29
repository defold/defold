Setup
=====

Windows
-------

* Requirements
 - Visual Studio 2008

Steps
-------

1. Install [python 2.5](http://www.python.org/ftp/python/2.5.4/python-2.5.4.msi)

2. Install [setup tools 0.6c9](http://pypi.python.org/packages/2.5/s/setuptools/setuptools-0.6c9.win32-py2.5.exe#md5=602d06054ec1165e995ae54ac30884d7) for python 2.5 

3. *DYNAMO_HOME* directory
  - Create a *DYNAMO\_HOME* directory, eg c:/tmp/dynamo_home
  - NOTE: Use **forward** slash for *DYNAMO_HOME*

4. Environment variables (My Computer/Properties/Advanced/Environment Variables):
   - Add 2python to *PATH*
       - `C:/Python2.5`
       - `C:/Python2.5/Scripts`
   - Set *DYNAMO_HOME* environment variable

5. Logout, login from windows

6. SSH keys (to login to linux server without password)
  - Start Git Bash shell
  - On remote machine:

            # ssh YOUR_USER_NAME@overrated.dyndns.org
            # ssh-keygen
                - Press <enter> on all questions
            # cd .ssh
            # cp id_rsa.pub authorized_keys
            # exit

  - On local machine:

            # cd
            # mkdir .ssh
            # scp YOUR_USER_NAME@overrated.dyndns.org:.ssh/id_rsa .ssh
                - You can now use overrated.dyndns.org without password! :-)

7. Bootstrap
  - Start Git Bash shell
  - Clone "dynamo"

             # git clone ssh://YOUR_USER_NAME@overrated.dyndns.org/repo/dynamo

  - Bootstrap/update *DYNAMO_HOME*
      
             # sh ./dynamo/bin/update-ext.sh

