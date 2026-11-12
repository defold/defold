/*
  Copyright (C) 2005 Commonwealth Scientific and Industrial Research
  Organisation (CSIRO) Australia

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  - Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  - Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  - Neither the name of CSIRO Australia nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
  PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __OGGZ_TOOLS_H__
#define __OGGZ_TOOLS_H__

#include "config.h"
#include <getopt.h>

const char *
ot_page_identify (OGGZ *oggz, const ogg_page * og, char ** info);

/*
 * Print a number of bytes to 3 significant figures
 * using standard abbreviations (GB, MB, kB, byte[s])
 */
int ot_fprint_bytes (FILE * stream, long nr_bytes);

/*
 * Print a bitrate to 3 significant figures
 * using quasi-standard abbreviations (Gbps, Mbps, kbps, bps)
 */
int ot_print_bitrate (long bps);

int ot_fprint_time (FILE * stream, double seconds);

int ot_fprint_granulepos (FILE * stream, OGGZ * oggz, long serialno,
                          ogg_int64_t granulepos);

/*
 * Tool initialization function. Sets stdin, stdio to binary on windows etc.
 * Call this at the beginning of main().
 */
void ot_init (void);

/*
 * Print options. Must use these in response to -? for each command,
 * for bash completion.
 */
void ot_print_short_options (char * optstring);

void ot_print_options (struct option long_options[], char * optstring);

#endif /* __OGGZ_TOOLS_H__ */
