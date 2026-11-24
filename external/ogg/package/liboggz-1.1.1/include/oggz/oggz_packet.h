/*
   Copyright (C) 2009 Annodex Association

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of the Annodex Association nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ASSOCIATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __OGGZ_PACKET_H__
#define __OGGZ_PACKET_H__

/** \file
 * Packet positioning
 *
 * oggz_packet derives from ogg_packet, and includes position information.
 */

/************************************************************
 * OggzPacket
 */

/**
 * The position of an oggz_packet.
 */
typedef struct {
  /**
   * Granulepos calculated by inspection of codec data.
   * -1 if unknown
   */
  ogg_int64_t calc_granulepos;

  /**
   * Byte offset of the start of the page on which this
   * packet begins.
   */
  oggz_off_t begin_page_offset;

  /**
   * Byte offset of the start of the page on which this
   * packet ends.
   */
  oggz_off_t end_page_offset;

  /** Number of pages this packet spans. */
  int pages;

  /**
   * Index into begin_page's lacing values
   * for the segment that begins this packet.
   * NB. if begin_page is continued then the first
   * of these packets will not be reported by
   * ogg_sync_packetout() after a seek.
   * -1 if unknown.
   */
  int begin_segment_index;
} oggz_position;

/**
 * An ogg_packet and its position in the stream.
 */
typedef struct {
  /** The ogg_packet structure, defined in <ogg/ogg.h> */
  ogg_packet op;

  /** Its position */
  oggz_position pos;
} oggz_packet;

#endif /* __OGGZ_PACKET_H__ */
