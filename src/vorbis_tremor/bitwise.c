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

  function: packing variable sized words into an octet stream

 ********************************************************************/

/* We're 'LSb' endian; if we write a word but read individual bits,
   then we'll read the lsb first */

#include <string.h>
#include <stdlib.h>
#include "misc.h"
#include "ogg.h"

static unsigned long mask[]=
{0x00000000,0x00000001,0x00000003,0x00000007,0x0000000f,
 0x0000001f,0x0000003f,0x0000007f,0x000000ff,0x000001ff,
 0x000003ff,0x000007ff,0x00000fff,0x00001fff,0x00003fff,
 0x00007fff,0x0000ffff,0x0001ffff,0x0003ffff,0x0007ffff,
 0x000fffff,0x001fffff,0x003fffff,0x007fffff,0x00ffffff,
 0x01ffffff,0x03ffffff,0x07ffffff,0x0fffffff,0x1fffffff,
 0x3fffffff,0x7fffffff,0xffffffff };

/* spans forward, skipping as many bytes as headend is negative; if
   headend is zero, simply finds next byte.  If we're up to the end
   of the buffer, leaves headend at zero.  If we've read past the end,
   halt the decode process. */

static void _span(oggpack_buffer *b){
  while(b->headend-(b->headbit>>3)<1){
    b->headend-=b->headbit>>3;
    b->headbit&=0x7;

    if(b->head->next){
      b->count+=b->head->length;
      b->head=b->head->next;

      if(b->headend+b->head->length>0)
	b->headptr=b->head->buffer->data+b->head->begin-b->headend;

      b->headend+=b->head->length; 
    }else{
      /* we've either met the end of decode, or gone past it. halt
	 only if we're past */
      if(b->headend*8<b->headbit)
	/* read has fallen off the end */
	b->headend=-1;
        break;
    }
  }
}

void oggpack_readinit(oggpack_buffer *b,ogg_reference *r){
  memset(b,0,sizeof(*b));

  b->tail=b->head=r;
  b->count=0;
  b->headptr=b->head->buffer->data+b->head->begin;
  b->headend=b->head->length;
  _span(b);
}

#define _lookspan()   while(!end){\
                        head=head->next;\
                        if(!head) return -1;\
                        ptr=head->buffer->data + head->begin;\
                        end=head->length;\
                      }

/* Read in bits without advancing the bitptr; bits <= 32 */
long oggpack_look(oggpack_buffer *b,int bits){
  unsigned long m=mask[bits];
  unsigned long ret;

  bits+=b->headbit;

  if(bits >= b->headend<<3){
    int            end=b->headend;
    unsigned char *ptr=b->headptr;
    ogg_reference *head=b->head;

    if(end<0)return -1;
    
    if(bits){
      _lookspan();
      ret=*ptr++>>b->headbit;
      if(bits>8){
        --end;
        _lookspan();
        ret|=*ptr++<<(8-b->headbit);  
        if(bits>16){
          --end;
          _lookspan();
          ret|=*ptr++<<(16-b->headbit);  
          if(bits>24){
            --end;
            _lookspan();
            ret|=*ptr++<<(24-b->headbit);  
            if(bits>32 && b->headbit){
              --end;
              _lookspan();
              ret|=*ptr<<(32-b->headbit);
            }
          }
        }
      }
    }

  }else{

    /* make this a switch jump-table */
    ret=b->headptr[0]>>b->headbit;
    if(bits>8){
      ret|=b->headptr[1]<<(8-b->headbit);  
      if(bits>16){
        ret|=b->headptr[2]<<(16-b->headbit);  
        if(bits>24){
          ret|=b->headptr[3]<<(24-b->headbit);  
          if(bits>32 && b->headbit)
            ret|=b->headptr[4]<<(32-b->headbit);
        }
      }
    }
  }

  ret&=m;
  return ret;
}

/* limited to 32 at a time */
void oggpack_adv(oggpack_buffer *b,int bits){
  bits+=b->headbit;
  b->headbit=bits&7;
  b->headend-=(bits>>3);
  b->headptr+=(bits>>3);
  if(b->headend<1)_span(b);
}

int oggpack_eop(oggpack_buffer *b){
  if(b->headend<0)return -1;
  return 0;
}

/* bits <= 32 */
long oggpack_read(oggpack_buffer *b,int bits){
  long ret=oggpack_look(b,bits);
  oggpack_adv(b,bits);
  return(ret);
}

long oggpack_bytes(oggpack_buffer *b){
  if(b->headend<0)return b->count+b->head->length;
  return b->count + b->head->length-b->headend + 
    (b->headbit+7)/8;
}

long oggpack_bits(oggpack_buffer *b){
  if(b->headend<0)return (b->count+b->head->length)*8;
  return (b->count + b->head->length-b->headend)*8 + 
    b->headbit;
}

/* Self test of the bitwise routines; everything else is based on
   them, so they damned well better be solid. */

#ifdef _V_BIT_TEST
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "framing.c"

static int ilog(unsigned long v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}
      
oggpack_buffer r;
oggpack_buffer o;
ogg_buffer_state *bs;
ogg_reference *or;
#define TESTWORDS 256

void report(char *in){
  fprintf(stderr,"%s",in);
  exit(1);
}

int getbyte(ogg_reference *or,int position){
  while(or && position>=or->length){
    position-=or->length;
    or=or->next;
    if(or==NULL){
      fprintf(stderr,"\n\tERROR: getbyte ran off end of buffer.\n");
      exit(1);
    }
  }

  if((position+or->begin)&1)
    return (or->buffer->data[(position+or->begin)>>1])&0xff;
  else
    return (or->buffer->data[(position+or->begin)>>1]>>8)&0xff;
}

void cliptest(unsigned long *b,int vals,int bits,int *comp,int compsize){
  long i,bitcount=0;
  ogg_reference *or=ogg_buffer_alloc(bs,64);
  for(i=0;i<compsize;i++)
    or->buffer->data[i]= comp[i];
  or->length=i;

  oggpack_readinit(&r,or);
  for(i=0;i<vals;i++){
    unsigned long test;
    int tbit=bits?bits:ilog(b[i]);
    if((test=oggpack_look(&r,tbit))==0xffffffff)
      report("out of data!\n");
    if(test!=(b[i]&mask[tbit])){
      fprintf(stderr,"%ld) %lx %lx\n",i,(b[i]&mask[tbit]),test);
      report("looked at incorrect value!\n");
    }
    if((test=oggpack_read(&r,tbit))==0xffffffff){
      report("premature end of data when reading!\n");
    }
    if(test!=(b[i]&mask[tbit])){
      fprintf(stderr,"%ld) %lx %lx\n",i,(b[i]&mask[tbit]),test);
      report("read incorrect value!\n");
    }
    bitcount+=tbit;

    if(bitcount!=oggpack_bits(&r))
      report("wrong number of bits while reading!\n");
    if((bitcount+7)/8!=oggpack_bytes(&r))
      report("wrong number of bytes while reading!\n");

  }
  if(oggpack_bytes(&r)!=(bitcount+7)/8)report("leftover bytes after read!\n");
  ogg_buffer_release(or);
}

void _end_verify(int count){
  int i;

  /* are the proper number of bits left over? */
  int leftover=count*8-oggpack_bits(&o);
  if(leftover>7)
    report("\nERROR: too many bits reported left over.\n");
  
  /* does reading to exactly byte alignment *not* trip EOF? */
  if(oggpack_read(&o,leftover)==-1)
    report("\nERROR: read to but not past exact end tripped EOF.\n");
  if(oggpack_bits(&o)!=count*8)
    report("\nERROR: read to but not past exact end reported bad bitcount.\n");

  /* does EOF trip properly after a single additional bit? */
  if(oggpack_read(&o,1)!=-1)
    report("\nERROR: read past exact end did not trip EOF.\n");
  if(oggpack_bits(&o)!=count*8)
    report("\nERROR: read past exact end reported bad bitcount.\n");
  
  /* does EOF stay set over additional bit reads? */
  for(i=0;i<=32;i++){
    if(oggpack_read(&o,i)!=-1)
      report("\nERROR: EOF did not stay set on stream.\n");
    if(oggpack_bits(&o)!=count*8)
      report("\nERROR: read past exact end reported bad bitcount.\n");
  }
}       

void _end_verify2(int count){
  int i;

  /* are the proper number of bits left over? */
  int leftover=count*8-oggpack_bits(&o);
  if(leftover>7)
    report("\nERROR: too many bits reported left over.\n");
  
  /* does reading to exactly byte alignment *not* trip EOF? */
  oggpack_adv(&o,leftover);
  if(o.headend!=0)
    report("\nERROR: read to but not past exact end tripped EOF.\n");
  if(oggpack_bits(&o)!=count*8)
    report("\nERROR: read to but not past exact end reported bad bitcount.\n");
  
  /* does EOF trip properly after a single additional bit? */
  oggpack_adv(&o,1);
  if(o.headend>=0)
    report("\nERROR: read past exact end did not trip EOF.\n");
  if(oggpack_bits(&o)!=count*8)
    report("\nERROR: read past exact end reported bad bitcount.\n");
  
  /* does EOF stay set over additional bit reads? */
  for(i=0;i<=32;i++){
    oggpack_adv(&o,i);
    if(o.headend>=0)
      report("\nERROR: EOF did not stay set on stream.\n");
    if(oggpack_bits(&o)!=count*8)
      report("\nERROR: read past exact end reported bad bitcount.\n");
  }
}       

long ogg_buffer_length(ogg_reference *or){
  int count=0;
  while(or){
    count+=or->length;
    or=or->next;
  }
  return count;
}

ogg_reference *ogg_buffer_extend(ogg_reference *or,long bytes){
  if(or){
    while(or->next){
      or=or->next;
    }
    or->next=ogg_buffer_alloc(or->buffer->ptr.owner,bytes);
    return(or->next);
  }
  return 0;
}

void ogg_buffer_posttruncate(ogg_reference *or,long pos){
  /* walk to the point where we want to begin truncate */
  while(or && pos>or->length){
    pos-=or->length;
    or=or->next;
  }
  if(or){
    ogg_buffer_release(or->next);
    or->next=0;
    or->length=pos;
  }
}

int main(void){
  long i;
  static unsigned long testbuffer1[]=
    {18,12,103948,4325,543,76,432,52,3,65,4,56,32,42,34,21,1,23,32,546,456,7,
       567,56,8,8,55,3,52,342,341,4,265,7,67,86,2199,21,7,1,5,1,4};
  int test1size=43;

  static unsigned long testbuffer2[]=
    {216531625L,1237861823,56732452,131,3212421,12325343,34547562,12313212,
       1233432,534,5,346435231,14436467,7869299,76326614,167548585,
       85525151,0,12321,1,349528352};
  int test2size=21;

  static unsigned long testbuffer3[]=
    {1,0,14,0,1,0,12,0,1,0,0,0,1,1,0,1,0,1,0,1,0,1,0,1,0,1,0,0,1,1,1,1,1,0,0,1,
       0,1,30,1,1,1,0,0,1,0,0,0,12,0,11,0,1,0,0,1};
  int test3size=56;

  static unsigned long large[]=
    {2136531625L,2137861823,56732452,131,3212421,12325343,34547562,12313212,
       1233432,534,5,2146435231,14436467,7869299,76326614,167548585,
       85525151,0,12321,1,2146528352};

  int onesize=33;
  static int one[33]={146,25,44,151,195,15,153,176,233,131,196,65,85,172,47,40,
                    34,242,223,136,35,222,211,86,171,50,225,135,214,75,172,
                    223,4};

  int twosize=6;
  static int two[6]={61,255,255,251,231,29};

  int threesize=54;
  static int three[54]={169,2,232,252,91,132,156,36,89,13,123,176,144,32,254,
                      142,224,85,59,121,144,79,124,23,67,90,90,216,79,23,83,
                      58,135,196,61,55,129,183,54,101,100,170,37,127,126,10,
                      100,52,4,14,18,86,77,1};

  int foursize=38;
  static int four[38]={18,6,163,252,97,194,104,131,32,1,7,82,137,42,129,11,72,
                     132,60,220,112,8,196,109,64,179,86,9,137,195,208,122,169,
                     28,2,133,0,1};

  int fivesize=45;
  static int five[45]={169,2,126,139,144,172,30,4,80,72,240,59,130,218,73,62,
                     241,24,210,44,4,20,0,248,116,49,135,100,110,130,181,169,
                     84,75,159,2,1,0,132,192,8,0,0,18,22};

  int sixsize=7;
  static int six[7]={17,177,170,242,169,19,148};

  /* Test read/write together */
  /* Later we test against pregenerated bitstreams */
  bs=ogg_buffer_create();

  fprintf(stderr,"\nSmall preclipped packing (LSb): ");
  cliptest(testbuffer1,test1size,0,one,onesize);
  fprintf(stderr,"ok.");

  fprintf(stderr,"\nNull bit call (LSb): ");
  cliptest(testbuffer3,test3size,0,two,twosize);
  fprintf(stderr,"ok.");

  fprintf(stderr,"\nLarge preclipped packing (LSb): ");
  cliptest(testbuffer2,test2size,0,three,threesize);
  fprintf(stderr,"ok.");

  fprintf(stderr,"\n32 bit preclipped packing (LSb): ");

  or=ogg_buffer_alloc(bs,128);
  for(i=0;i<test2size;i++){
    or->buffer->data[i*4]  = large[i]&0xff;
    or->buffer->data[i*4+1]  = (large[i]>>8)&0xff;
    or->buffer->data[i*4+2]  = (large[i]>>16)&0xff;
    or->buffer->data[i*4+3]  = (large[i]>>24)&0xff;
  }
  or->length=test2size*4;
  oggpack_readinit(&r,or);
  for(i=0;i<test2size;i++){
    unsigned long test;
    if((test=oggpack_look(&r,32))==0xffffffffUL)report("out of data. failed!");
    if(test!=large[i]){
      fprintf(stderr,"%ld != %ld (%lx!=%lx):",test,large[i],
              test,large[i]);
      report("read incorrect value!\n");
    }
    oggpack_adv(&r,32);
  }
  ogg_buffer_release(or);
  if(oggpack_bytes(&r)!=test2size*4)report("leftover bytes after read!\n");
  fprintf(stderr,"ok.");
  
  fprintf(stderr,"\nSmall unclipped packing (LSb): ");
  cliptest(testbuffer1,test1size,7,four,foursize);
  fprintf(stderr,"ok.");

  fprintf(stderr,"\nLarge unclipped packing (LSb): ");
  cliptest(testbuffer2,test2size,17,five,fivesize);
  fprintf(stderr,"ok.");

  fprintf(stderr,"\nSingle bit unclipped packing (LSb): ");
  cliptest(testbuffer3,test3size,1,six,sixsize);
  fprintf(stderr,"ok.");

  fprintf(stderr,"\nTesting read past end (LSb): ");
  {
    unsigned char dda[]={0,0,0,0};
    ogg_buffer lob={dda,8,0,{0}};
    ogg_reference lor={&lob,0,8,0};

    oggpack_readinit(&r,&lor);
    for(i=0;i<64;i++){
      if(oggpack_read(&r,1)<0){
        fprintf(stderr,"failed; got -1 prematurely.\n");
        exit(1);
      }
    }
    if(oggpack_look(&r,1)!=-1 ||
       oggpack_read(&r,1)!=-1){
      fprintf(stderr,"failed; read past end without -1.\n");
      exit(1);
    }
  }
  {
    unsigned char dda[]={0,0,0,0};
    ogg_buffer lob={dda,8,0,{0}};
    ogg_reference lor={&lob,0,8,0};
    unsigned long test;

    oggpack_readinit(&r,&lor);
    if((test=oggpack_read(&r,30))==0xffffffffUL || 
       (test=oggpack_read(&r,16))==0xffffffffUL){
      fprintf(stderr,"failed 2; got -1 prematurely.\n");
      exit(1);
    }
    
    if((test=oggpack_look(&r,18))==0xffffffffUL){
      fprintf(stderr,"failed 3; got -1 prematurely.\n");
      exit(1);
    }
    if((test=oggpack_look(&r,19))!=0xffffffffUL){
      fprintf(stderr,"failed; read past end without -1.\n");
      exit(1);
    }
    if((test=oggpack_look(&r,32))!=0xffffffffUL){
      fprintf(stderr,"failed; read past end without -1.\n");
      exit(1);
    }
  }
  fprintf(stderr,"ok.\n");

  /* now the scary shit: randomized testing */

  for(i=0;i<10000;i++){
    long j,count=0,count2=0,bitcount=0;
    unsigned long values[TESTWORDS];
    int len[TESTWORDS];
    unsigned char flat[4*TESTWORDS]; /* max possible needed size */

    memset(flat,0,sizeof(flat));
    fprintf(stderr,"\rRandomized testing (LSb)... (%ld)   ",10000-i);

    /* generate a list of words and lengths */
    /* write the required number of bits out to packbuffer */
    {
      long word=0;
      long bit=0;
      int k;

      for(j=0;j<TESTWORDS;j++){
	values[j]=rand();
	len[j]=(rand()%33);

	for(k=0;k<len[j];k++){
	  flat[word] |= ((values[j]>>k)&0x1)<<bit;
	  bit++;
	  bitcount++;
	  if(bit>7){
	    bit=0;
	    word++;
	  }
	}
      }
    }
    count2=(bitcount+7)>>3;

    /* construct random-length buffer chain from flat vector; random
       byte starting offset within the length of the vector */
    {
      ogg_reference *or=NULL,*orl=NULL;
      long pos=0;
      
      /* build buffer chain */
      while(count2){
        int ilen=(rand()%32),k;
        int ibegin=(rand()%32);
	

        if(ilen>count2)ilen=count2;

        if(or)
          orl=ogg_buffer_extend(orl,64);
        else
          or=orl=ogg_buffer_alloc(bs,64);

        orl->length=ilen;
        orl->begin=ibegin;

	for(k=0;k<ilen;k++)
	  orl->buffer->data[ibegin++]= flat[pos++];
	
        count2-=ilen;
      }

      if(ogg_buffer_length(or)!=(bitcount+7)/8){
        fprintf(stderr,"\nERROR: buffer length incorrect after build.\n");
        exit(1);
      }


      {
        int begin=0; //=(rand()%TESTWORDS);
        int ilen=(rand()%(TESTWORDS-begin));
        int bitoffset,bitcount=0;
        unsigned long temp;

        for(j=0;j<begin;j++)
          bitcount+=len[j];
        or=ogg_buffer_pretruncate(or,bitcount/8);
        bitoffset=bitcount%=8;
        for(;j<begin+ilen;j++)
          bitcount+=len[j];
        ogg_buffer_posttruncate(or,((bitcount+7)/8));

        if((count=ogg_buffer_length(or))!=(bitcount+7)/8){
          fprintf(stderr,"\nERROR: buffer length incorrect after truncate.\n");
          exit(1);
        }
        
        oggpack_readinit(&o,or);

        /* verify bit count */
        if(oggpack_bits(&o)!=0){
          fprintf(stderr,"\nERROR: Read bitcounter not zero!\n");
          exit(1);
        }
        if(oggpack_bytes(&o)!=0){
          fprintf(stderr,"\nERROR: Read bytecounter not zero!\n");
          exit(1);
        }

        bitcount=bitoffset;
        oggpack_read(&o,bitoffset);

        /* read and compare to original list */
        for(j=begin;j<begin+ilen;j++){
	  temp=oggpack_read(&o,len[j]);
          if(temp==0xffffffffUL){
            fprintf(stderr,"\nERROR: End of stream too soon! word: %ld,%d\n",
                    j-begin,ilen);
            exit(1);
          }
          if(temp!=(values[j]&mask[len[j]])){
            fprintf(stderr,"\nERROR: Incorrect read %lx != %lx, word %ld, len %d\n"
,
                    values[j]&mask[len[j]],temp,j-begin,len[j]);
            exit(1);
          }
          bitcount+=len[j];
          if(oggpack_bits(&o)!=bitcount){
            fprintf(stderr,"\nERROR: Read bitcounter %d != %ld!\n",
                    bitcount,oggpack_bits(&o));
            exit(1);
          }
          if(oggpack_bytes(&o)!=(bitcount+7)/8){
            fprintf(stderr,"\nERROR: Read bytecounter %d != %ld!\n",
                    (bitcount+7)/8,oggpack_bytes(&o));
            exit(1);
          }
          
        }
        _end_verify(count);
        
        /* look/adv version */
        oggpack_readinit(&o,or);
        bitcount=bitoffset;
        oggpack_adv(&o,bitoffset);

        /* read and compare to original list */
        for(j=begin;j<begin+ilen;j++){
	  temp=oggpack_look(&o,len[j]);

          if(temp==0xffffffffUL){
            fprintf(stderr,"\nERROR: End of stream too soon! word: %ld\n",
                    j-begin);
            exit(1);
          }
          if(temp!=(values[j]&mask[len[j]])){
            fprintf(stderr,"\nERROR: Incorrect look %lx != %lx, word %ld, len %d\n"
,
                    values[j]&mask[len[j]],temp,j-begin,len[j]);
            exit(1);
          }
	  oggpack_adv(&o,len[j]);
          bitcount+=len[j];
          if(oggpack_bits(&o)!=bitcount){
            fprintf(stderr,"\nERROR: Look/Adv bitcounter %d != %ld!\n",
                    bitcount,oggpack_bits(&o));
            exit(1);
          }
          if(oggpack_bytes(&o)!=(bitcount+7)/8){
            fprintf(stderr,"\nERROR: Look/Adv bytecounter %d != %ld!\n",
                    (bitcount+7)/8,oggpack_bytes(&o));
            exit(1);
          }
          
        }
        _end_verify2(count);

      }
      ogg_buffer_release(or);
    }
  }
  fprintf(stderr,"\rRandomized testing (LSb)... ok.   \n");

  return(0);
}  
#endif
