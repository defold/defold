/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis 'TREMOR' CODEC SOURCE CODE.   *
 *                                                                  *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis 'TREMOR' SOURCE CODE IS (C) COPYRIGHT 1994-2003    *
 * BY THE Xiph.Org FOUNDATION http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

 function: maintain the info structure, info <-> header packets

 ********************************************************************/

/* general handling of the header and the vorbis_info structure (and
   substructures) */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ogg.h"
#include "ivorbiscodec.h"
#include "codec_internal.h"
#include "codebook.h"
#include "misc.h"
#include "os.h"

/* helpers */
static void _v_readstring(oggpack_buffer *o,char *buf,int bytes){
  while(bytes--){
    *buf++=oggpack_read(o,8);
  }
}

void vorbis_comment_init(vorbis_comment *vc){
  memset(vc,0,sizeof(*vc));
}

/* This is more or less the same as strncasecmp - but that doesn't exist
 * everywhere, and this is a fairly trivial function, so we include it */
static int tagcompare(const char *s1, const char *s2, int n){
  int c=0;
  while(c < n){
    if(toupper(s1[c]) != toupper(s2[c]))
      return !0;
    c++;
  }
  return 0;
}

char *vorbis_comment_query(vorbis_comment *vc, char *tag, int count){
  long i;
  int found = 0;
  int taglen = strlen(tag)+1; /* +1 for the = we append */
  char *fulltag = (char *)alloca(taglen+ 1);

  strcpy(fulltag, tag);
  strcat(fulltag, "=");
  
  for(i=0;i<vc->comments;i++){
    if(!tagcompare(vc->user_comments[i], fulltag, taglen)){
      if(count == found)
	/* We return a pointer to the data, not a copy */
      	return vc->user_comments[i] + taglen;
      else
	found++;
    }
  }
  return NULL; /* didn't find anything */
}

int vorbis_comment_query_count(vorbis_comment *vc, char *tag){
  int i,count=0;
  int taglen = strlen(tag)+1; /* +1 for the = we append */
  char *fulltag = (char *)alloca(taglen+1);
  strcpy(fulltag,tag);
  strcat(fulltag, "=");

  for(i=0;i<vc->comments;i++){
    if(!tagcompare(vc->user_comments[i], fulltag, taglen))
      count++;
  }

  return count;
}

void vorbis_comment_clear(vorbis_comment *vc){
  if(vc){
    long i;
    for(i=0;i<vc->comments;i++)
      if(vc->user_comments[i])_ogg_free(vc->user_comments[i]);
    if(vc->user_comments)_ogg_free(vc->user_comments);
	if(vc->comment_lengths)_ogg_free(vc->comment_lengths);
    if(vc->vendor)_ogg_free(vc->vendor);
  }
  memset(vc,0,sizeof(*vc));
}

/* blocksize 0 is guaranteed to be short, 1 is guarantted to be long.
   They may be equal, but short will never ge greater than long */
int vorbis_info_blocksize(vorbis_info *vi,int zo){
  codec_setup_info *ci = (codec_setup_info *)vi->codec_setup;
  return ci ? ci->blocksizes[zo] : -1;
}

/* used by synthesis, which has a full, alloced vi */
void vorbis_info_init(vorbis_info *vi){
  memset(vi,0,sizeof(*vi));
  vi->codec_setup=(codec_setup_info *)_ogg_calloc(1,sizeof(codec_setup_info));
}

void vorbis_info_clear(vorbis_info *vi){
  codec_setup_info     *ci=(codec_setup_info *)vi->codec_setup;
  int i;

  if(ci){

    if(ci->mode_param)_ogg_free(ci->mode_param);

    if(ci->map_param){
      for(i=0;i<ci->maps;i++) /* unpack does the range checking */
	mapping_clear_info(ci->map_param+i);
      _ogg_free(ci->map_param);
    }

    if(ci->floor_param){
      for(i=0;i<ci->floors;i++) /* unpack does the range checking */
	if(ci->floor_type[i])
	  floor1_free_info(ci->floor_param[i]);
	else
	  floor0_free_info(ci->floor_param[i]);
      _ogg_free(ci->floor_param);
      _ogg_free(ci->floor_type);
    }

    if(ci->residue_param){
      for(i=0;i<ci->residues;i++) /* unpack does the range checking */
	res_clear_info(ci->residue_param+i);
      _ogg_free(ci->residue_param);
    }

    if(ci->book_param){
      for(i=0;i<ci->books;i++)
	vorbis_book_clear(ci->book_param+i);
      _ogg_free(ci->book_param);
    }
    
    _ogg_free(ci);
  }

  memset(vi,0,sizeof(*vi));
}

/* Header packing/unpacking ********************************************/

static int _vorbis_unpack_info(vorbis_info *vi,oggpack_buffer *opb){
  codec_setup_info     *ci=(codec_setup_info *)vi->codec_setup;
  if(!ci)return(OV_EFAULT);

  vi->version=oggpack_read(opb,32);
  if(vi->version!=0)return(OV_EVERSION);

  vi->channels=oggpack_read(opb,8);
  vi->rate=oggpack_read(opb,32);

  vi->bitrate_upper=oggpack_read(opb,32);
  vi->bitrate_nominal=oggpack_read(opb,32);
  vi->bitrate_lower=oggpack_read(opb,32);

  ci->blocksizes[0]=1<<oggpack_read(opb,4);
  ci->blocksizes[1]=1<<oggpack_read(opb,4);
  
#ifdef LIMIT_TO_64kHz
  if(vi->rate>=64000 || ci->blocksizes[1]>4096)goto err_out;
#else
  if(vi->rate<64000 && ci->blocksizes[1]>4096)goto err_out;
#endif

  if(vi->rate<1)goto err_out;
  if(vi->channels<1)goto err_out;
  if(ci->blocksizes[0]<64)goto err_out; 
  if(ci->blocksizes[1]<ci->blocksizes[0])goto err_out;
  if(ci->blocksizes[1]>8192)goto err_out;
  
  if(oggpack_read(opb,1)!=1)goto err_out; /* EOP check */

  return(0);
 err_out:
  vorbis_info_clear(vi);
  return(OV_EBADHEADER);
}

static int _vorbis_unpack_comment(vorbis_comment *vc,oggpack_buffer *opb){
  int i;
  int vendorlen=oggpack_read(opb,32);
  if(vendorlen<0)goto err_out;
  vc->vendor=(char *)_ogg_calloc(vendorlen+1,1);
  _v_readstring(opb,vc->vendor,vendorlen);
  vc->comments=oggpack_read(opb,32);
  if(vc->comments<0)goto err_out;
  vc->user_comments=(char **)_ogg_calloc(vc->comments+1,sizeof(*vc->user_comments));
  vc->comment_lengths=(int *)_ogg_calloc(vc->comments+1, sizeof(*vc->comment_lengths));
	    
  for(i=0;i<vc->comments;i++){
    int len=oggpack_read(opb,32);
    if(len<0)goto err_out;
	vc->comment_lengths[i]=len;
    vc->user_comments[i]=(char *)_ogg_calloc(len+1,1);
    _v_readstring(opb,vc->user_comments[i],len);
  }	  
  if(oggpack_read(opb,1)!=1)goto err_out; /* EOP check */

  return(0);
 err_out:
  vorbis_comment_clear(vc);
  return(OV_EBADHEADER);
}

/* all of the real encoding details are here.  The modes, books,
   everything */
static int _vorbis_unpack_books(vorbis_info *vi,oggpack_buffer *opb){
  codec_setup_info     *ci=(codec_setup_info *)vi->codec_setup;
  int i;
  if(!ci)return(OV_EFAULT);

  /* codebooks */
  ci->books=oggpack_read(opb,8)+1;
  ci->book_param=(codebook *)_ogg_calloc(ci->books,sizeof(*ci->book_param));
  for(i=0;i<ci->books;i++)
    if(vorbis_book_unpack(opb,ci->book_param+i))goto err_out;

  /* time backend settings, not actually used */
  i=oggpack_read(opb,6);
  for(;i>=0;i--)
    if(oggpack_read(opb,16)!=0)goto err_out;

  /* floor backend settings */
  ci->floors=oggpack_read(opb,6)+1;
  ci->floor_param=_ogg_malloc(sizeof(*ci->floor_param)*ci->floors);
  ci->floor_type=_ogg_malloc(sizeof(*ci->floor_type)*ci->floors);
  for(i=0;i<ci->floors;i++){
    ci->floor_type[i]=oggpack_read(opb,16);
    if(ci->floor_type[i]<0 || ci->floor_type[i]>=VI_FLOORB)goto err_out;
    if(ci->floor_type[i])
      ci->floor_param[i]=floor1_info_unpack(vi,opb);
    else
      ci->floor_param[i]=floor0_info_unpack(vi,opb);
    if(!ci->floor_param[i])goto err_out;
  }

  /* residue backend settings */
  ci->residues=oggpack_read(opb,6)+1;
  ci->residue_param=_ogg_malloc(sizeof(*ci->residue_param)*ci->residues);
  for(i=0;i<ci->residues;i++)
    if(res_unpack(ci->residue_param+i,vi,opb))goto err_out;

  /* map backend settings */
  ci->maps=oggpack_read(opb,6)+1;
  ci->map_param=_ogg_malloc(sizeof(*ci->map_param)*ci->maps);
  for(i=0;i<ci->maps;i++){
    if(oggpack_read(opb,16)!=0)goto err_out;
    if(mapping_info_unpack(ci->map_param+i,vi,opb))goto err_out;
  }
  
  /* mode settings */
  ci->modes=oggpack_read(opb,6)+1;
  ci->mode_param=
    (vorbis_info_mode *)_ogg_malloc(ci->modes*sizeof(*ci->mode_param));
  for(i=0;i<ci->modes;i++){
    ci->mode_param[i].blockflag=oggpack_read(opb,1);
    if(oggpack_read(opb,16))goto err_out;
    if(oggpack_read(opb,16))goto err_out;
    ci->mode_param[i].mapping=oggpack_read(opb,8);
    if(ci->mode_param[i].mapping>=ci->maps)goto err_out;
  }
  
  if(oggpack_read(opb,1)!=1)goto err_out; /* top level EOP check */

  return(0);
 err_out:
  vorbis_info_clear(vi);
  return(OV_EBADHEADER);
}

/* The Vorbis header is in three packets; the initial small packet in
   the first page that identifies basic parameters, a second packet
   with bitstream comments and a third packet that holds the
   codebook. */

int vorbis_dsp_headerin(vorbis_info *vi,vorbis_comment *vc,ogg_packet *op){
  oggpack_buffer opb;
  
  if(op){
    oggpack_readinit(&opb,op->packet);

    /* Which of the three types of header is this? */
    /* Also verify header-ness, vorbis */
    {
      char buffer[6];
      int packtype=oggpack_read(&opb,8);
      memset(buffer,0,6);
      _v_readstring(&opb,buffer,6);
      if(memcmp(buffer,"vorbis",6)){
	/* not a vorbis header */
	return(OV_ENOTVORBIS);
      }
      switch(packtype){
      case 0x01: /* least significant *bit* is read first */
	if(!op->b_o_s){
	  /* Not the initial packet */
	  return(OV_EBADHEADER);
	}
	if(vi->rate!=0){
	  /* previously initialized info header */
	  return(OV_EBADHEADER);
	}

	return(_vorbis_unpack_info(vi,&opb));

      case 0x03: /* least significant *bit* is read first */
	if(vi->rate==0){
	  /* um... we didn't get the initial header */
	  return(OV_EBADHEADER);
	}

	return(_vorbis_unpack_comment(vc,&opb));

      case 0x05: /* least significant *bit* is read first */
	if(vi->rate==0 || vc->vendor==NULL){
	  /* um... we didn;t get the initial header or comments yet */
	  return(OV_EBADHEADER);
	}

	return(_vorbis_unpack_books(vi,&opb));

      default:
	/* Not a valid vorbis header type */
	return(OV_EBADHEADER);
	break;
      }
    }
  }
  return(OV_EBADHEADER);
}

