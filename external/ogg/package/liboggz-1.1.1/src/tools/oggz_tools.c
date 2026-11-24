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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "oggz/oggz.h"

#include "dirac.h"
#include "oggz_tools_dirac.h"

#if defined (WIN32) || defined (__EMX__)
#include <fcntl.h>
#include <io.h>
#define snprintf _snprintf
#endif

#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#else
#  define PRId64 "I64d"
#endif


static  ogg_uint32_t
_le_32 (ogg_uint32_t i)
{
   ogg_uint32_t ret=i;
#ifdef WORDS_BIGENDIAN
   ret =  (i>>24);
   ret += (i>>8) & 0x0000ff00;
   ret += (i<<8) & 0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

static  ogg_uint16_t
_be_16 (ogg_uint16_t s)
{
  unsigned short ret=s;
#ifndef WORDS_BIGENDIAN
  ret = (s>>8) & 0x00ffU;
  ret += (s<<8) & 0xff00U;
#endif
  return ret;
}

static  ogg_uint32_t
_be_32 (ogg_uint32_t i)
{
   ogg_uint32_t ret=i;
#ifndef WORDS_BIGENDIAN
   ret =  (i>>24);
   ret += (i>>8) & 0x0000ff00;
   ret += (i<<8) & 0x00ff0000;
   ret += (i<<24);
#endif
   return ret;
}

static  ogg_int64_t
_le_64 (ogg_int64_t l)
{
  ogg_int64_t ret=l;
  unsigned char *ucptr = (unsigned char *)&ret;
#ifdef WORDS_BIGENDIAN
  unsigned char temp;

  temp = ucptr [0] ;
  ucptr [0] = ucptr [7] ;
  ucptr [7] = temp ;

  temp = ucptr [1] ;
  ucptr [1] = ucptr [6] ;
  ucptr [6] = temp ;

  temp = ucptr [2] ;
  ucptr [2] = ucptr [5] ;
  ucptr [5] = temp ;

  temp = ucptr [3] ;
  ucptr [3] = ucptr [4] ;
  ucptr [4] = temp ;

#endif
  return (*(ogg_int64_t *)ucptr);
}

#define INT32_LE_AT(x) _le_32((*(ogg_int32_t *)(x)))
#define INT16_BE_AT(x) _be_16((*(ogg_uint16_t *)(x)))
#define INT32_BE_AT(x) _be_32((*(ogg_int32_t *)(x)))
#define INT64_LE_AT(x) _le_64((*(ogg_int64_t *)(x)))

typedef char * (* OTCodecInfoFunc) (unsigned char * data, long n);

static char *
ot_theora_info (unsigned char * data, long len)
{
  char * buf;
  int width, height;

  if (len < 41) return NULL;

  buf = malloc (128);

  width = INT16_BE_AT(&data[15]);
  height = INT16_BE_AT(&data[18]);

  snprintf (buf, 128,
            "\tTheora-Version: %d.%d.%d\n"
	    "\tVideo-Framerate: %.3f fps\n"
	    "\tVideo-Width: %d\n\tVideo-Height: %d\n",
            data[7], data[8], data[9],
	    (double)INT32_BE_AT(&data[22])/ (double)INT32_BE_AT(&data[26]),
	    width, height);

  return buf;
}

static char *
ot_vorbis_info (unsigned char * data, long len)
{
  char * buf;

  if (len < 30) return NULL;

  buf = malloc (60);

  snprintf (buf, 60,
	    "\tAudio-Samplerate: %d Hz\n\tAudio-Channels: %d\n",
	    INT32_LE_AT(&data[12]), (int)(data[11]));

  return buf;
}

static char *
ot_speex_info (unsigned char * data, long len)
{
  char * buf;

  if (len < 68) return NULL;

  buf = malloc (60);

  snprintf (buf, 60,
	    "\tAudio-Samplerate: %d Hz\n\tAudio-Channels: %d\n",
	    INT32_LE_AT(&data[36]), INT32_LE_AT(&data[48]));

  return buf;
}

static char *
ot_celt_info (unsigned char * data, long len)
{
  char * buf;

  if (len < 56) return NULL;

  buf = malloc (60);

  snprintf (buf, 60,
	    "\tAudio-Samplerate: %d Hz\n\tAudio-Channels: %d\n",
	    INT32_LE_AT(&data[40]), INT32_LE_AT(&data[44]));

  return buf;
}

static char *
ot_flac_info (unsigned char * data, long len)
{
  char * buf;
  int n;
  int version_major, version_minor;
  int samplerate;
  int channels;

  if (len < 30) return NULL;

  buf = malloc (120);

  version_major = data[5];
  version_minor = data[6];

  samplerate = (ogg_int64_t) (data[27] << 12) | (data[28] << 4) |
               ((data[29] >> 4)&0xf);
  channels = 1 + ((data[29] >> 1)&0x7);

  n = snprintf (buf, 120,
	    "\tAudio-Samplerate: %d Hz\n\tAudio-Channels: %d\n",
            samplerate, channels);

  snprintf (buf+n, 120-n,
            "\tFLAC-Ogg-Mapping-Version: %d.%d\n",
            version_major, version_minor);

  return buf;
}

static char *
ot_oggpcm2_info (unsigned char * data, long len)
{
  char * buf;

  if (len < 28) return NULL;

  buf = malloc (60);

  snprintf (buf, 60,
	    "\tAudio-Samplerate: %d Hz\n\tAudio-Channels: %d\n",
	    INT32_BE_AT(&data[16]), (int)data[21]);

  return buf;
}

static char *
ot_kate_info (unsigned char * data, long len)
{
  char * buf;

  static const size_t KATE_INFO_BUFFER_LEN =
    1 /* tab */
  +18 /* "Content-Language: " */
  +15 /* 15 chars + NUL for language */
   +1 /* \n */
   +1 /* tab */
  +18 /* "Content-Category: " */
  +15 /* 15 chars + NUL for category */
   +1 /* \n */
   +1;/* terminating NUL */

  if (len < 64) return NULL;

  buf = malloc (KATE_INFO_BUFFER_LEN);

  /* Are these headers coming from some standard ? If so, need to find what should these be for Kate */
  snprintf (buf, KATE_INFO_BUFFER_LEN,
	    "\tContent-Language: %s\n"
            "\tContent-Category: %s\n",
	    &data[32], &data[48]);

#undef KATE_INFO_BUFFER_LEN

  return buf;
}

static char *
ot_dirac_info (unsigned char * data, long len)
{
  char * buf;
  dirac_info *info;

  /* read in useful bits from sequence header */
  if (len < 24) return NULL;

  buf = malloc (80);
  info = malloc(sizeof(dirac_info));

  if (dirac_parse_info(info, data, len) == -1) {
    free (info);
    free (buf);
    return NULL;
  }

  snprintf (buf, 80,
	    "\tVideo-Framerate: %.3f fps\n"
	    "\tVideo-Width: %d\n\tVideo-Height: %d\n",
	    (double)info->fps_numerator/ (double)info->fps_denominator,
	    info->width, info->height);

  free(info);

  return buf;
}


static char *
ot_skeleton_info (unsigned char * data, long len)
{
  char * buf;
  double pres_n, pres_d, pres;
  double base_n, base_d, base;

  if (len < 64L) return NULL;

  buf = malloc (60);

  pres_n = (double)INT64_LE_AT(&data[12]);
  pres_d = (double)INT64_LE_AT(&data[20]);
  if (pres_d != 0.0) {
    pres = pres_n / pres_d;
  } else {
    pres = 0.0;
  }

  base_n = (double)INT64_LE_AT(&data[28]);
  base_d = (double)INT64_LE_AT(&data[36]);
  if (base_d != 0.0) {
    base = base_n / base_d;
  } else {
    base = 0.0;
  }

  snprintf (buf, 60,
	    "\tPresentation-Time: %.3f\n\tBasetime: %.3f\n", pres, base);

  return buf;
}

static const OTCodecInfoFunc codec_ident[] = {
  ot_theora_info,
  ot_vorbis_info,
  ot_speex_info,
  ot_oggpcm2_info,
  NULL,             /* CMML */
  NULL,             /* ANNODEX */
  ot_skeleton_info,
  NULL,             /* FLAC0 */
  ot_flac_info,     /* FLAC */
  NULL,             /* ANXDATA */
  ot_celt_info,     /* CELT */
  ot_kate_info,     /* KATE */
  ot_dirac_info,    /* BBCD */
  NULL              /* UNKNOWN */
};

const char *
ot_page_identify (OGGZ *oggz, const ogg_page * og, char ** info)
{
  const char * ret = NULL;
  int serial_no;
  int content;

  /*
   * identify stream content using oggz_stream_get_content, identify
   * stream content name using oggz_stream_get_content_type
   */

  serial_no = ogg_page_serialno((ogg_page *)og);

  content = oggz_stream_get_content(oggz, serial_no);
  if (content == OGGZ_ERR_BAD_SERIALNO) return NULL;

  ret = oggz_stream_get_content_type(oggz, serial_no);

  if (info != NULL)
  {
    if (codec_ident[content] != NULL)
    {
      *info = codec_ident[content](og->body, og->body_len);
    }
  }

  return ret;
}

/*
 * Print a number of bytes to 3 significant figures
 * using standard abbreviations (GB, MB, kB, byte[s])
 */
int
ot_fprint_bytes (FILE * stream, long nr_bytes)
{
  if (nr_bytes > (1L<<30)) {
    return fprintf (stream, "%0.3f GB",
		   (double)nr_bytes / (1024.0 * 1024.0 * 1024.0));
  } else if (nr_bytes > (1L<<20)) {
    return fprintf (stream, "%0.3f MB",
		   (double)nr_bytes / (1024.0 * 1024.0));
  } else if (nr_bytes > (1L<<10)) {
    return fprintf (stream, "%0.3f kB",
		   (double)nr_bytes / (1024.0));
  } else if (nr_bytes == 1) {
    return fprintf (stream, "1 byte");
  } else {
    return fprintf (stream, "%ld bytes", nr_bytes);
  }
}

/*
 * Print a bitrate to 3 significant figures
 * using quasi-standard abbreviations (Gbps, Mbps, kbps, bps)
 */
int
ot_print_bitrate (long bps)
{
  if (bps > (1000000000L)) {
    return printf ("%0.3f Gbps",
		   (double)bps / (1000.0 * 1000.0 * 1000.0));
  } else if (bps > (1000000L)) {
    return printf ("%0.3f Mbps",
		   (double)bps / (1000.0 * 1000.0));
  } else if (bps > (1000L)) {
    return printf ("%0.3f kbps",
		   (double)bps / (1000.0));
  } else {
    return printf ("%ld bps", bps);
  }
}

int
ot_fprint_time (FILE * stream, double seconds)
{
  int hrs, min;
  double sec;
  char * sign;

  sign = (seconds < 0.0) ? "-" : "";

  if (seconds < 0.0) seconds = -seconds;

  hrs = (int) (seconds/3600.0);
  min = (int) ((seconds - ((double)hrs * 3600.0)) / 60.0);
  sec = seconds - ((double)hrs * 3600.0)- ((double)min * 60.0);

  return fprintf (stream, "%s%02d:%02d:%06.3f", sign, hrs, min, sec);
}

void
ot_dirac_gpos_parse (ogg_int64_t iframe, ogg_int64_t pframe,
                     struct ot_dirac_gpos * dg)
{
  dg->pt = (iframe + pframe) >> 9;
  dg->dist = ((iframe & 0xff) << 8) | (pframe & 0xff);
  dg->delay = pframe >> 9;
  dg->dt = (ogg_int64_t)dg->pt - dg->delay;
}

int
ot_fprint_granulepos (FILE * stream, OGGZ * oggz, long serialno,
                      ogg_int64_t granulepos)
{
  int ret, granuleshift = oggz_get_granuleshift (oggz, serialno);

  if (granuleshift < 1) {
    ret = fprintf (stream, "%" PRId64, granulepos);
  } else {
    ogg_int64_t iframe, pframe;
    iframe = granulepos >> granuleshift;
    pframe = granulepos - (iframe << granuleshift);

    if (oggz_stream_get_content (oggz, serialno) != OGGZ_CONTENT_DIRAC) {
      ret = fprintf (stream, "%" PRId64 "|%" PRId64, iframe, pframe);
    } else {
      struct ot_dirac_gpos dg;
      ot_dirac_gpos_parse (iframe, pframe, &dg);
      ret = fprintf (stream,
		     "(pt:%u,dt:%" PRId64 ",dist:%hu,delay:%hu)",
		     dg.pt, dg.dt, dg.dist, dg.delay);
    }

}

  return ret;
}

void
ot_init (void)
{
#ifdef _WIN32
  /* We need to set stdin/stdout to binary mode on Win32 */

  _setmode( _fileno( stdin ), _O_BINARY );
  _setmode( _fileno( stdout ), _O_BINARY );
#endif
#ifdef __EMX__
  /* We need to set stdin/stdout to binary mode on OS/2*/

  setmode( fileno( stdin ), O_BINARY );
  setmode( fileno( stdout ), O_BINARY );
#endif
}

void
ot_print_short_options (char * optstring)
{
  char *c;

  for (c=optstring; *c; c++) {
    if (*c != ':') printf ("-%c ", *c);
  }

  printf ("\n");
}

#ifdef HAVE_GETOPT_LONG
void
ot_print_options (struct option long_options[], char * optstring)
{
  int i;
  for (i=0; long_options[i].name != NULL; i++)  {
    printf ("--%s ", long_options[i].name);
  }

  ot_print_short_options (optstring);
}
#endif
