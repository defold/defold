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

 function: floor backend 1 implementation

 ********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ogg.h"
#include "ivorbiscodec.h"
#include "codec_internal.h"
#include "codebook.h"
#include "misc.h"

extern const ogg_int32_t FLOOR_fromdB_LOOKUP[];
#define floor1_rangedB 140 /* floor 1 fixed at -140dB to 0dB range */
#define VIF_POSIT 63

/***********************************************/
 
void floor1_free_info(vorbis_info_floor *i){
  vorbis_info_floor1 *info=(vorbis_info_floor1 *)i;
  if(info){
    if(info->class)_ogg_free(info->class);
    if(info->partitionclass)_ogg_free(info->partitionclass);
    if(info->postlist)_ogg_free(info->postlist);
    if(info->forward_index)_ogg_free(info->forward_index);
    if(info->hineighbor)_ogg_free(info->hineighbor);
    if(info->loneighbor)_ogg_free(info->loneighbor);
    memset(info,0,sizeof(*info));
    _ogg_free(info);
  }
}

static int ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

static void vorbis_mergesort(char *index,ogg_uint16_t *vals,ogg_uint16_t n){
  ogg_uint16_t i,j;
  char *temp,*A=index,*B=_ogg_malloc(n*sizeof(*B));

  for(i=1;i<n;i<<=1){
    for(j=0;j+i<n;){
      int k1=j;
      int mid=j+i;
      int k2=mid;
      int end=(j+i*2<n?j+i*2:n);
      while(k1<mid && k2<end){
	if(vals[A[k1]]<vals[A[k2]])
	  B[j++]=A[k1++];
	else
	  B[j++]=A[k2++];
      }
      while(k1<mid) B[j++]=A[k1++];
      while(k2<end) B[j++]=A[k2++];
    }
    for(;j<n;j++)B[j]=A[j];
    temp=A;A=B;B=temp;
  }
 
  if(B==index){
    for(j=0;j<n;j++)B[j]=A[j];
    _ogg_free(A);
  }else
    _ogg_free(B);
}


vorbis_info_floor *floor1_info_unpack (vorbis_info *vi,oggpack_buffer *opb){
  codec_setup_info     *ci=(codec_setup_info *)vi->codec_setup;
  int j,k,count=0,maxclass=-1,rangebits;
  
  vorbis_info_floor1 *info=(vorbis_info_floor1 *)_ogg_calloc(1,sizeof(*info));
  /* read partitions */
  info->partitions=oggpack_read(opb,5); /* only 0 to 31 legal */
  info->partitionclass=
    (char *)_ogg_malloc(info->partitions*sizeof(*info->partitionclass));
  for(j=0;j<info->partitions;j++){
    info->partitionclass[j]=oggpack_read(opb,4); /* only 0 to 15 legal */
    if(maxclass<info->partitionclass[j])maxclass=info->partitionclass[j];
  }

  /* read partition classes */
  info->class=
    (floor1class *)_ogg_malloc((maxclass+1)*sizeof(*info->class));
  for(j=0;j<maxclass+1;j++){
    info->class[j].class_dim=oggpack_read(opb,3)+1; /* 1 to 8 */
    info->class[j].class_subs=oggpack_read(opb,2); /* 0,1,2,3 bits */
    if(oggpack_eop(opb)<0) goto err_out;
    if(info->class[j].class_subs)
      info->class[j].class_book=oggpack_read(opb,8);
    else
      info->class[j].class_book=0;
    if(info->class[j].class_book>=ci->books)goto err_out;
    for(k=0;k<(1<<info->class[j].class_subs);k++){
      info->class[j].class_subbook[k]=oggpack_read(opb,8)-1;
      if(info->class[j].class_subbook[k]>=ci->books &&
	 info->class[j].class_subbook[k]!=0xff)goto err_out;
    }
  }

  /* read the post list */
  info->mult=oggpack_read(opb,2)+1;     /* only 1,2,3,4 legal now */ 
  rangebits=oggpack_read(opb,4);

  for(j=0,k=0;j<info->partitions;j++)
    count+=info->class[info->partitionclass[j]].class_dim; 
  info->postlist=
    (ogg_uint16_t *)_ogg_malloc((count+2)*sizeof(*info->postlist));
  info->forward_index=
    (char *)_ogg_malloc((count+2)*sizeof(*info->forward_index));
  info->loneighbor=
    (char *)_ogg_malloc(count*sizeof(*info->loneighbor));
  info->hineighbor=
    (char *)_ogg_malloc(count*sizeof(*info->hineighbor));

  count=0;
  for(j=0,k=0;j<info->partitions;j++){
    count+=info->class[info->partitionclass[j]].class_dim; 
    for(;k<count;k++){
      int t=info->postlist[k+2]=oggpack_read(opb,rangebits);
      if(t>=(1<<rangebits))goto err_out;
    }
  }
  if(oggpack_eop(opb))goto err_out;
  info->postlist[0]=0;
  info->postlist[1]=1<<rangebits;
  info->posts=count+2;

  /* also store a sorted position index */
  for(j=0;j<info->posts;j++)info->forward_index[j]=j;
  vorbis_mergesort(info->forward_index,info->postlist,info->posts);
  
  /* discover our neighbors for decode where we don't use fit flags
     (that would push the neighbors outward) */
  for(j=0;j<info->posts-2;j++){
    int lo=0;
    int hi=1;
    int lx=0;
    int hx=info->postlist[1];
    int currentx=info->postlist[j+2];
    for(k=0;k<j+2;k++){
      int x=info->postlist[k];
      if(x>lx && x<currentx){
	lo=k;
	lx=x;
      }
      if(x<hx && x>currentx){
	hi=k;
	hx=x;
      }
    }
    info->loneighbor[j]=lo;
    info->hineighbor[j]=hi;
  }

  return(info);
  
 err_out:
  floor1_free_info(info);
  return(NULL);
}

static int render_point(int x0,int x1,int y0,int y1,int x){
  y0&=0x7fff; /* mask off flag */
  y1&=0x7fff;
    
  {
    int dy=y1-y0;
    int adx=x1-x0;
    int ady=abs(dy);
    int err=ady*(x-x0);
    
    int off=err/adx;
    if(dy<0)return(y0-off);
    return(y0+off);
  }
}

static void render_line(int n,int x0,int x1,int y0,int y1,ogg_int32_t *d){
  int dy=y1-y0;
  int adx=x1-x0;
  int ady=abs(dy);
  int base=dy/adx;
  int sy=(dy<0?base-1:base+1);
  int x=x0;
  int y=y0;
  int err=0;

  if(n>x1)n=x1;
  ady-=abs(base*adx);

  if(x<n)
    d[x]= MULT31_SHIFT15(d[x],FLOOR_fromdB_LOOKUP[y]);

  while(++x<n){
    err=err+ady;
    if(err>=adx){
      err-=adx;
      y+=sy;
    }else{
      y+=base;
    }
    d[x]= MULT31_SHIFT15(d[x],FLOOR_fromdB_LOOKUP[y]);
  }
}

int floor1_memosize(vorbis_info_floor *i){
  vorbis_info_floor1 *info=(vorbis_info_floor1 *)i;
  return info->posts;
}

static int quant_look[4]={256,128,86,64};

ogg_int32_t *floor1_inverse1(vorbis_dsp_state *vd,vorbis_info_floor *in,
			     ogg_int32_t *fit_value){
  vorbis_info_floor1 *info=(vorbis_info_floor1 *)in;
  codec_setup_info   *ci=(codec_setup_info *)vd->vi->codec_setup;
  
  int i,j,k;
  codebook *books=ci->book_param;   
  int quant_q=quant_look[info->mult-1];

  /* unpack wrapped/predicted values from stream */
  if(oggpack_read(&vd->opb,1)==1){
    fit_value[0]=oggpack_read(&vd->opb,ilog(quant_q-1));
    fit_value[1]=oggpack_read(&vd->opb,ilog(quant_q-1));
    
    /* partition by partition */
    /* partition by partition */
    for(i=0,j=2;i<info->partitions;i++){
      int classv=info->partitionclass[i];
      int cdim=info->class[classv].class_dim;
      int csubbits=info->class[classv].class_subs;
      int csub=1<<csubbits;
      int cval=0;

      /* decode the partition's first stage cascade value */
      if(csubbits){
	cval=vorbis_book_decode(books+info->class[classv].class_book,&vd->opb);

	if(cval==-1)goto eop;
      }

      for(k=0;k<cdim;k++){
	int book=info->class[classv].class_subbook[cval&(csub-1)];
	cval>>=csubbits;
	if(book!=0xff){
	  if((fit_value[j+k]=vorbis_book_decode(books+book,&vd->opb))==-1)
	    goto eop;
	}else{
	  fit_value[j+k]=0;
	}
      }
      j+=cdim;
    }

    /* unwrap positive values and reconsitute via linear interpolation */
    for(i=2;i<info->posts;i++){
      int predicted=render_point(info->postlist[info->loneighbor[i-2]],
				 info->postlist[info->hineighbor[i-2]],
				 fit_value[info->loneighbor[i-2]],
				 fit_value[info->hineighbor[i-2]],
				 info->postlist[i]);
      int hiroom=quant_q-predicted;
      int loroom=predicted;
      int room=(hiroom<loroom?hiroom:loroom)<<1;
      int val=fit_value[i];

      if(val){
	if(val>=room){
	  if(hiroom>loroom){
	    val = val-loroom;
	  }else{
	  val = -1-(val-hiroom);
	  }
	}else{
	  if(val&1){
	    val= -((val+1)>>1);
	  }else{
	    val>>=1;
	  }
	}

	fit_value[i]=val+predicted;
	fit_value[info->loneighbor[i-2]]&=0x7fff;
	fit_value[info->hineighbor[i-2]]&=0x7fff;

      }else{
	fit_value[i]=predicted|0x8000;
      }
	
    }

    return(fit_value);
  }
 eop:
  return(NULL);
}

int floor1_inverse2(vorbis_dsp_state *vd,vorbis_info_floor *in,
		    ogg_int32_t *fit_value,ogg_int32_t *out){
  vorbis_info_floor1 *info=(vorbis_info_floor1 *)in;

  codec_setup_info   *ci=(codec_setup_info *)vd->vi->codec_setup;
  int                  n=ci->blocksizes[vd->W]/2;
  int j;

  if(fit_value){
    /* render the lines */
    int hx=0;
    int lx=0;
    int ly=fit_value[0]*info->mult;
    for(j=1;j<info->posts;j++){
      int current=info->forward_index[j];
      int hy=fit_value[current]&0x7fff;
      if(hy==fit_value[current]){
	
	hy*=info->mult;
	hx=info->postlist[current];
	
	render_line(n,lx,hx,ly,hy,out);
	
	lx=hx;
	ly=hy;
      }
    }
    for(j=hx;j<n;j++)out[j]*=ly; /* be certain */    
    return(1);
  }
  memset(out,0,sizeof(*out)*n);
  return(0);
}
