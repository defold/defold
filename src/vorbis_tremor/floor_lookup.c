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

 function: floor dB lookup

 ********************************************************************/

#include "os.h"

#ifdef _LOW_ACCURACY_
#  define XdB(n) ((((n)>>8)+1)>>1)
#else
#  define XdB(n) (n)
#endif

const ogg_int32_t FLOOR_fromdB_LOOKUP[256]={
  XdB(0x000000e5), XdB(0x000000f4), XdB(0x00000103), XdB(0x00000114),
  XdB(0x00000126), XdB(0x00000139), XdB(0x0000014e), XdB(0x00000163),
  XdB(0x0000017a), XdB(0x00000193), XdB(0x000001ad), XdB(0x000001c9),
  XdB(0x000001e7), XdB(0x00000206), XdB(0x00000228), XdB(0x0000024c),
  XdB(0x00000272), XdB(0x0000029b), XdB(0x000002c6), XdB(0x000002f4),
  XdB(0x00000326), XdB(0x0000035a), XdB(0x00000392), XdB(0x000003cd),
  XdB(0x0000040c), XdB(0x00000450), XdB(0x00000497), XdB(0x000004e4),
  XdB(0x00000535), XdB(0x0000058c), XdB(0x000005e8), XdB(0x0000064a),
  XdB(0x000006b3), XdB(0x00000722), XdB(0x00000799), XdB(0x00000818),
  XdB(0x0000089e), XdB(0x0000092e), XdB(0x000009c6), XdB(0x00000a69),
  XdB(0x00000b16), XdB(0x00000bcf), XdB(0x00000c93), XdB(0x00000d64),
  XdB(0x00000e43), XdB(0x00000f30), XdB(0x0000102d), XdB(0x0000113a),
  XdB(0x00001258), XdB(0x0000138a), XdB(0x000014cf), XdB(0x00001629),
  XdB(0x0000179a), XdB(0x00001922), XdB(0x00001ac4), XdB(0x00001c82),
  XdB(0x00001e5c), XdB(0x00002055), XdB(0x0000226f), XdB(0x000024ac),
  XdB(0x0000270e), XdB(0x00002997), XdB(0x00002c4b), XdB(0x00002f2c),
  XdB(0x0000323d), XdB(0x00003581), XdB(0x000038fb), XdB(0x00003caf),
  XdB(0x000040a0), XdB(0x000044d3), XdB(0x0000494c), XdB(0x00004e10),
  XdB(0x00005323), XdB(0x0000588a), XdB(0x00005e4b), XdB(0x0000646b),
  XdB(0x00006af2), XdB(0x000071e5), XdB(0x0000794c), XdB(0x0000812e),
  XdB(0x00008993), XdB(0x00009283), XdB(0x00009c09), XdB(0x0000a62d),
  XdB(0x0000b0f9), XdB(0x0000bc79), XdB(0x0000c8b9), XdB(0x0000d5c4),
  XdB(0x0000e3a9), XdB(0x0000f274), XdB(0x00010235), XdB(0x000112fd),
  XdB(0x000124dc), XdB(0x000137e4), XdB(0x00014c29), XdB(0x000161bf),
  XdB(0x000178bc), XdB(0x00019137), XdB(0x0001ab4a), XdB(0x0001c70e),
  XdB(0x0001e4a1), XdB(0x0002041f), XdB(0x000225aa), XdB(0x00024962),
  XdB(0x00026f6d), XdB(0x000297f0), XdB(0x0002c316), XdB(0x0002f109),
  XdB(0x000321f9), XdB(0x00035616), XdB(0x00038d97), XdB(0x0003c8b4),
  XdB(0x000407a7), XdB(0x00044ab2), XdB(0x00049218), XdB(0x0004de23),
  XdB(0x00052f1e), XdB(0x0005855c), XdB(0x0005e135), XdB(0x00064306),
  XdB(0x0006ab33), XdB(0x00071a24), XdB(0x0007904b), XdB(0x00080e20),
  XdB(0x00089422), XdB(0x000922da), XdB(0x0009bad8), XdB(0x000a5cb6),
  XdB(0x000b091a), XdB(0x000bc0b1), XdB(0x000c8436), XdB(0x000d5471),
  XdB(0x000e3233), XdB(0x000f1e5f), XdB(0x001019e4), XdB(0x001125c1),
  XdB(0x00124306), XdB(0x001372d5), XdB(0x0014b663), XdB(0x00160ef7),
  XdB(0x00177df0), XdB(0x001904c1), XdB(0x001aa4f9), XdB(0x001c603d),
  XdB(0x001e384f), XdB(0x00202f0f), XdB(0x0022467a), XdB(0x002480b1),
  XdB(0x0026dff7), XdB(0x002966b3), XdB(0x002c1776), XdB(0x002ef4fc),
  XdB(0x0032022d), XdB(0x00354222), XdB(0x0038b828), XdB(0x003c67c2),
  XdB(0x004054ae), XdB(0x004482e8), XdB(0x0048f6af), XdB(0x004db488),
  XdB(0x0052c142), XdB(0x005821ff), XdB(0x005ddc33), XdB(0x0063f5b0),
  XdB(0x006a74a7), XdB(0x00715faf), XdB(0x0078bdce), XdB(0x0080967f),
  XdB(0x0088f1ba), XdB(0x0091d7f9), XdB(0x009b5247), XdB(0x00a56a41),
  XdB(0x00b02a27), XdB(0x00bb9ce2), XdB(0x00c7ce12), XdB(0x00d4ca17),
  XdB(0x00e29e20), XdB(0x00f15835), XdB(0x0101074b), XdB(0x0111bb4e),
  XdB(0x01238531), XdB(0x01367704), XdB(0x014aa402), XdB(0x016020a7),
  XdB(0x017702c3), XdB(0x018f6190), XdB(0x01a955cb), XdB(0x01c4f9cf),
  XdB(0x01e269a8), XdB(0x0201c33b), XdB(0x0223265a), XdB(0x0246b4ea),
  XdB(0x026c9302), XdB(0x0294e716), XdB(0x02bfda13), XdB(0x02ed9793),
  XdB(0x031e4e09), XdB(0x03522ee4), XdB(0x03896ed0), XdB(0x03c445e2),
  XdB(0x0402efd6), XdB(0x0445ac4b), XdB(0x048cbefc), XdB(0x04d87013),
  XdB(0x05290c67), XdB(0x057ee5ca), XdB(0x05da5364), XdB(0x063bb204),
  XdB(0x06a36485), XdB(0x0711d42b), XdB(0x0787710e), XdB(0x0804b299),
  XdB(0x088a17ef), XdB(0x0918287e), XdB(0x09af747c), XdB(0x0a50957e),
  XdB(0x0afc2f19), XdB(0x0bb2ef7f), XdB(0x0c759034), XdB(0x0d44d6ca),
  XdB(0x0e2195bc), XdB(0x0f0cad0d), XdB(0x10070b62), XdB(0x1111aeea),
  XdB(0x122da66c), XdB(0x135c120f), XdB(0x149e24d9), XdB(0x15f525b1),
  XdB(0x176270e3), XdB(0x18e7794b), XdB(0x1a85c9ae), XdB(0x1c3f06d1),
  XdB(0x1e14f07d), XdB(0x200963d7), XdB(0x221e5ccd), XdB(0x2455f870),
  XdB(0x26b2770b), XdB(0x29363e2b), XdB(0x2be3db5c), XdB(0x2ebe06b6),
  XdB(0x31c7a55b), XdB(0x3503ccd4), XdB(0x3875c5aa), XdB(0x3c210f44),
  XdB(0x4009632b), XdB(0x4432b8cf), XdB(0x48a149bc), XdB(0x4d59959e),
  XdB(0x52606733), XdB(0x57bad899), XdB(0x5d6e593a), XdB(0x6380b298),
  XdB(0x69f80e9a), XdB(0x70dafda8), XdB(0x78307d76), XdB(0x7fffffff),
};
  
