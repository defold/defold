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

 function: normalized modified discrete cosine transform
           power of two length transform only [64 <= n ]
 last mod: $Id: mdct.c,v 1.9.6.5 2003/04/29 04:03:27 xiphmont Exp $

 Original algorithm adapted long ago from _The use of multirate filter
 banks for coding of high quality digital audio_, by T. Sporer,
 K. Brandenburg and B. Edler, collection of the European Signal
 Processing Conference (EUSIPCO), Amsterdam, June 1992, Vol.1, pp
 211-214

 The below code implements an algorithm that no longer looks much like
 that presented in the paper, but the basic structure remains if you
 dig deep enough to see it.

 This module DOES NOT INCLUDE code to generate/apply the window
 function.  Everybody has their own weird favorite including me... I
 happen to like the properties of y=sin(.5PI*sin^2(x)), but others may
 vehemently disagree.

 ********************************************************************/

#include "ivorbiscodec.h"
#include "os.h"
#include "misc.h"
#include "mdct.h"
#include "mdct_lookup.h"

STIN void presymmetry(DATA_TYPE *in,int n2,int step){
  DATA_TYPE *aX;
  DATA_TYPE *bX;
  LOOKUP_T *T;
  int n4=n2>>1;
  
  aX            = in+n2-3;
  T             = sincos_lookup0;

  do{
    REG_TYPE  r0= aX[0];
    REG_TYPE  r2= aX[2];
    XPROD31( r0, r2, T[0], T[1], &aX[0], &aX[2] ); T+=step;
    aX-=4;
  }while(aX>=in+n4);
  do{
    REG_TYPE  r0= aX[0];
    REG_TYPE  r2= aX[2];
    XPROD31( r0, r2, T[1], T[0], &aX[0], &aX[2] ); T-=step;
    aX-=4;
  }while(aX>=in);

  aX            = in+n2-4;
  bX            = in;
  T             = sincos_lookup0;
  do{
    REG_TYPE  ri0= aX[0];
    REG_TYPE  ri2= aX[2];
    REG_TYPE  ro0= bX[0];
    REG_TYPE  ro2= bX[2];
    
    XNPROD31( ro2, ro0, T[1], T[0], &aX[0], &aX[2] ); T+=step;
    XNPROD31( ri2, ri0, T[0], T[1], &bX[0], &bX[2] );
    
    aX-=4;
    bX+=4;
  }while(aX>=in+n4);

}

/* 8 point butterfly (in place) */
STIN void mdct_butterfly_8(DATA_TYPE *x){

  REG_TYPE r0   = x[0] + x[1];
  REG_TYPE r1   = x[0] - x[1];
  REG_TYPE r2   = x[2] + x[3];
  REG_TYPE r3   = x[2] - x[3];
  REG_TYPE r4   = x[4] + x[5];
  REG_TYPE r5   = x[4] - x[5];
  REG_TYPE r6   = x[6] + x[7];
  REG_TYPE r7   = x[6] - x[7];

	   x[0] = r5   + r3;
	   x[1] = r7   - r1;
	   x[2] = r5   - r3;
	   x[3] = r7   + r1;
           x[4] = r4   - r0;
	   x[5] = r6   - r2;
           x[6] = r4   + r0;
	   x[7] = r6   + r2;
	   MB();
}

/* 16 point butterfly (in place, 4 register) */
STIN void mdct_butterfly_16(DATA_TYPE *x){
  
  REG_TYPE r0, r1, r2, r3;
  
	   r0 = x[ 8] - x[ 9]; x[ 8] += x[ 9];
	   r1 = x[10] - x[11]; x[10] += x[11];
	   r2 = x[ 1] - x[ 0]; x[ 9]  = x[ 1] + x[0];
	   r3 = x[ 3] - x[ 2]; x[11]  = x[ 3] + x[2];
	   x[ 0] = MULT31((r0 - r1) , cPI2_8);
	   x[ 1] = MULT31((r2 + r3) , cPI2_8);
	   x[ 2] = MULT31((r0 + r1) , cPI2_8);
	   x[ 3] = MULT31((r3 - r2) , cPI2_8);
	   MB();

	   r2 = x[12] - x[13]; x[12] += x[13];
	   r3 = x[14] - x[15]; x[14] += x[15];
	   r0 = x[ 4] - x[ 5]; x[13]  = x[ 5] + x[ 4];
	   r1 = x[ 7] - x[ 6]; x[15]  = x[ 7] + x[ 6];
	   x[ 4] = r2; x[ 5] = r1; 
	   x[ 6] = r3; x[ 7] = r0;
	   MB();

	   mdct_butterfly_8(x);
	   mdct_butterfly_8(x+8);
}

/* 32 point butterfly (in place, 4 register) */
STIN void mdct_butterfly_32(DATA_TYPE *x){

  REG_TYPE r0, r1, r2, r3;

	   r0 = x[16] - x[17]; x[16] += x[17];
	   r1 = x[18] - x[19]; x[18] += x[19];
	   r2 = x[ 1] - x[ 0]; x[17]  = x[ 1] + x[ 0];
	   r3 = x[ 3] - x[ 2]; x[19]  = x[ 3] + x[ 2];
	   XNPROD31( r0, r1, cPI3_8, cPI1_8, &x[ 0], &x[ 2] );
	   XPROD31 ( r2, r3, cPI1_8, cPI3_8, &x[ 1], &x[ 3] );
	   MB();

	   r0 = x[20] - x[21]; x[20] += x[21];
	   r1 = x[22] - x[23]; x[22] += x[23];
	   r2 = x[ 5] - x[ 4]; x[21]  = x[ 5] + x[ 4];
	   r3 = x[ 7] - x[ 6]; x[23]  = x[ 7] + x[ 6];
	   x[ 4] = MULT31((r0 - r1) , cPI2_8);
	   x[ 5] = MULT31((r3 + r2) , cPI2_8);
	   x[ 6] = MULT31((r0 + r1) , cPI2_8);
	   x[ 7] = MULT31((r3 - r2) , cPI2_8);
	   MB();

	   r0 = x[24] - x[25]; x[24] += x[25];           
	   r1 = x[26] - x[27]; x[26] += x[27];
	   r2 = x[ 9] - x[ 8]; x[25]  = x[ 9] + x[ 8];
	   r3 = x[11] - x[10]; x[27]  = x[11] + x[10];
	   XNPROD31( r0, r1, cPI1_8, cPI3_8, &x[ 8], &x[10] );
	   XPROD31 ( r2, r3, cPI3_8, cPI1_8, &x[ 9], &x[11] );
	   MB();

	   r0 = x[28] - x[29]; x[28] += x[29];           
	   r1 = x[30] - x[31]; x[30] += x[31];
	   r2 = x[12] - x[13]; x[29]  = x[13] + x[12];
	   r3 = x[15] - x[14]; x[31]  = x[15] + x[14];
	   x[12] = r0; x[13] = r3; 
	   x[14] = r1; x[15] = r2;
	   MB();

	   mdct_butterfly_16(x);
	   mdct_butterfly_16(x+16);
}

/* N/stage point generic N stage butterfly (in place, 2 register) */
STIN void mdct_butterfly_generic(DATA_TYPE *x,int points,int step){

  LOOKUP_T   *T  = sincos_lookup0;
  DATA_TYPE *x1  = x + points - 4;
  DATA_TYPE *x2  = x + (points>>1) - 4;
  REG_TYPE   r0, r1, r2, r3;

  do{
    r0 = x1[0] - x1[1]; x1[0] += x1[1];
    r1 = x1[3] - x1[2]; x1[2] += x1[3];
    r2 = x2[1] - x2[0]; x1[1]  = x2[1] + x2[0];
    r3 = x2[3] - x2[2]; x1[3]  = x2[3] + x2[2];
    XPROD31( r1, r0, T[0], T[1], &x2[0], &x2[2] );
    XPROD31( r2, r3, T[0], T[1], &x2[1], &x2[3] ); T+=step;
    x1-=4; 
    x2-=4;
  }while(T<sincos_lookup0+1024);
  do{
    r0 = x1[0] - x1[1]; x1[0] += x1[1];
    r1 = x1[2] - x1[3]; x1[2] += x1[3];
    r2 = x2[0] - x2[1]; x1[1]  = x2[1] + x2[0];
    r3 = x2[3] - x2[2]; x1[3]  = x2[3] + x2[2];
    XNPROD31( r0, r1, T[0], T[1], &x2[0], &x2[2] );
    XNPROD31( r3, r2, T[0], T[1], &x2[1], &x2[3] ); T-=step;
    x1-=4; 
    x2-=4; 
  }while(T>sincos_lookup0);
}

STIN void mdct_butterflies(DATA_TYPE *x,int points,int shift){

  int stages=8-shift;
  int i,j;

  for(i=0;--stages>0;i++){
    for(j=0;j<(1<<i);j++)
      mdct_butterfly_generic(x+(points>>i)*j,points>>i,4<<(i+shift));
  }
  
  for(j=0;j<points;j+=32)
    mdct_butterfly_32(x+j);
}

static unsigned char bitrev[16]={0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};

STIN int bitrev12(int x){
  return bitrev[x>>8]|(bitrev[(x&0x0f0)>>4]<<4)|(((int)bitrev[x&0x00f])<<8);
}

STIN void mdct_bitreverse(DATA_TYPE *x,int n,int shift){
  int          bit   = 0;
  DATA_TYPE   *w     = x+(n>>1);

  do{
    DATA_TYPE  b     = bitrev12(bit++);
    DATA_TYPE *xx    = x + (b>>shift);
    REG_TYPE  r;

               w    -= 2;

	       if(w>xx){

		 r      = xx[0];
		 xx[0]  = w[0];
		 w[0]   = r;
		 
		 r      = xx[1];
		 xx[1]  = w[1];
		 w[1]   = r;
	       }
  }while(w>x);
}

STIN void mdct_step7(DATA_TYPE *x,int n,int step){
  DATA_TYPE   *w0    = x;
  DATA_TYPE   *w1    = x+(n>>1);
  LOOKUP_T    *T = (step>=4)?(sincos_lookup0+(step>>1)):sincos_lookup1;
  LOOKUP_T    *Ttop  = T+1024;
  REG_TYPE     r0, r1, r2, r3;
  
  do{
	      w1    -= 2;

              r0     = w0[0]  + w1[0];
              r1     = w1[1]  - w0[1];	      
	      r2     = MULT32(r0, T[1]) + MULT32(r1, T[0]);
	      r3     = MULT32(r1, T[1]) - MULT32(r0, T[0]);
	      T+=step;

	      r0     = (w0[1] + w1[1])>>1;
              r1     = (w0[0] - w1[0])>>1;
	      w0[0]  = r0     + r2;
	      w0[1]  = r1     + r3;
	      w1[0]  = r0     - r2;
	      w1[1]  = r3     - r1;

	      w0    += 2;
  }while(T<Ttop);
  do{
	      w1    -= 2;

              r0     = w0[0]  + w1[0];
              r1     = w1[1]  - w0[1];	
	      T-=step;
	      r2     = MULT32(r0, T[0]) + MULT32(r1, T[1]);
	      r3     = MULT32(r1, T[0]) - MULT32(r0, T[1]);      

	      r0     = (w0[1] + w1[1])>>1;
              r1     = (w0[0] - w1[0])>>1;
	      w0[0]  = r0     + r2;
	      w0[1]  = r1     + r3;
	      w1[0]  = r0     - r2;
	      w1[1]  = r3     - r1;

	      w0    += 2;
  }while(w0<w1);
}

STIN void mdct_step8(DATA_TYPE *x, int n, int step){
  LOOKUP_T *T;
  LOOKUP_T *V;
  DATA_TYPE *iX =x+(n>>1);
  step>>=2;

  switch(step) {
  default: 
    T=(step>=4)?(sincos_lookup0+(step>>1)):sincos_lookup1;
    do{
      REG_TYPE     r0  =  x[0];
      REG_TYPE     r1  = -x[1];
                   XPROD31( r0, r1, T[0], T[1], x, x+1); T+=step;
                   x  +=2;
    }while(x<iX);
    break;
  
  case 1: 
    {
      /* linear interpolation between table values: offset=0.5, step=1 */
      REG_TYPE    t0,t1,v0,v1,r0,r1;
      T         = sincos_lookup0;
      V         = sincos_lookup1;
      t0        = (*T++)>>1;
      t1        = (*T++)>>1;
      do{
	    r0  =  x[0];
	    r1  = -x[1];	
	    t0 += (v0 = (*V++)>>1);
	    t1 += (v1 = (*V++)>>1);
	    XPROD31( r0, r1, t0, t1, x, x+1 );
	    
	    r0  =  x[2];
	    r1  = -x[3];
	    v0 += (t0 = (*T++)>>1);
	    v1 += (t1 = (*T++)>>1);
	    XPROD31( r0, r1, v0, v1, x+2, x+3 );
	    
	    x += 4;
      }while(x<iX);
      break;
    }
    
  case 0: 
    {
      /* linear interpolation between table values: offset=0.25, step=0.5 */
      REG_TYPE    t0,t1,v0,v1,q0,q1,r0,r1;
      T         = sincos_lookup0;
      V         = sincos_lookup1;
      t0        = *T++;
      t1        = *T++;
      do{

	
	v0  = *V++;
	v1  = *V++;
	t0 +=  (q0 = (v0-t0)>>2);
	t1 +=  (q1 = (v1-t1)>>2);
	r0  =  x[0];
	r1  = -x[1];	
	XPROD31( r0, r1, t0, t1, x, x+1 );
	t0  = v0-q0;
	t1  = v1-q1;
	r0  =  x[2];
	r1  = -x[3];	
	XPROD31( r0, r1, t0, t1, x+2, x+3 );
	
	t0  = *T++;
	t1  = *T++;
	v0 += (q0 = (t0-v0)>>2);
	v1 += (q1 = (t1-v1)>>2);
	r0  =  x[4];
	r1  = -x[5];	
	XPROD31( r0, r1, v0, v1, x+4, x+5 );
	v0  = t0-q0;
	v1  = t1-q1;
	r0  =  x[6];
	r1  = -x[7];	
	XPROD31( r0, r1, v0, v1, x+5, x+6 );

	x+=8;
      }while(x<iX);
      break;
    }
  }
}

/* partial; doesn't perform last-step deinterleave/unrolling.  That
   can be done more efficiently during pcm output */
void mdct_backward(int n, DATA_TYPE *in){
  int shift;
  int step;
  
  for (shift=4;!(n&(1<<shift));shift++);
  shift=13-shift;
  step=2<<shift;
   
  presymmetry(in,n>>1,step);
  mdct_butterflies(in,n>>1,shift);
  mdct_bitreverse(in,n,shift);
  mdct_step7(in,n,step);
  mdct_step8(in,n,step);
}

void mdct_shift_right(int n, DATA_TYPE *in, DATA_TYPE *right){
  int i;
  n>>=2;
  in+=1;

  for(i=0;i<n;i++)
    right[i]=in[i<<1];
}

void mdct_unroll_lap(int n0,int n1,
		     int lW,int W,
		     DATA_TYPE *in,
		     DATA_TYPE *right,
		     LOOKUP_T *w0,
		     LOOKUP_T *w1,
		     ogg_int16_t *out,
		     int step,
		     int start, /* samples, this frame */
		     int end    /* samples, this frame */){

  DATA_TYPE *l=in+(W&&lW ? n1>>1 : n0>>1);
  DATA_TYPE *r=right+(lW ? n1>>2 : n0>>2);
  DATA_TYPE *post;
  LOOKUP_T *wR=(W && lW ? w1+(n1>>1) : w0+(n0>>1));
  LOOKUP_T *wL=(W && lW ? w1         : w0        );

  int preLap=(lW && !W ? (n1>>2)-(n0>>2) : 0 );
  int halfLap=(lW && W ? (n1>>2) : (n0>>2) );
  int postLap=(!lW && W ? (n1>>2)-(n0>>2) : 0 );
  int n,off;

  /* preceeding direct-copy lapping from previous frame, if any */
  if(preLap){
    n      = (end<preLap?end:preLap);
    off    = (start<preLap?start:preLap);
    post   = r-n;
    r     -= off;
    start -= off;
    end   -= n;
    while(r>post){
      *out = CLIP_TO_15((*--r)>>9);
      out+=step;
    }
  }
  
  /* cross-lap; two halves due to wrap-around */
  n      = (end<halfLap?end:halfLap);
  off    = (start<halfLap?start:halfLap);
  post   = r-n;
  r     -= off;
  l     -= off*2;
  start -= off;
  wR    -= off;
  wL    += off;
  end   -= n;
  while(r>post){
    l-=2;
    *out = CLIP_TO_15((MULT31(*--r,*--wR) + MULT31(*l,*wL++))>>9);
    out+=step;
  }

  n      = (end<halfLap?end:halfLap);
  off    = (start<halfLap?start:halfLap);
  post   = r+n;
  r     += off;
  l     += off*2;
  start -= off;
  end   -= n;
  wR    -= off;
  wL    += off;
  while(r<post){
    *out = CLIP_TO_15((MULT31(*r++,*--wR) - MULT31(*l,*wL++))>>9);
    out+=step;
    l+=2;
  }

  /* preceeding direct-copy lapping from previous frame, if any */
  if(postLap){
    n      = (end<postLap?end:postLap);
    off    = (start<postLap?start:postLap);
    post   = l+n*2;
    l     += off*2;
    while(l<post){
      *out = CLIP_TO_15((-*l)>>9);
      out+=step;
      l+=2;
    }
  }
}

