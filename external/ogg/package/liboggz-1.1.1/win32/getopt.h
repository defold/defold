/**
 * @file getopt.h
 * @ingroup wbxml2xml_tool
 * @ingroup xml2wbxml_tool
 *
 * @author Kannel Team (http://www.kannel.org/)
 *
 * @brief getopt() implementation
 */

#ifndef WBXML_GETOPT_H
#define WBXML_GETOPT_H

int getopt(int argc, char **argv, char *opts);
extern int opterr;
extern int optind;
extern int optopt;
extern char *optarg;

#endif
