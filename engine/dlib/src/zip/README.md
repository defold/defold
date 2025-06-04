To update this folder:
* go to https://github.com/kuba--/zip
* Download miniz.h, zip.h and zip.c
* Rename zip.c to zip.cpp
* In miniz.h, remove the `extern "C"`, so that the functions will be private within zip.cpp
