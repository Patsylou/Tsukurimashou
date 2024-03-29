/* $Id: autowidth2.h 4020 2015-06-14 18:15:09Z mskala $ */
/* Copyright (C) 2009-2012  George Williams
 * Copyright (C) 2015  Matthew Skala
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _AUTOWIDTH2_H
#   define _AUTOWIDTH2_H

typedef struct aw_glyph {
   SplineChar *sc;
   DBounds bb;
   /*int zero_pos; */
   /*real min_y, max_y; */
   int imin_y, imax_y;		/* floor(bb.min_y/sub_height) */
   /* Allocate two arrays [min_y/sub_height,max_y/sub_height) */
   /*  normalize so that min_y=sub_height*floor(bb.miny/sub_height) */
   /*                    max_y=sub_height*ceil (bb.maxy/sub_height) */
   /* left[i] is the minimum x value any spline attains between min_y+i*sub_hight and min_y+(i+1)*sub_height */
   /*  in other words, it is a sampling of the left edge of the glyph */
   /* Similarly right is a sampling of the right edge */
   /* The left array is normalized so that the smallest leftmost extent is 0 */
   /*  (left side bearing set to zero) so all entries will be non-negative */
   /* Conversely the right array is normalized so that the greatest right extent is 0 */
   /*  so all entries will be non-positive */
   short *left;
   short *right;
   int lsb, rsb;		/* approximations we refine as we go along */
   int nlsb, nrsb;		/* next guess */
} AW_Glyph;

typedef struct aw_data {
   SplineFont *sf;
   FontViewBase *fv;
   int layer;
   /*uint32_t *script_list; */
   AW_Glyph *glyphs;
   int gcnt;
   int *visual_separation;	/* array[gcnt][gcnt] of my guess at the visual */
   /*  separation between all pairs of glyphs */
   /*  array[index_left_glyph*gcnt+index_right_glyph] */
   /* Only used for width, not kern */
   int sub_height;		/* Each glyph is divided into vertical chunks this high */
   int loop_cnt;		/* Number of times to iterate... */
   int desired_separation;
   int min_sidebearing, max_sidebearing;
   unsigned int normalize:1;
   real denom;
} AW_Data;

#endif /* _AUTOWIDTH2_H */
