/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#else
#  define PRId64 "I64d"
#endif

#include "oggz/oggz.h"

#include "oggz_tools.h"

#define MAX_ERRORS 10

#define SUBSECONDS 1000.0

/* #define DEBUG */

typedef ogg_int64_t timestamp_t;

typedef struct _OVData {
  OGGZ * writer;
  OggzTable * missing_eos;
  OggzTable * packetno;

  int theora_count;
  int audio_count;

  int chain_ended;
} OVData;

typedef struct {
  int error;
  char * description;
} error_text;

static error_text errors[] = {
  {-20, "Packet belongs to unknown serialno"},
  {-24, "Granulepos decreasing within track"},
  {-5, "Multiple bos pages"},
  {-6, "Multiple eos pages"},
  {0, NULL}
};

static char * progname;
static int max_errors = MAX_ERRORS;
static int multifile = 0;
static char * current_filename = NULL;
static timestamp_t current_timestamp = 0;
static int exit_status = 0;
static int nr_errors = 0;
static int prefix = 0, suffix = 0;

static void
list_errors (void)
{
  int i = 0;

  printf ("  File contains no Ogg packets\n");
  printf ("  Packets out of order\n");
  for (i = 0; errors[i].error; i++) {
    printf ("  %s\n", errors[i].description);
  }
  printf ("  eos marked but no bos\n");
  printf ("  Missing eos pages\n");
  printf ("  eos marked on page with no completed packets\n");
  printf ("  Granulepos on page with no completed packets\n");
  printf ("  Theora video bos page after audio bos page\n");
  printf ("  Terminal header page has non-zero granulepos\n");
  printf ("  Terminal header page contains non-header packet\n");
  printf ("  Terminal header page contains non-header segment\n");
}

static void
usage (char * progname)
{

  printf ("Usage: %s [options] filename ...\n", progname);
  printf ("Validate the Ogg framing of one or more files\n");
  printf ("\n%s detects the following errors in Ogg framing:\n", progname);

  list_errors ();

  printf ("\nError reporting options\n");
  printf ("  -M num, --max-errors num\n");
  printf ("                         Exit after the specified number of errors.\n");
  printf ("                         A value of 0 specifies no maximum. Default: %d\n", MAX_ERRORS);
  printf ("  -p, --prefix           Treat input as the prefix of a stream; suppress\n");
  printf ("                         warnings about missing end-of-stream markers\n");
  printf ("  -s, --suffix           Treat input as the suffix of a stream; suppress\n");
  printf ("                         warnings about missing beginning-of-stream markers\n");
  printf ("                         on the first chain\n");
  printf ("  -P, --partial          Treat input as a the middle portion of a stream;\n");
  printf ("                         equivalent to both --prefix and --suffix\n");

  printf ("\nMiscellaneous options\n");
  printf ("  -h, --help             Display this help and exit\n");
  printf ("  -E, --help-errors      List known types of error and exit\n");
  printf ("  -v, --version          Output version information and exit\n");
  printf ("\n");
  printf ("Exit status is 0 if all input files are valid, 1 otherwise.\n\n");
  printf ("Please report bugs to <ogg-dev@xiph.org>\n");
}

static void
exit_out_of_memory (void)
{
  fprintf (stderr, "%s: Out of memory\n", progname);
  exit (1);
}

static int
log_error (void)
{
  if (multifile && nr_errors == 0) {
    fprintf (stderr, "%s: Error:\n", current_filename);
  }

  exit_status = 1;

  nr_errors++;
  if (max_errors && nr_errors > max_errors)
    return OGGZ_STOP_ERR;

  return OGGZ_STOP_OK;
}

static ogg_int64_t
gp_to_granule (OGGZ * oggz, long serialno, ogg_int64_t granulepos)
{
  int granuleshift;
  ogg_int64_t iframe, pframe, granule;

  granuleshift = oggz_get_granuleshift (oggz, serialno);

  iframe = granulepos >> granuleshift;
  pframe = granulepos - (iframe << granuleshift);
  granule = iframe+pframe;

  if (oggz_stream_get_content (oggz, serialno) == OGGZ_CONTENT_DIRAC)
    granule >>= 9;

  return granule;
}

static timestamp_t
gp_to_time (OGGZ * oggz, long serialno, ogg_int64_t granulepos)
{
  ogg_int64_t gr_n, gr_d;
  ogg_int64_t granule;

  if (granulepos == -1) return -1.0;
  if (oggz_get_granulerate (oggz, serialno, &gr_n, &gr_d) != 0) return -1.0;

  granule = gp_to_granule (oggz, serialno, granulepos);

  return (timestamp_t)((double)(SUBSECONDS * granule * gr_d) / (double)gr_n);
}

static void
ovdata_init (OVData * ovdata)
{
  int flags;

  current_timestamp = 0;

  flags = OGGZ_WRITE|OGGZ_AUTO;
  if (prefix) flags |= OGGZ_PREFIX;
  if (suffix) flags |= OGGZ_SUFFIX;

  if ((ovdata->writer = oggz_new (flags)) == NULL) {
    fprintf (stderr, "oggz-validate: unable to create new writer\n");
    exit (1);
  }

  if ((ovdata->missing_eos = oggz_table_new ()) == NULL)
    exit_out_of_memory();

  if ((ovdata->packetno = oggz_table_new ()) == NULL)
    exit_out_of_memory();

  ovdata->theora_count = 0;
  ovdata->audio_count = 0;
  ovdata->chain_ended = 0;
}

static void
ovdata_clear (OVData * ovdata)
{
  long serialno;
  int i, nr_missing_eos = 0;

  oggz_close (ovdata->writer);

  if (!prefix && (max_errors == 0 || nr_errors <= max_errors)) {
    nr_missing_eos = oggz_table_size (ovdata->missing_eos);
    for (i = 0; i < nr_missing_eos; i++) {
      log_error ();
      oggz_table_nth (ovdata->missing_eos, i, &serialno);
      fprintf (stderr, "serialno %010lu: missing *** eos\n", serialno);
    }
  }

  oggz_table_delete (ovdata->missing_eos);
  oggz_table_delete (ovdata->packetno);
}

static int
read_page (OGGZ * oggz, const ogg_page * og, long serialno, void * user_data)
{
  OVData * ovdata = (OVData *)user_data;
  ogg_int64_t gpos = ogg_page_granulepos((ogg_page *)og);
  OggzStreamContent content_type;
  int packets, packetno, headers, ret = 0;

  if (ovdata->chain_ended) {
    ovdata_clear (ovdata);
    ovdata_init (ovdata);
    suffix = 0;
  }

  if (ogg_page_bos ((ogg_page *)og)) {
    /* Register this serialno as needing eos */
    if (oggz_table_insert (ovdata->missing_eos, serialno, (void *)0x1) == NULL)
      exit_out_of_memory();

    /* Handle ordering of Theora vs. audio packets */
    content_type = oggz_stream_get_content (oggz, serialno);

    switch (content_type) {
      case OGGZ_CONTENT_THEORA:
	ovdata->theora_count++;
	if (ovdata->audio_count > 0) {
	  log_error ();
	  fprintf (stderr, "serialno %010lu: Theora video bos page after audio bos page\n", serialno);
	}
        break;
      case OGGZ_CONTENT_VORBIS:
      case OGGZ_CONTENT_SPEEX:
      case OGGZ_CONTENT_PCM:
      case OGGZ_CONTENT_FLAC0:
      case OGGZ_CONTENT_FLAC:
      case OGGZ_CONTENT_CELT:
	ovdata->audio_count++;
        break;
      default:
        break;
    }
  }

  packets = ogg_page_packets ((ogg_page *)og);

  /* Check header constraints */
  if (!suffix) {
    if (oggz_table_lookup (ovdata->missing_eos, serialno) == NULL) {
      ret = log_error ();
      fprintf (stderr, "serialno %010lu: missing *** bos\n", serialno);
    }

    packetno = (int)oggz_table_lookup (ovdata->packetno, serialno);
    headers = oggz_stream_get_numheaders (oggz, serialno);
    if (packetno < headers-1) {
      /* The previous page was headers, and more are expected */
      packetno += packets;
      if (oggz_table_insert (ovdata->packetno, serialno, (void *)packetno) == NULL)
        exit_out_of_memory();

      if (packetno == headers && gpos != 0) {
        ret = log_error ();
        fprintf (stderr, "serialno %010lu: Terminal header page has non-zero granulepos\n", serialno);
      } else if (packetno > headers) {
        ret = log_error ();
        fprintf (stderr, "serialno %010lu: Terminal header page contains non-header packet\n", serialno);
      }
    } else if (packetno == headers) {
      /* This is the next page after the page on which the last header finished */
      if (ogg_page_continued (og)) {
        ret = log_error ();
        fprintf (stderr, "serialno %010lu: Terminal header page contains non-header segment\n", serialno);
      }

      /* Mark packetno as greater than headers to avoid these checks for this serialno */
      if (oggz_table_insert (ovdata->packetno, serialno, (void *)(headers+1)) == NULL)
        exit_out_of_memory();
    }

  }

  /* Check EOS */
  if (ogg_page_eos((ogg_page *)og)) {
    int removed = oggz_table_remove (ovdata->missing_eos, serialno);
    if (!suffix && removed == -1) {
      ret = log_error ();
      fprintf (stderr, "serialno %010lu: *** eos marked but no bos\n",
  	       serialno);
    }

    if (packets == 0) {
      ret = log_error ();
      fprintf (stderr, "serialno %010lu: *** eos marked on page with no completed packets\n",
  	       serialno);
    }

    if (oggz_table_size (ovdata->missing_eos) == 0) {
      ovdata->chain_ended = 1;
    }
  }


  if(gpos != -1 && packets == 0) {
    ret = log_error ();
    fprintf (stderr, "serialno %010lu: granulepos %" PRId64 " on page with no completed packets, must be -1\n", serialno, gpos);
  }

  return ret;
}

static int
read_packet (OGGZ * oggz, oggz_packet * zp, long serialno, void * user_data)
{
  OVData * ovdata = (OVData *)user_data;
  ogg_packet * op = &zp->op;
  timestamp_t timestamp;
  int flush;
  int ret = 0, feed_err = 0, i;

  timestamp = gp_to_time (oggz, serialno, op->granulepos);
  if (timestamp != -1.0 && oggz_stream_get_content (oggz, serialno) != OGGZ_CONTENT_DIRAC) {
    if (timestamp < current_timestamp) {
      ret = log_error();
      ot_fprint_time (stderr, (double)timestamp/SUBSECONDS);
      fprintf (stderr, ": serialno %010lu: Packet out of order (previous ",
	       serialno);
      ot_fprint_time (stderr, (double)current_timestamp/SUBSECONDS);
      fprintf (stderr, ")\n");
    }
    current_timestamp = timestamp;
  }

  if (op->granulepos == -1) {
    flush = 0;
  } else {
    flush = OGGZ_FLUSH_AFTER;
  }

  if ((feed_err = oggz_write_feed (ovdata->writer, op, serialno, flush, NULL)) != 0) {
    ret = log_error ();
    if (timestamp == -1.0) {
      fprintf (stderr, "%" PRId64 , oggz_tell (oggz));
    } else {
      ot_fprint_time (stderr, (double)timestamp/SUBSECONDS);
    }
    fprintf (stderr, ": serialno %010lu: ", serialno);
    for (i = 0; errors[i].error; i++) {
      if (errors[i].error == feed_err) {
	fprintf (stderr, "%s\n", errors[i].description);
	break;
      }
    }
    if (errors[i].error == 0) {
      fprintf (stderr,
	       "Packet violates Ogg framing constraints: %d\n",
	       feed_err);
    }
  }

  return ret;
}

static int
validate (char * filename)
{
  OGGZ * reader;
  OVData ovdata;
  unsigned char buf[1024];
  long n, nout = 0, bytes_written = 0;
  int active = 1;

  current_filename = filename;
  current_timestamp = 0;
  nr_errors = 0;

  /*printf ("oggz-validate: %s\n", filename);*/

  if (!strncmp (filename, "-", 2)) {
    if ((reader = oggz_open_stdio (stdin, OGGZ_READ|OGGZ_AUTO)) == NULL) {
      fprintf (stderr, "oggz-validate: unable to open stdin\n");
      return -1;
    }
  } else if ((reader = oggz_open (filename, OGGZ_READ|OGGZ_AUTO)) == NULL) {
    fprintf (stderr, "oggz-validate: unable to open file %s\n", filename);
    return -1;
  }

  ovdata_init (&ovdata);

  oggz_set_read_callback (reader, -1, read_packet, &ovdata);
  oggz_set_read_page (reader, -1, read_page, &ovdata);

  while (active && (n = oggz_read (reader, 1024)) != 0) {
#ifdef DEBUG
      fprintf (stderr, "validate: read %ld bytes\n", n);
#endif
    
    if (max_errors && nr_errors > max_errors) {
      fprintf (stderr,
	       "oggz-validate --max-errors %d: maximum error count reached, bailing out ...\n",
               max_errors);
      active = 0;
    } else while ((nout = oggz_write_output (ovdata.writer, buf, n)) > 0) {
#ifdef DEBUG
      fprintf (stderr, "validate: wrote %ld bytes\n", nout);
#endif
      bytes_written += nout;
    }
  }

  oggz_close (reader);

  if (bytes_written == 0) {
    log_error ();
    fprintf (stderr, "File contains no Ogg packets\n");
  }

  ovdata_clear (&ovdata);

  return active ? 0 : -1;
}

int
main (int argc, char ** argv)
{
  int show_version = 0;
  int show_help = 0;

  /* Cache the --prefix, --suffix options and reset before validating
   * each input file */
  int opt_prefix = 0;
  int opt_suffix = 0;

  char * filename;
  int i = 1;

  char * optstring = "M:psPhvE";

#ifdef HAVE_GETOPT_LONG
  static struct option long_options[] = {
    {"max-errors", required_argument, 0, 'M'},
    {"prefix", no_argument, 0, 'p'},
    {"suffix", no_argument, 0, 's'},
    {"partial", no_argument, 0, 'P'},
    {"help", no_argument, 0, 'h'},
    {"help-errors", no_argument, 0, 'E'},
    {"version", no_argument, 0, 'v'},
    {0,0,0,0}
  };
#endif

  ot_init();

  progname = argv[0];

  if (argc < 2) {
    usage (progname);
    return (1);
  }

  if (!strncmp (argv[1], "-?", 2)) {
#ifdef HAVE_GETOPT_LONG
    ot_print_options (long_options, optstring);
#else
    ot_print_short_options (optstring);
#endif
    exit (0);
  }

  while (1) {
#ifdef HAVE_GETOPT_LONG
    i = getopt_long(argc, argv, optstring, long_options, NULL);
#else
    i = getopt (argc, argv, optstring);
#endif
    if (i == -1) {
      break;
    }
    if (i == ':') {
      usage (progname);
      goto exit_err;
    }

    switch (i) {
    case 'M': /* max-errors */
      max_errors = atoi (optarg);
      break;
    case 'p': /* prefix */
      opt_prefix = 1;
      break;
    case 's': /* suffix */
      opt_suffix = 1;
      break;
    case 'P': /* partial */
      opt_prefix = 1;
      opt_suffix = 1;
      break;
    case 'h': /* help */
      show_help = 1;
      break;
    case 'v': /* version */
      show_version = 1;
      break;
    case 'E': /* list errors */
      show_help = 2;
      break;
    default:
      break;
    }
  }

  if (show_version) {
    printf ("%s version " VERSION "\n", progname);
  }

  if (show_help == 1) {
    usage (progname);
  } else if (show_help == 2) {
    list_errors ();
  }

  if (show_version || show_help) {
    goto exit_out;
  }

  if (max_errors < 0) {
    printf ("%s: Error: [-M num, --max-errors num] option must be non-negative\n", progname);
    goto exit_err;
  }

  if (optind >= argc) {
    usage (progname);
    goto exit_err;
  }

  if (argc-i > 2) multifile = 1;

  for (i = optind; i < argc; i++) {
    filename = argv[i];
    prefix = opt_prefix;
    suffix = opt_suffix;
    if (validate (filename) == -1)
      exit_status = 1;
  }

 exit_out:
  exit (exit_status);

 exit_err:
  exit (1);
}
