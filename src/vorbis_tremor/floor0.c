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

 function: floor backend 0 implementation

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "ivorbiscodec.h"
#include "codec_internal.h"
#include "codebook.h"
#include "misc.h"
#include "os.h"

#define LSP_FRACBITS 14
extern const ogg_int32_t FLOOR_fromdB_LOOKUP[];

/*************** LSP decode ********************/

#include "lsp_lookup.h"

/* interpolated 1./sqrt(p) where .5 <= a < 1. (.100000... to .111111...) in
   16.16 format 
   returns in m.8 format */

static long ADJUST_SQRT2[2]={8192,5792};
static inline ogg_int32_t vorbis_invsqlook_i(long a,long e){
  long i=(a&0x7fff)>>(INVSQ_LOOKUP_I_SHIFT-1); 
  long d=a&INVSQ_LOOKUP_I_MASK;                              /*  0.10 */
  long val=INVSQ_LOOKUP_I[i]-                                /*  1.16 */
    ((INVSQ_LOOKUP_IDel[i]*d)>>INVSQ_LOOKUP_I_SHIFT);        /* result 1.16 */
  val*=ADJUST_SQRT2[e&1];
  e=(e>>1)+21;
  return(val>>e);
}

/* interpolated lookup based fromdB function, domain -140dB to 0dB only */
/* a is in n.12 format */
#ifdef _LOW_ACCURACY_
static inline ogg_int32_t vorbis_fromdBlook_i(long a){
  if(a>0) return 0x7fffffff;
  if(a<(-140<<12)) return 0;
  return FLOOR_fromdB_LOOKUP[((a+140)*467)>>20]<<9;
}
#else
static inline ogg_int32_t vorbis_fromdBlook_i(long a){
  if(a>0) return 0x7fffffff;
  if(a<(-140<<12)) return 0;
  return FLOOR_fromdB_LOOKUP[((a+(140<<12))*467)>>20];
}
#endif

/* interpolated lookup based cos function, domain 0 to PI only */
/* a is in 0.16 format, where 0==0, 2^^16-1==PI, return 0.14 */
static inline ogg_int32_t vorbis_coslook_i(long a){
  int i=a>>COS_LOOKUP_I_SHIFT;
  int d=a&COS_LOOKUP_I_MASK;
  return COS_LOOKUP_I[i]- ((d*(COS_LOOKUP_I[i]-COS_LOOKUP_I[i+1]))>>
			   COS_LOOKUP_I_SHIFT);
}

/* interpolated half-wave lookup based cos function */
/* a is in 0.16 format, where 0==0, 2^^16==PI, return .LSP_FRACBITS */
static inline ogg_int32_t vorbis_coslook2_i(long a){
  int i=a>>COS_LOOKUP_I_SHIFT;
  int d=a&COS_LOOKUP_I_MASK;
  return ((COS_LOOKUP_I[i]<<COS_LOOKUP_I_SHIFT)-
	  d*(COS_LOOKUP_I[i]-COS_LOOKUP_I[i+1]))>>
    (COS_LOOKUP_I_SHIFT-LSP_FRACBITS+14);
}

static const ogg_uint16_t barklook[54]={
  0,51,102,154,            206,258,311,365,
  420,477,535,594,         656,719,785,854,
  926,1002,1082,1166,      1256,1352,1454,1564,
  1683,1812,1953,2107,     2276,2463,2670,2900,
  3155,3440,3756,4106,     4493,4919,5387,5901,
  6466,7094,7798,8599,     9528,10623,11935,13524,
  15453,17775,20517,23667, 27183,31004
};

/* used in init only; interpolate the long way */
static inline ogg_int32_t toBARK(int n){
  int i;
  for(i=0;i<54;i++) 
    if(n>=barklook[i] && n<barklook[i+1])break;
  
  if(i==54){
    return 54<<14;
  }else{
    return (i<<14)+(((n-barklook[i])*  
		     ((1UL<<31)/(barklook[i+1]-barklook[i])))>>17);
  }
}

static const unsigned char MLOOP_1[64]={
   0,10,11,11, 12,12,12,12, 13,13,13,13, 13,13,13,13,
  14,14,14,14, 14,14,14,14, 14,14,14,14, 14,14,14,14,
  15,15,15,15, 15,15,15,15, 15,15,15,15, 15,15,15,15,
  15,15,15,15, 15,15,15,15, 15,15,15,15, 15,15,15,15,
};

static const unsigned char MLOOP_2[64]={
  0,4,5,5, 6,6,6,6, 7,7,7,7, 7,7,7,7,
  8,8,8,8, 8,8,8,8, 8,8,8,8, 8,8,8,8,
  9,9,9,9, 9,9,9,9, 9,9,9,9, 9,9,9,9,
  9,9,9,9, 9,9,9,9, 9,9,9,9, 9,9,9,9,
};

static const unsigned char MLOOP_3[8]={0,1,2,2,3,3,3,3};

void vorbis_lsp_to_curve(ogg_int32_t *curve,int n,int ln,
			 ogg_int32_t *lsp,int m,
			 ogg_int32_t amp,
			 ogg_int32_t ampoffset,
			 ogg_int32_t nyq){

  /* 0 <= m < 256 */

  /* set up for using all int later */
  int i;
  int ampoffseti=ampoffset*4096;
  int ampi=amp;
  ogg_int32_t *ilsp=(ogg_int32_t *)alloca(m*sizeof(*ilsp));

  ogg_uint32_t inyq= (1UL<<31) / toBARK(nyq);
  ogg_uint32_t imap= (1UL<<31) / ln;
  ogg_uint32_t tBnyq1 = toBARK(nyq)<<1;

  /* Besenham for frequency scale to avoid a division */
  int f=0;
  int fdx=n;
  int fbase=nyq/fdx;
  int ferr=0;
  int fdy=nyq-fbase*fdx;
  int map=0;

#ifdef _LOW_ACCURACY_
  ogg_uint32_t nextbark=((tBnyq1<<11)/ln)>>12;
#else
  ogg_uint32_t nextbark=MULT31(imap>>1,tBnyq1);
#endif
  int nextf=barklook[nextbark>>14]+(((nextbark&0x3fff)*
	    (barklook[(nextbark>>14)+1]-barklook[nextbark>>14]))>>14);

  /* lsp is in 8.24, range 0 to PI; coslook wants it in .16 0 to 1*/
  for(i=0;i<m;i++){
#ifndef _LOW_ACCURACY_
    ogg_int32_t val=MULT32(lsp[i],0x517cc2);
#else
    ogg_int32_t val=((lsp[i]>>10)*0x517d)>>14;
#endif

    /* safeguard against a malicious stream */
    if(val<0 || (val>>COS_LOOKUP_I_SHIFT)>=COS_LOOKUP_I_SZ){
      memset(curve,0,sizeof(*curve)*n);
      return;
    }

    ilsp[i]=vorbis_coslook_i(val);
  }

  i=0;
  while(i<n){
    int j;
    ogg_uint32_t pi=46341; /* 2**-.5 in 0.16 */
    ogg_uint32_t qi=46341;
    ogg_int32_t qexp=0,shift;
    ogg_int32_t wi;

    wi=vorbis_coslook2_i((map*imap)>>15);


#ifdef _V_LSP_MATH_ASM
    lsp_loop_asm(&qi,&pi,&qexp,ilsp,wi,m);

    pi=((pi*pi)>>16);
    qi=((qi*qi)>>16);
    
    if(m&1){
      qexp= qexp*2-28*((m+1)>>1)+m;	     
      pi*=(1<<14)-((wi*wi)>>14);
      qi+=pi>>14;     
    }else{
      qexp= qexp*2-13*m;
      
      pi*=(1<<14)-wi;
      qi*=(1<<14)+wi;
      
      qi=(qi+pi)>>14;
    }
    
    if(qi&0xffff0000){ /* checks for 1.xxxxxxxxxxxxxxxx */
      qi>>=1; qexp++; 
    }else
      lsp_norm_asm(&qi,&qexp);

#else

    qi*=labs(ilsp[0]-wi);
    pi*=labs(ilsp[1]-wi);

    for(j=3;j<m;j+=2){
      if(!(shift=MLOOP_1[(pi|qi)>>25]))
      	if(!(shift=MLOOP_2[(pi|qi)>>19]))
      	  shift=MLOOP_3[(pi|qi)>>16];
      
      qi=(qi>>shift)*labs(ilsp[j-1]-wi);
      pi=(pi>>shift)*labs(ilsp[j]-wi);
      qexp+=shift;
    }
    if(!(shift=MLOOP_1[(pi|qi)>>25]))
      if(!(shift=MLOOP_2[(pi|qi)>>19]))
	shift=MLOOP_3[(pi|qi)>>16];
    
    /* pi,qi normalized collectively, both tracked using qexp */

    if(m&1){
      /* odd order filter; slightly assymetric */
      /* the last coefficient */
      qi=(qi>>shift)*labs(ilsp[j-1]-wi);
      pi=(pi>>shift)<<14;
      qexp+=shift;

      if(!(shift=MLOOP_1[(pi|qi)>>25]))
	if(!(shift=MLOOP_2[(pi|qi)>>19]))
	  shift=MLOOP_3[(pi|qi)>>16];
      
      pi>>=shift;
      qi>>=shift;
      qexp+=shift-14*((m+1)>>1);

      pi=((pi*pi)>>16);
      qi=((qi*qi)>>16);
      qexp=qexp*2+m;

      pi*=(1<<14)-((wi*wi)>>14);
      qi+=pi>>14;

    }else{
      /* even order filter; still symmetric */

      /* p*=p(1-w), q*=q(1+w), let normalization drift because it isn't
	 worth tracking step by step */
      
      pi>>=shift;
      qi>>=shift;
      qexp+=shift-7*m;

      pi=((pi*pi)>>16);
      qi=((qi*qi)>>16);
      qexp=qexp*2+m;
      
      pi*=(1<<14)-wi;
      qi*=(1<<14)+wi;
      qi=(qi+pi)>>14;
      
    }
    

    /* we've let the normalization drift because it wasn't important;
       however, for the lookup, things must be normalized again.  We
       need at most one right shift or a number of left shifts */

    if(qi&0xffff0000){ /* checks for 1.xxxxxxxxxxxxxxxx */
      qi>>=1; qexp++; 
    }else
      while(qi && !(qi&0x8000)){ /* checks for 0.0xxxxxxxxxxxxxxx or less*/
	qi<<=1; qexp--; 
      }

#endif

    amp=vorbis_fromdBlook_i(ampi*                     /*  n.4         */
			    vorbis_invsqlook_i(qi,qexp)- 
			                              /*  m.8, m+n<=8 */
			    ampoffseti);              /*  8.12[0]     */
    
#ifdef _LOW_ACCURACY_
    amp>>=9;
#endif
    curve[i]= MULT31_SHIFT15(curve[i],amp);

    while(++i<n){
	
      /* line plot to get new f */
      ferr+=fdy;
      if(ferr>=fdx){
	ferr-=fdx;
	f++;
      }
      f+=fbase;
      
      if(f>=nextf)break;

      curve[i]= MULT31_SHIFT15(curve[i],amp);
    }

    while(1){
      map++;

      if(map+1<ln){
	
#ifdef _LOW_ACCURACY_
	nextbark=((tBnyq1<<11)/ln*(map+1))>>12;
#else
	nextbark=MULT31((map+1)*(imap>>1),tBnyq1);
#endif
	nextf=barklook[nextbark>>14]+
	  (((nextbark&0x3fff)*
	    (barklook[(nextbark>>14)+1]-barklook[nextbark>>14]))>>14);
	if(f<=nextf)break;
	
      }else{
	nextf=9999999;
	break;
      }
    }
    if(map>=ln){
      map=ln-1; /* guard against the approximation */      
      nextf=9999999;
    }
  }
}

/*************** vorbis decode glue ************/

void floor0_free_info(vorbis_info_floor *i){
  vorbis_info_floor0 *info=(vorbis_info_floor0 *)i;
  if(info)_ogg_free(info);
}

vorbis_info_floor *floor0_info_unpack (vorbis_info *vi,oggpack_buffer *opb){
  codec_setup_info     *ci=(codec_setup_info *)vi->codec_setup;
  int j;

  vorbis_info_floor0 *info=(vorbis_info_floor0 *)_ogg_malloc(sizeof(*info));
  info->order=oggpack_read(opb,8);
  info->rate=oggpack_read(opb,16);
  info->barkmap=oggpack_read(opb,16);
  info->ampbits=oggpack_read(opb,6);
  info->ampdB=oggpack_read(opb,8);
  info->numbooks=oggpack_read(opb,4)+1;
  
  if(info->order<1)goto err_out;
  if(info->rate<1)goto err_out;
  if(info->barkmap<1)goto err_out;
    
  for(j=0;j<info->numbooks;j++){
    info->books[j]=oggpack_read(opb,8);
    if(info->books[j]>=ci->books)goto err_out;
  }

  if(oggpack_eop(opb))goto err_out;
  return(info);

 err_out:
  floor0_free_info(info);
  return(NULL);
}

int floor0_memosize(vorbis_info_floor *i){
  vorbis_info_floor0 *info=(vorbis_info_floor0 *)i;
  return info->order+1;
}

ogg_int32_t *floor0_inverse1(vorbis_dsp_state *vd,vorbis_info_floor *i,
			     ogg_int32_t *lsp){
  vorbis_info_floor0 *info=(vorbis_info_floor0 *)i;
  int j,k;
  
  int ampraw=oggpack_read(&vd->opb,info->ampbits);
  if(ampraw>0){ /* also handles the -1 out of data case */
    long maxval=(1<<info->ampbits)-1;
    int amp=((ampraw*info->ampdB)<<4)/maxval;
    int booknum=oggpack_read(&vd->opb,_ilog(info->numbooks));
    
    if(booknum!=-1 && booknum<info->numbooks){ /* be paranoid */
      codec_setup_info  *ci=(codec_setup_info *)vd->vi->codec_setup;
      codebook *b=ci->book_param+info->books[booknum];
      ogg_int32_t last=0;
            
      for(j=0;j<info->order;j+=b->dim)
	if(vorbis_book_decodev_set(b,lsp+j,&vd->opb,b->dim,-24)==-1)goto eop;
      for(j=0;j<info->order;){
	for(k=0;k<b->dim;k++,j++)lsp[j]+=last;
	last=lsp[j-1];
      }
      
      lsp[info->order]=amp;
      return(lsp);
    }
  }
 eop:
  return(NULL);
}

int floor0_inverse2(vorbis_dsp_state *vd,vorbis_info_floor *i,
			   ogg_int32_t *lsp,ogg_int32_t *out){
  vorbis_info_floor0 *info=(vorbis_info_floor0 *)i;
  codec_setup_info  *ci=(codec_setup_info *)vd->vi->codec_setup;
  
  if(lsp){
    ogg_int32_t amp=lsp[info->order];

    /* take the coefficients back to a spectral envelope curve */
    vorbis_lsp_to_curve(out,ci->blocksizes[vd->W]/2,info->barkmap,
			lsp,info->order,amp,info->ampdB,
			info->rate>>1);
    return(1);
  }
  memset(out,0,sizeof(*out)*ci->blocksizes[vd->W]/2);
  return(0);
}

