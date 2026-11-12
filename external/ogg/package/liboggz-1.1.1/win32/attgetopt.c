/**
 * @file attgetopt.c
 * @ingroup wbxml2xml_tool
 * @ingroup xml2wbxml_tool
 *
 * AT&T's public domain implementation of getopt.
 *
 * From the mod.sources newsgroup, volume 3, issue 58, with modifications
 * to bring it up to 21st century C.
 *
 * Taken from Kannel Project (http://www.kannel.org/)
 */

#include <stdio.h>
#include <string.h>


#define ERR(s, c)     if (opterr) (void) fprintf(stderr, "%s: %s\n", argv[0], s) 

int     opterr = 1;
int     optind = 1;
int     optopt;
char    *optarg;

int getopt(int argc, char **argv, char *opts)
{
    static int sp = 1;
    register int c;
    register char *cp;

    if(sp == 1) {
        if(optind >= argc ||
           argv[optind][0] != '-' || argv[optind][1] == '\0')
                return(EOF);
        else if(strcmp(argv[optind], "--") == 0) {
            optind++;
            return(EOF);
        }
    }
    optopt = c = argv[optind][sp];
    if(c == ':' || (cp=strchr(opts, c)) == NULL) {
        ERR(": illegal option -- ", c);
        if(argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return('?');
    }
    if(*++cp == ':') {
        if(argv[optind][sp+1] != '\0')
            optarg = &argv[optind++][sp+1];
        else if(++optind >= argc) {
            ERR(": option requires an argument -- ", c);
            sp = 1;
            return('?');
        } else
            optarg = argv[optind++];
        sp = 1;
    } else {
        if(argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = NULL;
    }
    return(c);
}
