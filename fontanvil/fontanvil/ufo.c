/* $Id: ufo.c 4287 2015-10-20 11:54:06Z mskala $ */
/* Copyright (C) 2003-2012  George Williams
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

#include <fontanvil-config.h>

#include <utype.h>

#include "fontanvilvw.h"
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <locale.h>
#include <chardata.h>
#include <gfile.h>
#include <ustring.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

/* The UFO (Unified Font Object) format, http://unifiedfontobject.org/ */
/* Obsolete: http://just.letterror.com/ltrwiki/UnifiedFontObject */
/* is a directory containing a bunch of (mac style) property lists and another*/
/* directory containing glif files (and contents.plist). */

/* Property lists contain one <dict> element which contains <key> elements */
/*  each followed by an <integer>, <real>, <true/>, <false/> or <string> element, */
/*  or another <dict> */

/* UFO format 2.0 includes an adobe feature file "features.fea" and slightly */
/*  different/more tags in fontinfo.plist */

static char *buildname(char *basedir,char *sub) {
   char *fname=malloc(strlen(basedir) + strlen(sub) + 2);

   strcpy(fname, basedir);
   if (fname[strlen(fname) - 1] != '/')
      strcat(fname, "/");
   strcat(fname, sub);
   return (fname);
}

static void DumpHintsLib(AFILE *file,SplineChar *sc) {
   StemInfo *h;
   int has_hints=(sc != NULL && (sc->hstem != NULL || sc->vstem != NULL));

   if (has_hints) {
      /* Not officially part of the UFO/glif spec, but used by robofab */
      afprintf(file, "  <lib>\n");
      afprintf(file, "    <dict>\n");
      afprintf(file, "      <key>com.fontlab.hintData</key>\n");
      afprintf(file, "      <dict>\n");
      if (sc->hstem != NULL) {
	 afprintf(file, "\t<key>hhints</key>\n");
	 afprintf(file, "\t<array>\n");
	 for (h=sc->hstem; h != NULL; h=h->next) {
	    afprintf(file, "\t  <dict>\n");
	    afprintf(file, "\t    <key>position</key>");
	    afprintf(file, "\t    <integer>%d</integer>\n",
		    (int) rint(h->start));
	    afprintf(file, "\t    <key>width</key>");
	    afprintf(file, "\t    <integer>%d</integer>\n",
		    (int) rint(h->width));
	    afprintf(file, "\t  </dict>\n");
	 }
	 afprintf(file, "\t</array>\n");
      }
      if (sc->vstem != NULL) {
	 afprintf(file, "\t<key>vhints</key>\n");
	 afprintf(file, "\t<array>\n");
	 for (h=sc->vstem; h != NULL; h=h->next) {
	    afprintf(file, "\t  <dict>\n");
	    afprintf(file, "\t    <key>position</key>\n");
	    afprintf(file, "\t    <integer>%d</integer>\n",
		    (int) rint(h->start));
	    afprintf(file, "\t    <key>width</key>\n");
	    afprintf(file, "\t    <integer>%d</integer>\n",
		    (int) rint(h->width));
	    afprintf(file, "\t  </dict>\n");
	 }
	 afprintf(file, "\t</array>\n");
      }
      afprintf(file, "      </dict>\n");
      if (sc != NULL) {
	 afprintf(file, "    </dict>\n");
	 afprintf(file, "  </lib>\n");
      }
   }
}

/* ************************************************************************** */
/* ****************************   GLIF Output    **************************** */
/* ************************************************************************** */
static int refcomp(const void *_r1,const void *_r2) {
   const RefChar *ref1=*(RefChar * const *) _r1;
   const RefChar *ref2=*(RefChar * const *) _r2;

   return (strcmp(ref1->sc->name, ref2->sc->name));
}

static int _GlifDump(AFILE *glif,SplineChar *sc,int layer) {
   struct altuni *altuni;
   int isquad=sc->layers[layer].order2;
   SplineSet *spl;
   SplinePoint *sp;
   AnchorPoint *ap;
   RefChar *ref;
   int err;

   if (glif==NULL)
      return (false);

   afprintf(glif, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
   /* No DTD for these guys??? */
   afprintf(glif, "<glyph name=\"%s\" format=\"1\">\n", sc->name);
   if (sc->parent->hasvmetrics)
      afprintf(glif, "  <advance width=\"%d\" height=\"%d\"/>\n", sc->width,
	      sc->vwidth);
   else
      afprintf(glif, "  <advance width=\"%d\"/>\n", sc->width);
   if (sc->unicodeenc != -1)
      afprintf(glif, "  <unicode hex=\"%04X\"/>\n", sc->unicodeenc);
   for (altuni=sc->altuni; altuni != NULL; altuni=altuni->next)
      if (altuni->vs==-1 && altuni->fid==0)
	 afprintf(glif, "  <unicode hex=\"%04x\"/>\n", altuni->unienc);

   if (sc->layers[layer].refs != NULL || sc->layers[layer].splines != NULL) {
      afprintf(glif, "  <outline>\n");
      /* RoboFab outputs components in alphabetic (case sensitive) order */
      /*  I've been asked to do that too */
      if (sc->layers[layer].refs != NULL) {
	 RefChar **refs;
	 int i, cnt;

	 for (cnt=0, ref=sc->layers[layer].refs; ref != NULL;
	      ref=ref->next)
	    if (SCWorthOutputting(ref->sc))
	       ++cnt;
	 refs=malloc(cnt * sizeof(RefChar *));
	 for (cnt=0, ref=sc->layers[layer].refs; ref != NULL;
	      ref=ref->next)
	    if (SCWorthOutputting(ref->sc))
	       refs[cnt++]=ref;
	 if (cnt > 1)
	    qsort(refs, cnt, sizeof(RefChar *), refcomp);
	 for (i=0; i < cnt; ++i) {
	    ref=refs[i];
	    afprintf(glif, "    <component base=\"%s\"", ref->sc->name);
	    if (ref->transform[0] != 1)
	       afprintf(glif, " xScale=\"%g\"", (double) ref->transform[0]);
	    if (ref->transform[3] != 1)
	       afprintf(glif, " yScale=\"%g\"", (double) ref->transform[3]);
	    if (ref->transform[1] != 0)
	       afprintf(glif, " xyScale=\"%g\"", (double) ref->transform[1]);
	    if (ref->transform[2] != 0)
	       afprintf(glif, " yxScale=\"%g\"", (double) ref->transform[2]);
	    if (ref->transform[4] != 0)
	       afprintf(glif, " xOffset=\"%g\"", (double) ref->transform[4]);
	    if (ref->transform[5] != 0)
	       afprintf(glif, " yOffset=\"%g\"", (double) ref->transform[5]);
	    afprintf(glif, "/>\n");
	 }
	 free(refs);
      }
      for (ap=sc->anchor; ap != NULL; ap=ap->next) {
	 int ismark=(ap->type==at_mark || ap->type==at_centry);

	 afprintf(glif, "    <contour>\n");
	 afprintf(glif,
		 "      <point x=\"%g\" y=\"%g\" type=\"move\" name=\"%s%s\"/>\n",
		 ap->me.x, ap->me.y, ismark ? "_" : "", ap->anchor->name);
	 afprintf(glif, "    </contour>\n");
      }
      for (spl=sc->layers[layer].splines; spl != NULL; spl=spl->next) {
	 afprintf(glif, "    <contour>\n");
	 for (sp=spl->first; sp != NULL;) {
	    /* Undocumented fact: If a contour contains a series of off-curve points with no on-curve then treat as quadratic even if no qcurve */
	    // We write the next on-curve point.
	    if (!isquad || sp->ttfindex != 0xffff || !SPInterpolate(sp)
		|| sp->pointtype != pt_curve || sp->name != NULL)
	       afprintf(glif,
		       "      <point x=\"%g\" y=\"%g\" type=\"%s\"%s%s%s%s/>\n",
		       (double) sp->me.x, (double) sp->me.y,
		       sp->prev ==
		       NULL ? "move" : sp->prev->
		       knownlinear ? "line" : isquad ? "qcurve" : "curve",
		       sp->pointtype != pt_corner ? " smooth=\"yes\"" : "",
		       sp->name ? " name=\"" : "", sp->name ? sp->name : "",
		       sp->name ? "\"" : "");
	    if (sp->next==NULL)
	       break;
	    // We write control points.
	    if (!sp->next->knownlinear)
	       afprintf(glif, "      <point x=\"%g\" y=\"%g\"/>\n",
		       (double) sp->nextcp.x, (double) sp->nextcp.y);
	    sp=sp->next->to;
	    if (!isquad && !sp->prev->knownlinear)
	       afprintf(glif, "      <point x=\"%g\" y=\"%g\"/>\n",
		       (double) sp->prevcp.x, (double) sp->prevcp.y);
	    if (sp==spl->first)
	       break;
	 }
	 afprintf(glif, "    </contour>\n");
      }
      afprintf(glif, "  </outline>\n");
   }
   DumpHintsLib(glif, sc);
   afprintf(glif, "</glyph>\n");
   err=aferror(glif);
   if (afclose(glif))
      err=true;
   return (!err);
}

static int GlifDump(char *glyphdir,char *gfname,SplineChar *sc,int layer) {
   char *gn=buildname(glyphdir, gfname);
   AFILE *glif=afopen(gn, "w");
   int ret=_GlifDump(glif, sc, layer);

   free(gn);
   return (ret);
}

int _ExportGlif(AFILE *glif, SplineChar * sc, int layer) {
   return (_GlifDump(glif, sc, layer));
}

/* ************************************************************************** */
/* ****************************    UFO Output    **************************** */
/* ************************************************************************** */

static void PListOutputHeader(AFILE *plist) {
   afprintf(plist, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
   afprintf(plist,
	   "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n");
   afprintf(plist, "<plist version=\"1.0\">\n");
   afprintf(plist, "    <dict>\n");
}

static AFILE *PListCreate(char *basedir,char *sub) {
   char *fname=buildname(basedir, sub);
   AFILE *plist=afopen(fname, "w");

   free(fname);
   if (plist==NULL)
      return (NULL);
   PListOutputHeader(plist);
   return (plist);
}

static int PListOutputTrailer(AFILE *plist) {
   int ret=true;

   afprintf(plist, "    </dict>\n");
   afprintf(plist, "</plist>\n");
   if (aferror(plist))
      ret=false;
   if (afclose(plist))
      ret=false;
   return (ret);
}

static void PListOutputInteger(AFILE *plist,char *key,int value) {
   afprintf(plist, "\t<key>%s</key>\n", key);
   afprintf(plist, "\t<integer>%d</integer>\n", value);
}

static void PListOutputReal(AFILE *plist,char *key,double value) {
   afprintf(plist, "\t<key>%s</key>\n", key);
   afprintf(plist, "\t<real>%g</real>\n", value);
}

static void PListOutputBoolean(AFILE *plist,char *key,int value) {
   afprintf(plist, "\t<key>%s</key>\n", key);
   afprintf(plist, value ? "\t<true/>\n" : "\t<false/>\n");
}

static void PListOutputDate(AFILE *plist,char *key,time_t timestamp) {
/* openTypeHeadCreated=string format as \"YYYY/MM/DD HH:MM:SS\".	*/
/* \"YYYY/MM/DD\" is year/month/day. The month is in the range 1-12 and	*/
/* the day is in the range 1-end of month.				*/
/*  \"HH:MM:SS\" is hour:minute:second. The hour is in the range 0:23.	*/
/* Minutes and seconds are in the range 0-59.				*/
   struct tm *tm=gmtime(&timestamp);

   afprintf(plist, "\t<key>%s</key>\n", key);
   afprintf(plist, "\t<string>%4d/%02d/%02d %02d:%02d:%02d</string>\n",
	   tm->tm_year + 1900, tm->tm_mon + 1,
	   tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static void PListOutputString(AFILE *plist,char *key,char *value) {
   if (value==NULL)
      value="";
   afprintf(plist, "\t<key>%s</key>\n", key);
   afprintf(plist, "\t<string>");
   while (*value) {
      if (*value=='<')
	 afprintf(plist, "&lt;");
      else if (*value=='&')
	 afprintf(plist, "&amp;");
      else
	 aputc(*value, plist);
      ++value;
   }
   afprintf(plist, "</string>\n");
}

static void PListOutputNameString(AFILE *plist,char *key,SplineFont *sf,
				  int strid) {
   char *value=NULL, *nonenglish=NULL, *freeme=NULL;
   struct ttflangname *nm;

   for (nm=sf->names; nm != NULL; nm=nm->next) {
      if (nm->names[strid] != NULL) {
	 nonenglish=nm->names[strid];
	 if (nm->lang==0x409) {
	    value=nm->names[strid];
	    break;
	 }
      }
   }
   if (value==NULL && strid==ttf_version && sf->version != NULL)
      value=freeme=strconcat("Version ", sf->version);
   if (value==NULL)
      value=nonenglish;
   if (value != NULL)
      PListOutputString(plist, key, value);
   free(freeme);
}

static void PListOutputIntArray(AFILE *plist,char *key,char *entries,
				int len) {
   int i;

   afprintf(plist, "\t<key>%s</key>\n", key);
   afprintf(plist, "\t<array>\n");
   for (i=0; i < len; ++i)
      afprintf(plist, "\t\t<integer>%d</integer>\n", entries[i]);
   afprintf(plist, "\t</array>\n");
}

static void PListOutputPrivateArray(AFILE *plist,char *key,
				    struct psdict *private) {
   char *value;
   int skipping;

   if (private==NULL)
      return;
   value=PSDictHasEntry(private, key);
   if (value==NULL)
      return;

   while (*value==' ' || *value=='[')
      ++value;

   afprintf(plist, "\t<key>postscript%s</key>\n", key);
   afprintf(plist, "\t<array>\n");
   while (1) {
      afprintf(plist, "\t\t<integer>");
      skipping=0;
      while (*value != ']' && *value != '\0' && *value != ' ') {
	 if (*value=='.' || skipping) {
	    skipping=true;
	    ++value;
	 } else
	    aputc(*value++,plist);
      }
      afprintf(plist,"</integer>\n");
      while (*value==' ')
	 ++value;
      if (*value==']' || *value=='\0')
	 break;
   }
   afprintf(plist, "\t</array>\n");
}

static void PListOutputPrivateThing(AFILE *plist,char *key,
				    struct psdict *private, char *type) {
   char *value;

   if (private==NULL)
      return;
   value=PSDictHasEntry(private, key);
   if (value==NULL)
      return;

   while (*value==' ' || *value=='[')
      ++value;

   afprintf(plist, "\t<key>postscript%s</key>\n", key);
   afprintf(plist, "\t<%s>%s</%s>\n", type, value, type);
}

static int UFOOutputMetaInfo(char *basedir,SplineFont *sf) {
   AFILE *plist=PListCreate(basedir, "metainfo.plist");

   if (plist==NULL)
      return (false);
   PListOutputString(plist, "creator", "net.GitHub.FontAnvil");
#ifdef Version_1
   PListOutputInteger(plist, "formatVersion", 1);
#else
   PListOutputInteger(plist, "formatVersion", 2);
#endif
   return (PListOutputTrailer(plist));
}

static int UFOOutputFontInfo(char *basedir,SplineFont *sf,int layer) {
   AFILE *plist=PListCreate(basedir, "fontinfo.plist");
   DBounds bb;
   double test;

   if (plist==NULL)
      return (false);
/* Same keys in both formats */
   PListOutputString(plist, "familyName",
		     sf->familyname_with_timestamp ? sf->
		     familyname_with_timestamp : sf->familyname);
   PListOutputString(plist, "styleName", SFGetModifiers(sf));
   PListOutputString(plist, "copyright", sf->copyright);
   PListOutputNameString(plist, "trademark", sf, ttf_trademark);
   PListOutputInteger(plist, "unitsPerEm", sf->ascent + sf->descent);
   test=SFXHeight(sf, layer, true);
   if (test > 0)
      PListOutputInteger(plist, "xHeight", (int) rint(test));
   test=SFCapHeight(sf, layer, true);
   if (test > 0)
      PListOutputInteger(plist, "capHeight", (int) rint(test));
   if (sf->ufo_ascent==0)
      PListOutputInteger(plist, "ascender", sf->ascent);
   else if (sf->ufo_ascent==floor(sf->ufo_ascent))
      PListOutputInteger(plist, "ascender", sf->ufo_ascent);
   else
      PListOutputReal(plist, "ascender", sf->ufo_ascent);
   if (sf->ufo_descent==0)
      PListOutputInteger(plist, "descender", -sf->descent);
   else if (sf->ufo_descent==floor(sf->ufo_descent))
      PListOutputInteger(plist, "descender", sf->ufo_descent);
   else
      PListOutputReal(plist, "descender", sf->ufo_descent);
   PListOutputReal(plist, "italicAngle", sf->italicangle);
#ifdef Version_1
   PListOutputString(plist, "fullName", sf->fullname);
   PListOutputString(plist, "fontName", sf->fontname);
   /* FontAnvil does not maintain a menuname except possibly in the ttfnames section where there are many different languages of it */
   PListOutputString(plist, "weightName", sf->weight);
   /* No longer in the spec. Was it ever? Did I get this wrong? */
   /* PListOutputString(plist,"curveType",sf->layers[layer].order2 ? "Quadratic" : "Cubic"); */
#else
   PListOutputString(plist, "note", sf->comments);
   PListOutputDate(plist, "openTypeHeadCreated", sf->creationtime);
   SplineFontFindBounds(sf, &bb);
   if (sf->pfminfo.hheadset) {
      if (sf->pfminfo.hheadascent_add)
	 PListOutputInteger(plist, "openTypeHheaAscender",
			    bb.maxy + sf->pfminfo.hhead_ascent);
      else
	 PListOutputInteger(plist, "openTypeHheaAscender",
			    sf->pfminfo.hhead_ascent);
      if (sf->pfminfo.hheaddescent_add)
	 PListOutputInteger(plist, "openTypeHheaDescender",
			    bb.miny + sf->pfminfo.hhead_descent);
      else
	 PListOutputInteger(plist, "openTypeHheaDescender",
			    sf->pfminfo.hhead_descent);
      PListOutputInteger(plist, "openTypeHheaLineGap", sf->pfminfo.linegap);
   }
   PListOutputNameString(plist, "openTypeNameDesigner", sf, ttf_designer);
   PListOutputNameString(plist, "openTypeNameDesignerURL", sf,
			 ttf_designerurl);
   PListOutputNameString(plist, "openTypeNameManufacturer", sf,
			 ttf_manufacturer);
   PListOutputNameString(plist, "openTypeNameManufacturerURL", sf,
			 ttf_venderurl);
   PListOutputNameString(plist, "openTypeNameLicense", sf, ttf_license);
   PListOutputNameString(plist, "openTypeNameLicenseURL", sf, ttf_licenseurl);
   PListOutputNameString(plist, "openTypeNameVersion", sf, ttf_version);
   PListOutputNameString(plist, "openTypeNameUniqueID", sf, ttf_uniqueid);
   PListOutputNameString(plist, "openTypeNameDescription", sf,
			 ttf_descriptor);
   PListOutputNameString(plist, "openTypeNamePreferedFamilyName", sf,
			 ttf_preffamilyname);
   PListOutputNameString(plist, "openTypeNamePreferedSubfamilyName", sf,
			 ttf_prefmodifiers);
   PListOutputNameString(plist, "openTypeNameCompatibleFullName", sf,
			 ttf_compatfull);
   PListOutputNameString(plist, "openTypeNameSampleText", sf, ttf_sampletext);
   PListOutputNameString(plist, "openTypeWWSFamilyName", sf, ttf_wwsfamily);
   PListOutputNameString(plist, "openTypeWWSSubfamilyName", sf,
			 ttf_wwssubfamily);
   if (sf->pfminfo.panose_set)
      PListOutputIntArray(plist, "openTypeOS2Panose", sf->pfminfo.panose, 10);
   if (sf->pfminfo.pfmset) {
      char vendor[8], fc[2];

      PListOutputInteger(plist, "openTypeOS2WidthClass", sf->pfminfo.width);
      PListOutputInteger(plist, "openTypeOS2WeightClass", sf->pfminfo.weight);
      memcpy(vendor, sf->pfminfo.os2_vendor, 4);
      vendor[4]=0;
      PListOutputString(plist, "openTypeOS2VendorID", vendor);
      fc[0]=sf->pfminfo.os2_family_class >> 8;
      fc[1]=sf->pfminfo.os2_family_class & 0xff;
      PListOutputIntArray(plist, "openTypeOS2FamilyClass", fc, 2);
      if (sf->pfminfo.fstype != -1) {
	 int fscnt, i;
	 char fstype[16];

	 for (i=fscnt=0; i < 16; ++i)
	    if (sf->pfminfo.fstype & (1 << i))
	       fstype[fscnt++]=i;
	 if (fscnt != 0)
	    PListOutputIntArray(plist, "openTypeOS2Type", fstype, fscnt);
      }
      if (sf->pfminfo.typoascent_add)
	 PListOutputInteger(plist, "openTypeOS2TypoAscender",
			    sf->ascent + sf->pfminfo.os2_typoascent);
      else
	 PListOutputInteger(plist, "openTypeOS2TypoAscender",
			    sf->pfminfo.os2_typoascent);
      if (sf->pfminfo.typodescent_add)
	 PListOutputInteger(plist, "openTypeOS2TypoDescender",
			    sf->descent + sf->pfminfo.os2_typodescent);
      else
	 PListOutputInteger(plist, "openTypeOS2TypoDescender",
			    sf->pfminfo.os2_typodescent);
      PListOutputInteger(plist, "openTypeOS2TypoLineGap",
			 sf->pfminfo.os2_typolinegap);
      if (sf->pfminfo.winascent_add)
	 PListOutputInteger(plist, "openTypeOS2WinAscent",
			    bb.maxy + sf->pfminfo.os2_winascent);
      else
	 PListOutputInteger(plist, "openTypeOS2WinAscent",
			    sf->pfminfo.os2_winascent);
      if (sf->pfminfo.windescent_add)
	 PListOutputInteger(plist, "openTypeOS2WinDescent",
			    bb.miny + sf->pfminfo.os2_windescent);
      else
	 PListOutputInteger(plist, "openTypeOS2WinDescent",
			    sf->pfminfo.os2_windescent);
   }
   if (sf->pfminfo.subsuper_set) {
      PListOutputInteger(plist, "openTypeOS2SubscriptXSize",
			 sf->pfminfo.os2_subxsize);
      PListOutputInteger(plist, "openTypeOS2SubscriptYSize",
			 sf->pfminfo.os2_subysize);
      PListOutputInteger(plist, "openTypeOS2SubscriptXOffset",
			 sf->pfminfo.os2_subxoff);
      PListOutputInteger(plist, "openTypeOS2SubscriptYOffset",
			 sf->pfminfo.os2_subyoff);
      PListOutputInteger(plist, "openTypeOS2SuperscriptXSize",
			 sf->pfminfo.os2_supxsize);
      PListOutputInteger(plist, "openTypeOS2SuperscriptYSize",
			 sf->pfminfo.os2_supysize);
      PListOutputInteger(plist, "openTypeOS2SuperscriptXOffset",
			 sf->pfminfo.os2_supxoff);
      PListOutputInteger(plist, "openTypeOS2SuperscriptYOffset",
			 sf->pfminfo.os2_supyoff);
      PListOutputInteger(plist, "openTypeOS2StrikeoutSize",
			 sf->pfminfo.os2_strikeysize);
      PListOutputInteger(plist, "openTypeOS2StrikeoutPosition",
			 sf->pfminfo.os2_strikeypos);
   }
   if (sf->pfminfo.vheadset)
      PListOutputInteger(plist, "openTypeVheaTypoLineGap",
			 sf->pfminfo.vlinegap);
   if (sf->pfminfo.hasunicoderanges) {
      char ranges[128];
      int i, j, c=0;

      for (i=0; i < 4; i++)
	 for (j=0; j < 32; j++)
	    if (sf->pfminfo.unicoderanges[i] & (1 << j))
	       ranges[c++]=i * 32 + j;
      if (c != 0)
	 PListOutputIntArray(plist, "openTypeOS2UnicodeRanges", ranges, c);
   }
   if (sf->pfminfo.hascodepages) {
      char pages[64];
      int i, j, c=0;

      for (i=0; i < 2; i++)
	 for (j=0; j < 32; j++)
	    if (sf->pfminfo.codepages[i] & (1 << j))
	       pages[c++]=i * 32 + j;
      if (c != 0)
	 PListOutputIntArray(plist, "openTypeOS2CodePageRanges", pages, c);
   }
   PListOutputString(plist, "postscriptFontName", sf->fontname);
   PListOutputString(plist, "postscriptFullName", sf->fullname);
   PListOutputString(plist, "postscriptWeightName", sf->weight);
   /* Spec defines a "postscriptSlantAngle" but I don't think postscript does */
   /* PS does define an italicAngle, but presumably that's the general italic */
   /* angle we output earlier */
   /* UniqueID is obsolete */
   PListOutputInteger(plist, "postscriptUnderlineThickness", sf->uwidth);
   PListOutputInteger(plist, "postscriptUnderlinePosition", sf->upos);
   if (sf->private != NULL) {
      char *pt;

      PListOutputPrivateArray(plist, "BlueValues", sf->private);
      PListOutputPrivateArray(plist, "OtherBlues", sf->private);
      PListOutputPrivateArray(plist, "FamilyBlues", sf->private);
      PListOutputPrivateArray(plist, "FamilyOtherBlues", sf->private);
      PListOutputPrivateArray(plist, "StemSnapH", sf->private);
      PListOutputPrivateArray(plist, "StemSnapV", sf->private);
      PListOutputPrivateThing(plist, "BlueFuzz", sf->private, "integer");
      PListOutputPrivateThing(plist, "BlueShift", sf->private, "integer");
      PListOutputPrivateThing(plist, "BlueScale", sf->private, "real");
      if ((pt=PSDictHasEntry(sf->private, "ForceBold")) != NULL &&
	  strstr(pt, "true") != NULL)
	 PListOutputBoolean(plist, "postscriptForceBold", true);
   }
   if (sf->fondname != NULL)
      PListOutputString(plist, "macintoshFONDName", sf->fondname);
#endif
   return (PListOutputTrailer(plist));
}

static int UFOOutputGroups(char *basedir,SplineFont *sf) {
   AFILE *plist=PListCreate(basedir, "groups.plist");

   if (plist==NULL)
      return (false);
   /* These don't act like fontanvil's groups. There are comments that this */
   /*  could be used for defining classes (kerning classes, etc.) but no */
   /*  resolution saying that the actually are. */
   /* Should I omit a file I don't use? Or leave it blank? */
   return (PListOutputTrailer(plist));
}

static void KerningPListOutputGlyph(AFILE *plist,char *key,KernPair *kp) {
   afprintf(plist, "\t<key>%s</key>\n", key);
   afprintf(plist, "\t<dict>\n");
   while (kp != NULL) {
      if (kp->off != 0 && SCWorthOutputting(kp->sc)) {
	 afprintf(plist, "\t    <key>%s</key>\n", kp->sc->name);
	 afprintf(plist, "\t    <integer>%d</integer>\n", kp->off);
      }
      kp=kp->next;
   }
   afprintf(plist, "\t</dict>\n");
}

static int UFOOutputKerning(char *basedir,SplineFont *sf) {
   AFILE *plist=PListCreate(basedir, "kerning.plist");
   SplineChar *sc;
   int i;

   if (plist==NULL)
      return (false);
   /* There is some muttering about how to do kerning by classes, but no */
   /*  resolution to those thoughts. So I ignore the issue */
   for (i=0; i < sf->glyphcnt; ++i)
      if (SCWorthOutputting(sc=sf->glyphs[i]) && sc->kerns != NULL)
	 KerningPListOutputGlyph(plist, sc->name, sc->kerns);
   return (PListOutputTrailer(plist));
}

static int UFOOutputVKerning(char *basedir,SplineFont *sf) {
   AFILE *plist;
   SplineChar *sc;
   int i;

   for (i=sf->glyphcnt - 1; i >= 0; --i)
      if (SCWorthOutputting(sc=sf->glyphs[i]) && sc->vkerns != NULL)
	 break;
   if (i < 0)
      return (true);

   plist=PListCreate(basedir, "vkerning.plist");
   if (plist==NULL)
      return (false);
   for (i=0; i < sf->glyphcnt; ++i)
      if ((sc=sf->glyphs[i]) != NULL && sc->vkerns != NULL)
	 KerningPListOutputGlyph(plist, sc->name, sc->vkerns);
   return (PListOutputTrailer(plist));
}

static int UFOOutputLib(char *basedir,SplineFont *sf) {
   return (true);
}

#ifndef Version_1
static int UFOOutputFeatures(char *basedir,SplineFont *sf) {
   char *fname=buildname(basedir, "features.fea");
   AFILE *feats=afopen(fname, "w");
   int err;

   free(fname);
   if (feats==NULL)
      return (false);
   FeatDumpFontLookups(feats, sf);
   err=aferror(feats);
   afclose(feats);
   return (!err);
}
#endif

int WriteUFOFont(char *basedir, SplineFont *sf, enum fontformat ff,
		 int flags, EncMap * map, int layer) {
   char *foo=malloc(strlen(basedir) + 20), *glyphdir, *gfname;
   int err;
   AFILE *plist;
   int i;
   SplineChar *sc;

   /* Clean it out, if it exists */
   sprintf(foo, "rm -rf %s", basedir);
   system(foo);
   free(foo);

   /* Create it */
   GFileMkDir(basedir);

   err=!UFOOutputMetaInfo(basedir, sf);
   err |= !UFOOutputFontInfo(basedir, sf, layer);
   err |= !UFOOutputGroups(basedir, sf);
   err |= !UFOOutputKerning(basedir, sf);
   err |= !UFOOutputVKerning(basedir, sf);
   err |= !UFOOutputLib(basedir, sf);
#ifndef Version_1
   err |= !UFOOutputFeatures(basedir, sf);
#endif

   if (err)
      return (false);

   glyphdir=buildname(basedir, "glyphs");
   GFileMkDir(glyphdir);

   plist=PListCreate(glyphdir, "contents.plist");
   if (plist==NULL) {
      free(glyphdir);
      return (false);
   }

   for (i=0; i < sf->glyphcnt; ++i)
      if (SCWorthOutputting(sc=sf->glyphs[i])) {
	 char *start, *gstart;

	 gstart=gfname=malloc(2 * strlen(sc->name) + 20);
	 start=sc->name;
	 if (*start=='.') {
	    *gstart++='_';
	    ++start;
	 }
	 while (*start) {
	    /* Now the spec has a very complicated algorithm for producing a */
	    /*  filename, dividing the glyph name into chunks at every period */
	    /*  and then again at every underscore, and then adding an under- */
	    /*  score at the end of a chunk if the chunk begins with a capital */
	    /* BUT... */
	    /* That's not what RoboFAB does. It simply adds an underscore after */
	    /*  every capital letter. Much easier. And since people have */
	    /*  complained that I follow the spec, let's not. */
	    *gstart++=*start;
	    if (isupper(*start++))
	       *gstart++='_';
	 }
	 strcpy(gstart, ".glif");
	 PListOutputString(plist, sc->name, gfname);
	 err |= !GlifDump(glyphdir, gfname, sc, layer);
	 free(gfname);
      }
   free(glyphdir);
   err |= !PListOutputTrailer(plist);
   return (!err);
}

/* ************************************************************************** */
/* *****************************    UFO Input    **************************** */
/* ************************************************************************** */

static char *get_thingy(AFILE *file,char *buffer,char *tag) {
   int ch;
   char *pt;

   while (1) {
      while ((ch=agetc(file)) != '<' && ch != EOF);
      if (ch==EOF)
	 return (NULL);
      while ((ch=agetc(file)) != EOF && isspace(ch));
      pt=tag;
      while (ch==*pt || tolower(ch)==*pt) {
	 ++pt;
	 ch=agetc(file);
      }
      if (*pt=='\0')
	 continue;
      if (ch==EOF)
	 return (NULL);
      while (isspace(ch))
	 ch=agetc(file);
      if (ch != '>')
	 continue;
      pt=buffer;
      while ((ch=agetc(file)) != '<' && ch != EOF && pt < buffer + 1000)
	 *pt++=ch;
      *pt='\0';
      return (buffer);
   }
}

char **NamesReadUFO(char *filename) {
   char *fn=buildname(filename, "fontinfo.plist");
   AFILE *info=afopen(fn, "r");
   char buffer[1024];
   char **ret;

   free(fn);
   if (info==NULL)
      return (NULL);
   while (get_thingy(info, buffer, "key") != NULL) {
      if (strcmp(buffer, "fontName") != 0) {
	 if (get_thingy(info, buffer, "string") != NULL) {
	    ret=calloc(2, sizeof(char *));
	    ret[0]=fastrdup(buffer);
	    afclose(info);
	    return (ret);
	 }
	 afclose(info);
	 return (NULL);
      }
   }
   afclose(info);
   return (NULL);
}

#ifdef _NO_LIBXML
int HasUFO(void) {
   return (false);
}

SplineFont *SFReadUFO(char *filename, int flags) {
   return (NULL);
}

SplineSet *SplinePointListInterpretGlif(SplineFont *sf, char *filename,
					char *memory, int memlen, int em_size,
					int ascent, int is_stroked) {
   return (NULL);
}
#else

#   ifndef HAVE_ICONV_H
#      undef iconv
#      undef iconv_t
#      undef iconv_open
#      undef iconv_close
#   endif

#   undef extended		/* used in xlink.h */
#   include <libxml/parser.h>

#   ifdef __CygWin
/*
 * FIXME: Check whether this kludge is still (a) necessary, (b)
 * functional. At least (a) seems unlikely to have remained true over
 * time.
 */
/* Nasty kludge, but xmlFree doesn't work on cygwin (or I can't get it to) */
#      define xmlFree free
#   endif

static int libxml_init_base() {
   return (true);
}

static xmlNodePtr FindNode(xmlNodePtr kids,char *name) {
   while (kids != NULL) {
      if (xmlStrcmp(kids->name, (const xmlChar *) name)==0)
	 return (kids);
      kids=kids->next;
   }
   return (NULL);
}

static StemInfo *GlifParseHints(xmlDocPtr doc,xmlNodePtr dict,
				char *hinttype) {
   StemInfo *head=NULL, *last=NULL, *h;
   xmlNodePtr keys, array, kids, poswidth, temp;
   double pos, width;

   for (keys=dict->children; keys != NULL; keys=keys->next) {
      if (xmlStrcmp(keys->name, (const xmlChar *) "key")==0) {
	 char *keyname =
	    (char *) xmlNodeListGetString(doc, keys->children, true);
	 int found=strcmp(keyname, hinttype)==0;

	 free(keyname);
	 if (found) {
	    for (array=keys->next; array != NULL; array=array->next) {
	       if (xmlStrcmp(array->name, (const xmlChar *) "array")==0)
		  break;
	    }
	    if (array != NULL) {
	       for (kids=array->children; kids != NULL; kids=kids->next) {
		  if (xmlStrcmp(kids->name, (const xmlChar *) "dict")==0) {
		     pos=-88888888;
		     width=0;
		     for (poswidth=kids->children; poswidth != NULL;
			  poswidth=poswidth->next) {
			if (xmlStrcmp(poswidth->name, (const xmlChar *) "key")
			   ==0) {
			   char *keyname2 =
			      (char *) xmlNodeListGetString(doc,
							    poswidth->
							    children, true);
			   int ispos =
			      strcmp(keyname2, "position")==0, iswidth =
			      strcmp(keyname2, "width")==0;
			   double value;

			   free(keyname2);
			   for (temp=poswidth->next; temp != NULL;
				temp=temp->next) {
			      if (xmlStrcmp
				  (temp->name, (const xmlChar *) "text") != 0)
				 break;
			   }
			   if (temp != NULL) {
			      char *valname =
				 (char *) xmlNodeListGetString(doc,
							       temp->children,
							       true);
			      if (xmlStrcmp
				  (temp->name,
				   (const xmlChar *) "integer")==0)
				 value=strtol(valname, NULL, 10);
			      else
				 if (xmlStrcmp
				     (temp->name,
				      (const xmlChar *) "real")==0)
				 value=strtod(valname, NULL);
			      else
				 ispos=iswidth=false;
			      free(valname);
			      if (ispos)
				 pos=value;
			      else if (iswidth)
				 width=value;
			      poswidth=temp;
			   }
			}
		     }
		     if (pos != -88888888 && width != 0) {
			h=chunkalloc(sizeof(StemInfo));
			h->start=pos;
			h->width=width;
			if (width==-20 || width==-21)
			   h->ghost=true;
			if (head==NULL)
			   head=last=h;
			else {
			   last->next=h;
			   last=h;
			}
		     }
		  }
	       }
	    }
	 }
      }
   }
   return (head);
}

/* Finds or adds an AnchorClass of the given name. Resets it to have the given subtable if not NULL */
static AnchorClass *SFFindOrAddAnchorClass(SplineFont *sf,char *name,struct lookup_subtable *sub) {
   AnchorClass *ac;
   int actype = act_unknown;
   for ( ac=sf->anchor; ac!=NULL; ac=ac->next )
     if (strcmp(name,ac->name)==0)
       break;
   if ( ac!=NULL && ( sub==NULL || ac->subtable==sub ) )
     return( ac );
   if ( sub!=NULL )
     actype = sub->lookup->lookup_type==gpos_cursive ? act_curs :
     sub->lookup->lookup_type==gpos_mark2base ? act_mark :
     sub->lookup->lookup_type==gpos_mark2ligature ? act_mklg :
     sub->lookup->lookup_type==gpos_mark2mark ? act_mkmk :
     act_unknown;
   if ( ac==NULL ) {
      ac=chunkalloc(sizeof(AnchorClass));
      ac->subtable=sub;
      ac->type=actype;
      ac->name=fastrdup(name);
      ac->next=sf->anchor;
      sf->anchor=ac;
   }
   else if ((sub!=NULL) && (ac->subtable!=sub)) {
      ac->subtable=sub;
      ac->type=actype;
   }
   return ac;
}


static SplineChar *_UFOLoadGlyph(SplineFont *sf,xmlDocPtr doc,
				 char *glifname, char *glyphname,
				 SplineChar * existingglyph, int layerdest) {
   xmlNodePtr glyph, kids, contour, points;
   SplineChar *sc;
   xmlChar *format, *width, *height, *u;
   char *name, *tmpname;
   int uni;
   char *cpt;
   SplineSet *last=NULL;
   int newsc=0;

   glyph=xmlDocGetRootElement(doc);
   format=xmlGetProp(glyph, (xmlChar *) "format");
   if (xmlStrcmp(glyph->name, (const xmlChar *) "glyph") != 0 ||
       (format != NULL && xmlStrcmp(format, (xmlChar *) "1") != 0)) {
      ErrorMsg(2,"Expected glyph file with format==1\n");
      xmlFreeDoc(doc);
      free(format);
      return (NULL);
   }
   free(format);
   tmpname=(char *) xmlGetProp(glyph, (xmlChar *) "name");
   if (glyphname != NULL) {
      // We use the provided name from the glyph listing since the specification says to trust that one more.
      name=fastrdup(glyphname);
      // But we still fetch the internally listed name for verification and fail on a mismatch.
      if ((name==NULL)
	  || ((name != NULL) && (tmpname != NULL)
	      && (strcmp(glyphname, name) != 0))) {
	 ErrorMsg(2,"Bad glyph name.\n");
	 if (tmpname != NULL) {
	    free(tmpname);
	    tmpname=NULL;
	 }
	 if (name != NULL) {
	    free(name);
	    name=NULL;
	 }
	 xmlFreeDoc(doc);
	 return NULL;
      }
      if (tmpname != NULL) {
	 free(tmpname);
	 tmpname=NULL;
      }
   } else {
      name=tmpname;
   }
   if (name==NULL && glifname != NULL) {
      char *pt=strrchr(glifname, '/');

      name=fastrdup(pt + 1);
      for (pt=cpt=name; *cpt != '\0'; ++cpt) {
	 if (*cpt != '_')
	    *pt++=*cpt;
	 else if (islower(*name))
	    *name=toupper(*name);
      }
      *pt='\0';
   } else if (name==NULL)
      name=fastrdup("nameless");
   // We assign a placeholder name if no name exists.
   // We create a new SplineChar 
   if (existingglyph != NULL) {
      sc=existingglyph;
      free(name);
      name=NULL;
   } else {
      sc=SplineCharCreate(2);
      sc->name=name;
      newsc=1;
   }
   if (sc==NULL) {
      xmlFreeDoc(doc);
      return NULL;
   }
   last=NULL;
   // Check layer availability here.
   if (layerdest >= sc->layer_cnt) {
      sc->layers=realloc(sc->layers, (layerdest + 1) * sizeof(Layer));
      memset(sc->layers + sc->layer_cnt, 0,
	     (layerdest + 1 - sc->layer_cnt) * sizeof(Layer));
      sc->layer_cnt=layerdest + 1;
   }
   if (sc->layers==NULL) {
      if ((newsc==1) && (sc != NULL)) {
	 SplineCharFree(sc);
      }
      xmlFreeDoc(doc);
      return NULL;
   }
   for (kids=glyph->children; kids != NULL; kids=kids->next) {
      if (xmlStrcmp(kids->name, (const xmlChar *) "advance")==0) {
	 if ((layerdest==ly_fore) || newsc) {
	    width=xmlGetProp(kids, (xmlChar *) "width");
	    height=xmlGetProp(kids, (xmlChar *) "height");
	    if (width != NULL)
	       sc->width=strtol((char *) width, NULL, 10);
	    if (height != NULL)
	       sc->vwidth=strtol((char *) height, NULL, 10);
	    sc->widthset=true;
	    free(width);
	    free(height);
	 }
      } else if (xmlStrcmp(kids->name, (const xmlChar *) "unicode")==0) {
	 if ((layerdest==ly_fore) || newsc) {
	    u=xmlGetProp(kids, (xmlChar *) "hex");
	    uni=strtol((char *) u, NULL, 16);
	    if (sc->unicodeenc==-1)
	       sc->unicodeenc=uni;
	    else
	       AltUniAdd(sc, uni);
	    free(u);
	 }
      } else if (xmlStrcmp(kids->name, (const xmlChar *) "outline")==0) {
	 for (contour=kids->children; contour != NULL;
	      contour=contour->next) {
	    if (xmlStrcmp(contour->name, (const xmlChar *) "component")==0) {
	       char *base=(char *) xmlGetProp(contour, (xmlChar *) "base"),
		  *xs=(char *) xmlGetProp(contour, (xmlChar *) "xScale"),
		  *ys=(char *) xmlGetProp(contour, (xmlChar *) "yScale"),
		  *xys=(char *) xmlGetProp(contour, (xmlChar *) "xyScale"),
		  *yxs=(char *) xmlGetProp(contour, (xmlChar *) "yxScale"),
		  *xo=(char *) xmlGetProp(contour, (xmlChar *) "xOffset"),
		  *yo=(char *) xmlGetProp(contour, (xmlChar *) "yOffset");
	       RefChar *r;

	       if (base==NULL)
		  ErrorMsg(2,"component with no base glyph\n");
	       else {
		  r=RefCharCreate();
		  r->sc=SplineCharCreate(0);
		  r->sc->name=base;
		  r->transform[0]=r->transform[3]=1;
		  if (xs != NULL)
		     r->transform[0]=strtod(xs, NULL);
		  if (ys != NULL)
		     r->transform[3]=strtod(ys, NULL);
		  if (xys != NULL)
		     r->transform[1]=strtod(xys, NULL);
		  if (yxs != NULL)
		     r->transform[2]=strtod(yxs, NULL);
		  if (xo != NULL)
		     r->transform[4]=strtod(xo, NULL);
		  if (yo != NULL)
		     r->transform[5]=strtod(yo, NULL);
		  r->next=sc->layers[layerdest].refs;
		  sc->layers[layerdest].refs=r;
	       }
	       free(xs);
	       free(ys);
	       free(xys);
	       free(yxs);
	       free(xo);
	       free(yo);
	    } else if (xmlStrcmp(contour->name, (const xmlChar *) "contour")
		      ==0) {
	       xmlNodePtr npoints;
	       SplineSet *ss;
	       SplinePoint *sp;
	       SplinePoint *sp2;
	       BasePoint pre[2], init[4];
	       int precnt=0, initcnt=0, open=0;
	       // precnt seems to count control points leading into the next on-curve point. pre stores those points.
	       // initcnt counts the control points that appear before the first on-curve point. This can get updated at the beginning and/or the end of the list.
	       // This is important for determining the order of the closing curve.
	       // A further improvement would be to prefetch the entire list so as to know the declared order of a curve before processing the point.

	       // We now look for anchor points.
	       char *sname;

	       for (points=contour->children; points != NULL;
		    points=points->next)
		  if (xmlStrcmp(points->name, (const xmlChar *) "point")==0)
		     break;
	       for (npoints=points->next; npoints != NULL;
		    npoints=npoints->next)
		  if (xmlStrcmp(npoints->name, (const xmlChar *) "point") ==
		      0)
		     break;
	       // If the contour has a single point without another point after it, we assume it to be an anchor point.
	       if (points != NULL && npoints==NULL) {
		  sname=(char *) xmlGetProp(points, (xmlChar *) "name");
		  if (sname != NULL) {
		     /* make an AP and if necessary an AC */
		     AnchorPoint *ap=chunkalloc(sizeof(AnchorPoint));
		     AnchorClass *ac;
		     char *namep=*sname=='_' ? sname + 1 : sname;
		     char *xs=(char *) xmlGetProp(points, (xmlChar *) "x");
		     char *ys=(char *) xmlGetProp(points, (xmlChar *) "y");

		     ap->me.x=strtod(xs, NULL);
		     ap->me.y=strtod(ys, NULL);

		     ac=SFFindOrAddAnchorClass(sf, namep, NULL);
		     if (*sname=='_')
			ap->type=ac->type==act_curs ? at_centry : at_mark;
		     else
			ap->type=ac->type==act_mkmk ? at_basemark :
			   ac->type==act_curs ? at_cexit :
			   ac->type==act_mklg ? at_baselig : at_basechar;
		     ap->anchor=ac;
		     ap->next=sc->anchor;
		     sc->anchor=ap;
		     free(xs);
		     free(ys);
		     free(sname);
		     continue;	// We stop processing the contour at this point.
		  }
	       }
	       // If we have not identified the contour as holding an anchor point, we continue processing it as a rendered shape.
	       int wasquad=-1;	// This tracks whether we identified the previous curve as quadratic. (-1 means undefined.)
	       int firstpointsaidquad=-1;	// This tracks the declared order of the curve leading into the first on-curve point.

	       ss=chunkalloc(sizeof(SplineSet));
	       ss->first=NULL;

	       for (points=contour->children; points != NULL;
		    points=points->next) {
		  char *xs, *ys, *type, *pname, *smooths;
		  double x, y;
		  int smooth=0;

		  // We discard any entities in the splineset that are not points.
		  if (xmlStrcmp(points->name, (const xmlChar *) "point") != 0)
		     continue;
		  // Read as strings from xml.
		  xs=(char *) xmlGetProp(points, (xmlChar *) "x");
		  ys=(char *) xmlGetProp(points, (xmlChar *) "y");
		  type=(char *) xmlGetProp(points, (xmlChar *) "type");
		  pname=(char *) xmlGetProp(points, (xmlChar *) "name");
		  smooths=(char *) xmlGetProp(points, (xmlChar *) "smooth");
		  if (smooths != NULL) {
		     if (strcmp(smooths, "yes")==0)
			smooth=1;
		     free(smooths);
		     smooths=NULL;
		  }
		  if (xs==NULL || ys==NULL) {
		     if (xs != NULL) {
			free(xs);
			xs=NULL;
		     }
		     if (ys != NULL) {
			free(ys);
			ys=NULL;
		     }
		     if (type != NULL) {
			free(type);
			type=NULL;
		     }
		     if (pname != NULL) {
			free(pname);
			pname=NULL;
		     }
		     continue;
		  }
		  x=strtod(xs, NULL);
		  y=strtod(ys, NULL);
		  if (type != NULL && (strcmp(type, "move")==0 ||
				       strcmp(type, "line")==0 ||
				       strcmp(type, "curve")==0 ||
				       strcmp(type, "qcurve")==0)) {
		     // This handles only actual points.
		     // We create and label the point.
		     sp=SplinePointCreate(x, y);
		     sp->dontinterpolate=1;
		     if (pname != NULL) {
			sp->name=fastrdup(pname);
		     }
		     if (smooth==1)
			sp->pointtype=pt_curve;
		     else
			sp->pointtype=pt_corner;
		     if (strcmp(type, "move")==0) {
			open=true;
			ss->first=ss->last=sp;
		     } else if (ss->first==NULL) {
			ss->first=ss->last=sp;
			memcpy(init, pre, sizeof(pre));
			initcnt=precnt;
			if (strcmp(type, "qcurve")==0)
			   wasquad=true;
			if (strcmp(type, "curve")==0)
			   wasquad=false;
			firstpointsaidquad=wasquad;
		     } else if (strcmp(type, "line")==0) {
			SplineMake(ss->last, sp, false);
			ss->last=sp;
		     } else if (strcmp(type, "curve")==0) {
			wasquad=false;
			if (precnt==2) {
			   ss->last->nextcp=pre[0];
			   ss->last->nonextcp=false;
			   sp->prevcp=pre[1];
			   sp->noprevcp=false;
			} else if (precnt==1) {
			   ss->last->nextcp=sp->prevcp=pre[0];
			   ss->last->nonextcp=sp->noprevcp=false;
			}
			SplineMake(ss->last, sp, false);
			ss->last=sp;
		     } else if (strcmp(type, "qcurve")==0) {
			wasquad=true;
			if (precnt > 0 && precnt <= 2) {
			   if (precnt==2) {
			      // If we have two cached control points and the end point is quadratic, we need an implied point between the two control points.
			      sp2 =
				 SplinePointCreate((pre[1].x + pre[0].x) / 2,
						   (pre[1].y + pre[0].y) / 2);
			      sp2->prevcp=ss->last->nextcp=pre[0];
			      sp2->noprevcp=ss->last->nonextcp=false;
			      sp2->ttfindex=0xffff;
			      SplineMake(ss->last, sp2, true);
			      ss->last=sp2;
			   }
			   // Now we connect the real point.
			   sp->prevcp=ss->last->nextcp=pre[precnt - 1];
			   sp->noprevcp=ss->last->nonextcp=false;
			}
			SplineMake(ss->last, sp, true);
			ss->last=sp;
		     }
		     precnt=0;
		  } else {
		     // This handles non-end-points (control points).
		     if (wasquad==-1 && precnt==2) {
			// We don't know whether the current curve is quadratic or cubic, but, if we're hitting three off-curve points in a row, something is off.
			// As mentioned below, we assume in this case that we're dealing with a quadratic TrueType curve that needs implied points.
			// We create those points since they are adjustable in Fontanvil.
			// There is not a valid case as far as Frank knows in which a cubic curve would have implied points.
			/* Undocumented fact: If there are no on-curve points (and therefore no indication of quadratic/cubic), assume truetype implied points */
			memcpy(init, pre, sizeof(pre));
			initcnt=1;
			// We make the point between the two already cached control points.
			sp =
			   SplinePointCreate((pre[1].x + pre[0].x) / 2,
					     (pre[1].y + pre[0].y) / 2);
			sp->ttfindex=0xffff;
			if (pname != NULL) {
			   sp->name=fastrdup(pname);
			}
			sp->nextcp=pre[1];
			sp->nonextcp=false;
			if (ss->first==NULL) {
			   // This is indeed possible if the first three points are control points.
			   ss->first=sp;
			   memcpy(init, pre, sizeof(pre));
			   initcnt=1;
			} else {
			   ss->last->nextcp=sp->prevcp=pre[0];
			   ss->last->nonextcp=sp->noprevcp=false;
			   initcnt=0;
			   SplineMake(ss->last, sp, true);
			}
			ss->last=sp;
			// We make the point between the previously cached control point and the new control point.
			sp =
			   SplinePointCreate((x + pre[1].x) / 2,
					     (y + pre[1].y) / 2);
			sp->prevcp=pre[1];
			sp->noprevcp=false;
			sp->ttfindex=0xffff;
			SplineMake(ss->last, sp, true);
			ss->last=sp;
			pre[0].x=x;
			pre[0].y=y;
			precnt=1;
			wasquad=true;
		     } else if (wasquad==true && precnt==1 && 0) {
			// Frank thinks that this might generate false positives for qcurves.
			// The only case in which this would create a qcurve missed by the previous condition block
			// and the point type reader would, it seems, be a cubic curve trailing a quadratic curve.
			// This seems not to be the best way to handle it.
			sp =
			   SplinePointCreate((x + pre[0].x) / 2,
					     (y + pre[0].y) / 2);
			if (pname != NULL) {
			   sp->name=fastrdup(pname);
			}
			sp->prevcp=pre[0];
			sp->noprevcp=false;
			sp->ttfindex=0xffff;
			if (ss->last==NULL) {
			   ss->first=sp;
			   memcpy(init, pre, sizeof(pre));
			   initcnt=1;
			} else {
			   ss->last->nextcp=sp->prevcp;
			   ss->last->nonextcp=false;
			   SplineMake(ss->last, sp, true);
			}
			ss->last=sp;
			pre[0].x=x;
			pre[0].y=y;
		     } else if (precnt < 2) {
			pre[precnt].x=x;
			pre[precnt].y=y;
			++precnt;
		     }
		  }
		  if (xs != NULL) {
		     free(xs);
		     xs=NULL;
		  }
		  if (ys != NULL) {
		     free(ys);
		     ys=NULL;
		  }
		  if (type != NULL) {
		     free(type);
		     type=NULL;
		  }
		  if (pname != NULL) {
		     free(pname);
		     pname=NULL;
		  }
	       }
	       // We are finished looping, so it's time to close the curve if it is to be closed.
	       if (!open) {
		  // init has a list of control points leading into the first point. pre has a list of control points trailing the last processed on-curve point.
		  // We merge pre into init and use init as the list of control points between the last processed on-curve point and the first on-curve point.
		  if (precnt != 0) {
		     BasePoint temp[2];

		     memcpy(temp, init, sizeof(temp));
		     memcpy(init, pre, sizeof(pre));
		     memcpy(init + precnt, temp, sizeof(temp));
		     initcnt += precnt;
		  }
		  if ((firstpointsaidquad==true && initcnt > 0)
		      || initcnt==1) {
		     // If the final curve is declared quadratic or is assumed to be by control point count, we proceed accordingly.
		     int i;

		     for (i=0; i < initcnt - 1; ++i) {
			// If the final curve is declared quadratic but has more than one control point, we add implied points.
			sp =
			   SplinePointCreate((init[i + 1].x + init[i].x) / 2,
					     (init[i + 1].y + init[i].y) / 2);
			sp->prevcp=ss->last->nextcp=init[i];
			sp->noprevcp=ss->last->nonextcp=false;
			sp->ttfindex=0xffff;
			SplineMake(ss->last, sp, true);
			ss->last=sp;
		     }
		     ss->last->nextcp=ss->first->prevcp=init[initcnt - 1];
		     ss->last->nonextcp=ss->first->noprevcp=false;
		     wasquad=true;
		  } else if (initcnt==2) {
		     ss->last->nextcp=init[0];
		     ss->first->prevcp=init[1];
		     ss->last->nonextcp=ss->first->noprevcp=false;
		     wasquad=false;
		  }
		  SplineMake(ss->last, ss->first, firstpointsaidquad);
		  ss->last=ss->first;
	       }
	       if (last==NULL) {
		  // FTODO
		  // Deal with existing splines somehow.
		  sc->layers[layerdest].splines=ss;
	       } else
		  last->next=ss;
	       last=ss;
	    }
	 }
      } else if (xmlStrcmp(kids->name, (const xmlChar *) "lib")==0) {
	 xmlNodePtr keys, temp, dict=FindNode(kids->children, "dict");

	 if (dict != NULL) {
	    for (keys=dict->children; keys != NULL; keys=keys->next) {
	       if (xmlStrcmp(keys->name, (const xmlChar *) "key")==0) {
		  char *keyname =
		     (char *) xmlNodeListGetString(doc, keys->children, true);
		  if (strcmp(keyname, "com.fontlab.hintData")==0) {
		     for (temp=keys->next; temp != NULL; temp=temp->next) {
			if (xmlStrcmp(temp->name, (const xmlChar *) "dict") ==
			    0)
			   break;
		     }
		     if (temp != NULL) {
			if (layerdest==ly_fore) {
			   if (sc->hstem==NULL) {
			      sc->hstem=GlifParseHints(doc, temp, "hhints");
			      SCGuessHHintInstancesList(sc, ly_fore);
			   }
			   if (sc->vstem==NULL) {
			      sc->vstem=GlifParseHints(doc, temp, "vhints");
			      SCGuessVHintInstancesList(sc, ly_fore);
			   }
			}
		     }
		     break;
		  }
		  free(keyname);
	       }
	    }
	 }
      }
   }
   xmlFreeDoc(doc);
   SPLCategorizePoints(sc->layers[layerdest].splines);
   return (sc);
}

static SplineChar *UFOLoadGlyph(SplineFont *sf,char *glifname,
				char *glyphname, SplineChar * existingglyph,
				int layerdest) {
   xmlDocPtr doc;

   doc=xmlParseFile(glifname);
   if (doc==NULL) {
      ErrorMsg(2,"Bad glif file %s\n", glifname);
      return (NULL);
   }
   return (_UFOLoadGlyph
	   (sf, doc, glifname, glyphname, existingglyph, layerdest));
}


static void UFORefFixup(SplineFont *sf,SplineChar *sc) {
   RefChar *r, *prev;
   SplineChar *rsc;

   if (sc==NULL || sc->ticked)
      return;
   sc->ticked=true;
   prev=NULL;
   // For each reference, attempt to locate the real splinechar matching the name stored in the fake splinechar.
   // Free the fake splinechar afterwards.
   for (r=sc->layers[ly_fore].refs; r != NULL; r=r->next) {
      rsc=SFGetChar(sf, -1, r->sc->name);
      if (rsc==NULL) {
	 ErrorMsg(2,"Failed to find glyph %s when fixing up references\n",
		  r->sc->name);
	 if (prev==NULL)
	    sc->layers[ly_fore].refs=r->next;
	 else
	    prev->next=r->next;
	 SplineCharFree(r->sc);
	 /* Memory leak. We loose r */
      } else {
	 UFORefFixup(sf, rsc);
	 SplineCharFree(r->sc);
	 r->sc=rsc;
	 prev=r;
	 SCReinstantiateRefChar(sc, r, ly_fore);
      }
   }
}

static void UFOLoadGlyphs(SplineFont *sf,char *glyphdir,int layerdest) {
   char *glyphlist=buildname(glyphdir, "contents.plist");
   xmlDocPtr doc;
   xmlNodePtr plist, dict, keys, value;
   char *valname, *glyphfname;
   int i;
   SplineChar *sc;
   int tot;

   doc=xmlParseFile(glyphlist);
   free(glyphlist);
   if (doc==NULL) {
      ErrorMsg(2,"Bad contents.plist\n");
      return;
   }
   plist=xmlDocGetRootElement(doc);
   dict=FindNode(plist->children, "dict");
   if (xmlStrcmp(plist->name, (const xmlChar *) "plist") != 0 || dict==NULL) {
      ErrorMsg(2,"Expected property list file\n");
      xmlFreeDoc(doc);
      return;
   }
   // Count glyphs for the benefit of measuring progress.
   for (tot=0, keys=dict->children; keys != NULL; keys=keys->next) {
      if (xmlStrcmp(keys->name, (const xmlChar *) "key")==0)
	 ++tot;
   }
   // Start reading in glyph name to file name mappings.
   for (keys=dict->children; keys != NULL; keys=keys->next) {
      for (value=keys->next;
	   value != NULL
	   && xmlStrcmp(value->name, (const xmlChar *) "text")==0;
	   value=value->next);
      if (value==NULL)
	 break;
      if (xmlStrcmp(keys->name, (const xmlChar *) "key")==0) {
	 char *glyphname =
	    (char *) xmlNodeListGetString(doc, keys->children, true);
	 int newsc=0;
	 SplineChar *existingglyph=NULL;

	 if (glyphname != NULL) {
	    existingglyph=SFGetChar(sf, -1, glyphname);
	    if (existingglyph==NULL)
	       newsc=1;
	    valname =
	       (char *) xmlNodeListGetString(doc, value->children, true);
	    glyphfname=buildname(glyphdir, valname);
	    free(valname);
	    sc =
	       UFOLoadGlyph(sf, glyphfname, glyphname, existingglyph,
			    layerdest);
	    if ((sc != NULL) && newsc) {
	       sc->parent=sf;
	       if (sf->glyphcnt >= sf->glyphmax)
		  sf->glyphs =
		     realloc(sf->glyphs,
			     (sf->glyphmax += 100) * sizeof(SplineChar *));
	       sc->orig_pos=sf->glyphcnt;
	       sf->glyphs[sf->glyphcnt++]=sc;
	    }
	 }
	 keys=value;
      }
   }
   xmlFreeDoc(doc);

   GlyphHashFree(sf);
   for (i=0; i < sf->glyphcnt; ++i)
      UFORefFixup(sf, sf->glyphs[i]);
}

static void UFOHandleKern(SplineFont *sf,char *basedir,int isv) {
   char *fname=buildname(basedir, isv ? "vkerning.plist" : "kerning.plist");
   xmlDocPtr doc=NULL;
   xmlNodePtr plist, dict, keys, value, subkeys;
   char *keyname, *valname;
   int offset;
   SplineChar *sc, *ssc;
   KernPair *kp;
   char *end;
   uint32_t script;

   if (GFileExists(fname))
      doc=xmlParseFile(fname);
   free(fname);
   if (doc==NULL)
      return;

   plist=xmlDocGetRootElement(doc);
   dict=FindNode(plist->children, "dict");
   if (xmlStrcmp(plist->name, (const xmlChar *) "plist") != 0 || dict==NULL) {
      ErrorMsg(2,"Expected property list file\n");
      xmlFreeDoc(doc);
      return;
   }
   for (keys=dict->children; keys != NULL; keys=keys->next) {
      for (value=keys->next;
	   value != NULL
	   && xmlStrcmp(value->name, (const xmlChar *) "text")==0;
	   value=value->next);
      if (value==NULL)
	 break;
      if (xmlStrcmp(keys->name, (const xmlChar *) "key")==0) {
	 keyname=(char *) xmlNodeListGetString(doc, keys->children, true);
	 sc=SFGetChar(sf, -1, keyname);
	 free(keyname);
	 if (sc==NULL)
	    continue;
	 keys=value;
	 for (subkeys=value->children; subkeys != NULL;
	      subkeys=subkeys->next) {
	    for (value=subkeys->next;
		 value != NULL
		 && xmlStrcmp(value->name, (const xmlChar *) "text")==0;
		 value=value->next);
	    if (value==NULL)
	       break;
	    if (xmlStrcmp(subkeys->name, (const xmlChar *) "key")==0) {
	       keyname =
		  (char *) xmlNodeListGetString(doc, subkeys->children, true);
	       ssc=SFGetChar(sf, -1, keyname);
	       free(keyname);
	       if (ssc==NULL)
		  continue;
	       for (kp=isv ? sc->vkerns : sc->kerns;
		    kp != NULL && kp->sc != ssc; kp=kp->next);
	       if (kp != NULL)
		  continue;
	       subkeys=value;
	       valname =
		  (char *) xmlNodeListGetString(doc, value->children, true);
	       offset=strtol(valname, &end, 10);
	       if (*end=='\0') {
		  kp=chunkalloc(sizeof(KernPair));
		  kp->off=offset;
		  kp->sc=ssc;
		  if (isv) {
		     kp->next=sc->vkerns;
		     sc->vkerns=kp;
		  } else {
		     kp->next=sc->kerns;
		     sc->kerns=kp;
		  }
		  script=SCScriptFromUnicode(sc);
		  if (script==DEFAULT_SCRIPT)
		     script=SCScriptFromUnicode(ssc);
		  kp->subtable=SFSubTableFindOrMake(sf,
						      isv ? CHR('v', 'k', 'r',
								'n') :
						      CHR('k', 'e', 'r', 'n'),
						      script, gpos_pair);
	       }
	       free(valname);
	    }
	 }
      }
   }
   xmlFreeDoc(doc);
}

static void UFOAddName(SplineFont *sf,char *value,int strid) {
   /* We simply assume that the entries in the name table are in English */
   /* The format doesn't say -- which bothers me */
   struct ttflangname *names;

   for (names=sf->names; names != NULL && names->lang != 0x409;
	names=names->next);
   if (names==NULL) {
      names=chunkalloc(sizeof(struct ttflangname));
      names->next=sf->names;
      names->lang=0x409;
      sf->names=names;
   }
   names->names[strid]=value;
}

static void UFOAddPrivate(SplineFont *sf,char *key,char *value) {
   char *pt;

   if (sf->private==NULL)
      sf->private=chunkalloc(sizeof(struct psdict));
   for (pt=value; *pt != '\0'; ++pt) {	/* Value might contain white space. turn into spaces */
      if (*pt=='\n' || *pt=='\r' || *pt=='\t')
	 *pt=' ';
   }
   PSDictChangeEntry(sf->private, key, value);
}

static void UFOAddPrivateArray(SplineFont *sf,char *key,xmlDocPtr doc,
			       xmlNodePtr value) {
   char space[400], *pt, *end;
   xmlNodePtr kid;

   if (xmlStrcmp(value->name, (const xmlChar *) "array") != 0)
      return;
   pt=space;
   end=pt + sizeof(space) - 10;
   *pt++='[';
   for (kid=value->children; kid != NULL; kid=kid->next) {
      if (xmlStrcmp(kid->name, (const xmlChar *) "integer")==0 ||
	  xmlStrcmp(kid->name, (const xmlChar *) "real")==0) {
	 char *valName =
	    (char *) xmlNodeListGetString(doc, kid->children, true);
	 if (pt + 1 + strlen(valName) < end) {
	    if (pt != space + 1)
	       *pt++=' ';
	    strcpy(pt, valName);
	    pt += strlen(pt);
	 }
	 free(valName);
      }
   }
   if (pt != space + 1) {
      *pt++=']';
      *pt++='\0';
      UFOAddPrivate(sf, key, space);
   }
}

static void UFOGetByteArray(char *array,int cnt,xmlDocPtr doc,
			    xmlNodePtr value) {
   xmlNodePtr kid;
   int i;

   memset(array, 0, cnt);

   if (xmlStrcmp(value->name, (const xmlChar *) "array") != 0)
      return;
   i=0;
   for (kid=value->children; kid != NULL; kid=kid->next) {
      if (xmlStrcmp(kid->name, (const xmlChar *) "integer")==0) {
	 char *valName =
	    (char *) xmlNodeListGetString(doc, kid->children, true);
	 if (i < cnt)
	    array[i++]=strtol(valName, NULL, 10);
	 free(valName);
      }
   }
}

static long UFOGetBits(xmlDocPtr doc,xmlNodePtr value) {
   xmlNodePtr kid;
   long mask=0;

   if (xmlStrcmp(value->name, (const xmlChar *) "array") != 0)
      return (0);
   for (kid=value->children; kid != NULL; kid=kid->next) {
      if (xmlStrcmp(kid->name, (const xmlChar *) "integer")==0) {
	 char *valName =
	    (char *) xmlNodeListGetString(doc, kid->children, true);
	 mask |= 1 << strtol(valName, NULL, 10);
	 free(valName);
      }
   }
   return (mask);
}

static void UFOGetBitArray(xmlDocPtr doc,xmlNodePtr value,uint32_t *res,
			   int len) {
   xmlNodePtr kid;
   int index;

   if (xmlStrcmp(value->name, (const xmlChar *) "array") != 0)
      return;
   for (kid=value->children; kid != NULL; kid=kid->next) {
      if (xmlStrcmp(kid->name, (const xmlChar *) "integer")==0) {
	 char *valName =
	    (char *) xmlNodeListGetString(doc, kid->children, true);
	 index=strtol(valName, NULL, 10);
	 if (index < len << 5)
	    res[index >> 5] |= 1 << (index & 31);
	 free(valName);
      }
   }
}

SplineFont *SFReadUFO(char *basedir, int flags) {
   xmlNodePtr plist, dict, keys, value;
   xmlDocPtr doc;
   SplineFont *sf;
   xmlChar *keyname, *valname;
   char *stylename=NULL;
   char *temp, *glyphlist, *glyphdir;
   char oldloc[25], *end;
   int as=-1, ds=-1, em=-1;

   if (!libxml_init_base()) {
      ErrorMsg(2,"Can't find libxml2.\n");
      return (NULL);
   }

   temp=buildname(basedir, "fontinfo.plist");
   doc=xmlParseFile(temp);
   free(temp);
   if (doc==NULL) {
      /* Can I get an error message from libxml? */
      return (NULL);
   }
   plist=xmlDocGetRootElement(doc);
   dict=FindNode(plist->children, "dict");
   if (xmlStrcmp(plist->name, (const xmlChar *) "plist") != 0 || dict==NULL) {
      ErrorMsg(2,"Expected property list file\n");
      xmlFreeDoc(doc);
      return (NULL);
   }

   sf=SplineFontEmpty();
   strncpy(oldloc, setlocale(LC_NUMERIC, NULL), 24);
   oldloc[24]=0;
   setlocale(LC_NUMERIC, "C");
   for (keys=dict->children; keys != NULL; keys=keys->next) {
      for (value=keys->next;
	   value != NULL
	   && xmlStrcmp(value->name, (const xmlChar *) "text")==0;
	   value=value->next);
      if (value==NULL)
	 break;
      if (xmlStrcmp(keys->name, (const xmlChar *) "key")==0) {
	 keyname=xmlNodeListGetString(doc, keys->children, true);
	 valname=xmlNodeListGetString(doc, value->children, true);
	 keys=value;
	 if (xmlStrcmp(keyname, (xmlChar *) "familyName")==0)
	    sf->familyname=(char *) valname;
	 else if (xmlStrcmp(keyname, (xmlChar *) "styleName")==0)
	    stylename=(char *) valname;
	 else if (xmlStrcmp(keyname, (xmlChar *) "fullName")==0 ||
		  xmlStrcmp(keyname, (xmlChar *) "postscriptFullName")==0)
	    sf->fullname=(char *) valname;
	 else if (xmlStrcmp(keyname, (xmlChar *) "fontName")==0 ||
		  xmlStrcmp(keyname, (xmlChar *) "postscriptFontName")==0)
	    sf->fontname=(char *) valname;
	 else if (xmlStrcmp(keyname, (xmlChar *) "weightName")==0 ||
		  xmlStrcmp(keyname, (xmlChar *) "postscriptWeightName")==0)
	    sf->weight=(char *) valname;
	 else if (xmlStrcmp(keyname, (xmlChar *) "note")==0)
	    sf->comments=(char *) valname;
	 else if (xmlStrcmp(keyname, (xmlChar *) "copyright")==0)
	    sf->copyright=(char *) valname;
	 else if (xmlStrcmp(keyname, (xmlChar *) "trademark")==0)
	    UFOAddName(sf, (char *) valname, ttf_trademark);
	 else if (strncmp((char *) keyname, "openTypeName", 12)==0) {
	    if (xmlStrcmp(keyname + 12, (xmlChar *) "Designer")==0)
	       UFOAddName(sf, (char *) valname, ttf_designer);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "DesignerURL")==0)
	       UFOAddName(sf, (char *) valname, ttf_designerurl);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "Manufacturer")==0)
	       UFOAddName(sf, (char *) valname, ttf_manufacturer);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "ManufacturerURL") ==
		     0)
	       UFOAddName(sf, (char *) valname, ttf_venderurl);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "License")==0)
	       UFOAddName(sf, (char *) valname, ttf_license);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "LicenseURL")==0)
	       UFOAddName(sf, (char *) valname, ttf_licenseurl);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "Version")==0)
	       UFOAddName(sf, (char *) valname, ttf_version);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "UniqueID")==0)
	       UFOAddName(sf, (char *) valname, ttf_uniqueid);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "Description")==0)
	       UFOAddName(sf, (char *) valname, ttf_descriptor);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "PreferedFamilyName")
		    ==0)
	       UFOAddName(sf, (char *) valname, ttf_preffamilyname);
	    else
	       if (xmlStrcmp
		   (keyname + 12, (xmlChar *) "PreferedSubfamilyName")==0)
	       UFOAddName(sf, (char *) valname, ttf_prefmodifiers);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "CompatibleFullName")
		    ==0)
	       UFOAddName(sf, (char *) valname, ttf_compatfull);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "SampleText")==0)
	       UFOAddName(sf, (char *) valname, ttf_sampletext);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "WWSFamilyName") ==
		     0)
	       UFOAddName(sf, (char *) valname, ttf_wwsfamily);
	    else if (xmlStrcmp(keyname + 12, (xmlChar *) "WWSSubfamilyName")
		    ==0)
	       UFOAddName(sf, (char *) valname, ttf_wwssubfamily);
	    else
	       free(valname);
	 } else if (strncmp((char *) keyname, "openTypeHhea", 12)==0) {
	    if (xmlStrcmp(keyname + 12, (xmlChar *) "Ascender")==0) {
	       sf->pfminfo.hhead_ascent=strtol((char *) valname, &end, 10);
	       sf->pfminfo.hheadascent_add=false;
	    } else if (xmlStrcmp(keyname + 12, (xmlChar *) "Descender")==0) {
	       sf->pfminfo.hhead_descent=strtol((char *) valname, &end, 10);
	       sf->pfminfo.hheaddescent_add=false;
	    } else if (xmlStrcmp(keyname + 12, (xmlChar *) "LineGap")==0)
	       sf->pfminfo.linegap=strtol((char *) valname, &end, 10);
	    free(valname);
	    sf->pfminfo.hheadset=true;
	 } else if (strncmp((char *) keyname, "openTypeVhea", 12)==0) {
	    if (xmlStrcmp(keyname + 12, (xmlChar *) "LineGap")==0)
	       sf->pfminfo.vlinegap=strtol((char *) valname, &end, 10);
	    sf->pfminfo.vheadset=true;
	    free(valname);
	 } else if (strncmp((char *) keyname, "openTypeOS2", 11)==0) {
	    sf->pfminfo.pfmset=true;
	    if (xmlStrcmp(keyname + 11, (xmlChar *) "Panose")==0) {
	       UFOGetByteArray(sf->pfminfo.panose, sizeof(sf->pfminfo.panose),
			       doc, value);
	       sf->pfminfo.panose_set=true;
	    } else if (xmlStrcmp(keyname + 11, (xmlChar *) "Type")==0) {
	       sf->pfminfo.fstype=UFOGetBits(doc, value);
	       if (sf->pfminfo.fstype < 0) {
		  /* all bits are set, but this is wrong, OpenType spec says */
		  /* bits 0, 4-7 and 10-15 must be unset, go see             */
		  /* http://www.microsoft.com/typography/otspec/os2.htm#fst  */
		  ErrorMsg(2,"Bad openTypeOS2type key: all bits are set. It will be ignored\n");
		  sf->pfminfo.fstype=0;
	       }
	    } else if (xmlStrcmp(keyname + 11, (xmlChar *) "FamilyClass") ==
		       0) {
	       char fc[2];

	       UFOGetByteArray(fc, sizeof(fc), doc, value);
	       sf->pfminfo.os2_family_class=(fc[0] << 8) | fc[1];
	    } else if (xmlStrcmp(keyname + 11, (xmlChar *) "WidthClass")==0)
	       sf->pfminfo.width=strtol((char *) valname, &end, 10);
	    else if (xmlStrcmp(keyname + 11, (xmlChar *) "WeightClass")==0)
	       sf->pfminfo.weight=strtol((char *) valname, &end, 10);
	    else if (xmlStrcmp(keyname + 11, (xmlChar *) "VendorID")==0) {
	       const int os2_vendor_sz=sizeof(sf->pfminfo.os2_vendor);
	       int valname_len;

	       if (valname &&
		   (valname_len=strlen((const char *)valname))
		   <=os2_vendor_sz)
		  strncpy(sf->pfminfo.os2_vendor,(const char *)valname,
			  valname_len);
	       char *temp=sf->pfminfo.os2_vendor + os2_vendor_sz - 1;

	       while (*temp==0 && temp >= sf->pfminfo.os2_vendor)
		  *temp--=' ';
	    } else if (xmlStrcmp(keyname + 11, (xmlChar *) "TypoAscender") ==
		       0) {
	       sf->pfminfo.typoascent_add=false;
	       sf->pfminfo.os2_typoascent =
		  strtol((char *) valname, &end, 10);
	    } else if (xmlStrcmp(keyname + 11, (xmlChar *) "TypoDescender") ==
		       0) {
	       sf->pfminfo.typodescent_add=false;
	       sf->pfminfo.os2_typodescent =
		  strtol((char *) valname, &end, 10);
	    } else if (xmlStrcmp(keyname + 11, (xmlChar *) "TypoLineGap") ==
		       0)
	       sf->pfminfo.os2_typolinegap =
		  strtol((char *) valname, &end, 10);
	    else if (xmlStrcmp(keyname + 11, (xmlChar *) "WinAscent")==0) {
	       sf->pfminfo.winascent_add=false;
	       sf->pfminfo.os2_winascent=strtol((char *) valname, &end, 10);
	    } else if (xmlStrcmp(keyname + 11, (xmlChar *) "WinDescent")==0) {
	       sf->pfminfo.windescent_add=false;
	       sf->pfminfo.os2_windescent =
		  strtol((char *) valname, &end, 10);
	    } else if (strncmp((char *) keyname + 11, "Subscript", 9)==0) {
	       sf->pfminfo.subsuper_set=true;
	       if (xmlStrcmp(keyname + 20, (xmlChar *) "XSize")==0)
		  sf->pfminfo.os2_subxsize =
		     strtol((char *) valname, &end, 10);
	       else if (xmlStrcmp(keyname + 20, (xmlChar *) "YSize")==0)
		  sf->pfminfo.os2_subysize =
		     strtol((char *) valname, &end, 10);
	       else if (xmlStrcmp(keyname + 20, (xmlChar *) "XOffset")==0)
		  sf->pfminfo.os2_subxoff =
		     strtol((char *) valname, &end, 10);
	       else if (xmlStrcmp(keyname + 20, (xmlChar *) "YOffset")==0)
		  sf->pfminfo.os2_subyoff =
		     strtol((char *) valname, &end, 10);
	    } else if (strncmp((char *) keyname + 11, "Superscript", 11)==0) {
	       sf->pfminfo.subsuper_set=true;
	       if (xmlStrcmp(keyname + 22, (xmlChar *) "XSize")==0)
		  sf->pfminfo.os2_supxsize =
		     strtol((char *) valname, &end, 10);
	       else if (xmlStrcmp(keyname + 22, (xmlChar *) "YSize")==0)
		  sf->pfminfo.os2_supysize =
		     strtol((char *) valname, &end, 10);
	       else if (xmlStrcmp(keyname + 22, (xmlChar *) "XOffset")==0)
		  sf->pfminfo.os2_supxoff =
		     strtol((char *) valname, &end, 10);
	       else if (xmlStrcmp(keyname + 22, (xmlChar *) "YOffset")==0)
		  sf->pfminfo.os2_supyoff =
		     strtol((char *) valname, &end, 10);
	    } else if (strncmp((char *) keyname + 11, "Strikeout", 9)==0) {
	       sf->pfminfo.subsuper_set=true;
	       if (xmlStrcmp(keyname + 20, (xmlChar *) "Size")==0)
		  sf->pfminfo.os2_strikeysize =
		     strtol((char *) valname, &end, 10);
	       else if (xmlStrcmp(keyname + 20, (xmlChar *) "Position")==0)
		  sf->pfminfo.os2_strikeypos =
		     strtol((char *) valname, &end, 10);
	    } else if (strncmp((char *) keyname + 11, "CodePageRanges", 14) ==
		       0) {
	       UFOGetBitArray(doc, value, sf->pfminfo.codepages, 2);
	       sf->pfminfo.hascodepages=true;
	    } else if (strncmp((char *) keyname + 11, "UnicodeRanges", 13) ==
		       0) {
	       UFOGetBitArray(doc, value, sf->pfminfo.unicoderanges, 4);
	       sf->pfminfo.hasunicoderanges=true;
	    }
	    free(valname);
	 } else if (strncmp((char *) keyname, "postscript", 10)==0) {
	    if (xmlStrcmp(keyname + 10, (xmlChar *) "UnderlineThickness") ==
		0)
	       sf->uwidth=strtol((char *) valname, &end, 10);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "UnderlinePosition")
		    ==0)
	       sf->upos=strtol((char *) valname, &end, 10);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "BlueFuzz")==0)
	       UFOAddPrivate(sf, "BlueFuzz", (char *) valname);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "BlueScale")==0)
	       UFOAddPrivate(sf, "BlueScale", (char *) valname);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "BlueShift")==0)
	       UFOAddPrivate(sf, "BlueShift", (char *) valname);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "BlueValues")==0)
	       UFOAddPrivateArray(sf, "BlueValues", doc, value);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "OtherBlues")==0)
	       UFOAddPrivateArray(sf, "OtherBlues", doc, value);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "FamilyBlues")==0)
	       UFOAddPrivateArray(sf, "FamilyBlues", doc, value);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "FamilyOtherBlues")
		    ==0)
	       UFOAddPrivateArray(sf, "FamilyOtherBlues", doc, value);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "StemSnapH")==0)
	       UFOAddPrivateArray(sf, "StemSnapH", doc, value);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "StemSnapV")==0)
	       UFOAddPrivateArray(sf, "StemSnapV", doc, value);
	    else if (xmlStrcmp(keyname + 10, (xmlChar *) "ForceBold")==0)
	       UFOAddPrivate(sf, "ForceBold", (char *) value->name);
	    /* value->name is either true or false */
	    free(valname);
	 } else if (strncmp((char *) keyname, "macintosh", 9)==0) {
	    if (xmlStrcmp(keyname + 9, (xmlChar *) "FONDName")==0)
	       sf->fondname=(char *) valname;
	    else
	       free(valname);
	 } else if (xmlStrcmp(keyname, (xmlChar *) "unitsPerEm")==0) {
	    em=strtol((char *) valname, &end, 10);
	    if (*end != '\0')
	       em=-1;
	    free(valname);
	 } else if (xmlStrcmp(keyname, (xmlChar *) "ascender")==0) {
	    as=strtod((char *) valname, &end);
	    if (*end != '\0')
	       as=-1;
	    else
	       sf->ufo_ascent=as;
	    free(valname);
	 } else if (xmlStrcmp(keyname, (xmlChar *) "descender")==0) {
	    ds=-strtod((char *) valname, &end);
	    if (*end != '\0')
	       ds=-1;
	    else
	       sf->ufo_descent=-ds;
	    free(valname);
	 } else if (xmlStrcmp(keyname, (xmlChar *) "italicAngle")==0 ||
		    xmlStrcmp(keyname,
			      (xmlChar *) "postscriptSlantAngle")==0) {
	    sf->italicangle=strtod((char *) valname, &end);
	    if (*end != '\0')
	       sf->italicangle=0;
	    free(valname);
	 } else if (xmlStrcmp(keyname, (xmlChar *) "descender")==0) {
	    ds=-strtol((char *) valname, &end, 10);
	    if (*end != '\0')
	       ds=-1;
	    free(valname);
	 } else
	    free(valname);
	 free(keyname);
      }
   }
   if (em==-1 && as != -1 && ds != -1)
      em=as + ds;
   if (em==as + ds)
      /* Yay! They follow my conventions */ ;
   else if (em != -1) {
      as=800 * em / 1000;
      ds=em - as;
   }
   if (em==-1) {
      ErrorMsg(2,"This font does not specify unitsPerEm\n");
      xmlFreeDoc(doc);
      setlocale(LC_NUMERIC, oldloc);
      SplineFontFree(sf);
      free(glyphdir);
      return (NULL);
   }
   sf->ascent=as;
   sf->descent=ds;
   if (sf->fontname==NULL) {
      if (stylename != NULL && sf->familyname != NULL)
	 sf->fontname=strconcat3(sf->familyname, "-", stylename);
      else
	 sf->fontname=fastrdup("Untitled");
   }
   if (sf->fullname==NULL) {
      if (stylename != NULL && sf->familyname != NULL)
	 sf->fullname=strconcat3(sf->familyname, " ", stylename);
      else
	 sf->fullname=fastrdup(sf->fontname);
   }
   if (sf->familyname==NULL)
      sf->familyname=fastrdup(sf->fontname);
   free(stylename);
   if (sf->weight==NULL)
      sf->weight=fastrdup("Regular");
   if (sf->version==NULL && sf->names != NULL &&
       sf->names->names[ttf_version] != NULL &&
       strncmp(sf->names->names[ttf_version], "Version ", 8)==0)
      sf->version=fastrdup(sf->names->names[ttf_version] + 8);
   xmlFreeDoc(doc);
   char *layercontentsname=buildname(basedir, "layercontents.plist");
   char **layernames=NULL;

   if (layercontentsname==NULL) {
      return (NULL);
   } else if (GFileExists(layercontentsname)) {
      xmlDocPtr layercontentsdoc=NULL;
      xmlNodePtr layercontentsplist=NULL;
      xmlNodePtr layercontentsdict=NULL;
      xmlNodePtr layercontentslayer=NULL;
      xmlNodePtr layercontentsvalue=NULL;
      int layercontentslayercount=0;
      int layernamesbuffersize=0;
      int layercontentsvaluecount=0;

      if ((layercontentsdoc=xmlParseFile(layercontentsname))) {
	 // The layercontents plist contains an array of double-element arrays. There is no top-level dict. Note that the indices in the layercontents array may not match those in the Fontanvil layers array due to reserved spaces.
	 if ((layercontentsplist=xmlDocGetRootElement(layercontentsdoc))
	     && (layercontentsdict =
		 FindNode(layercontentsplist->children, "array"))) {
	    layercontentslayercount=0;
	    layernamesbuffersize=2;
	    layernames=malloc(2 * sizeof(char *) * layernamesbuffersize);
	    // Look through the children of the top-level array. Stop if one of them is not an array. (Ignore text objects since these probably just have whitespace.)
	    for (layercontentslayer=layercontentsdict->children;
		 (layercontentslayer != NULL)
		 &&
		 ((xmlStrcmp
		   (layercontentslayer->name, (const xmlChar *) "array")==0)
		  ||
		  (xmlStrcmp
		   (layercontentslayer->name,
		    (const xmlChar *) "text")==0));
		 layercontentslayer=layercontentslayer->next) {
	       if (xmlStrcmp
		   (layercontentslayer->name,
		    (const xmlChar *) "array")==0) {
		  xmlChar *layerlabel=NULL;
		  xmlChar *layerglyphdirname=NULL;

		  layercontentsvaluecount=0;
		  // Look through the children (effectively columns) of the layer array (the row). Reject non-string values.
		  for (layercontentsvalue=layercontentslayer->children;
		       (layercontentsvalue != NULL)
		       &&
		       ((xmlStrcmp
			 (layercontentsvalue->name,
			  (const xmlChar *) "string")==0)
			||
			(xmlStrcmp
			 (layercontentsvalue->name,
			  (const xmlChar *) "text")==0));
		       layercontentsvalue=layercontentsvalue->next) {
		     if (xmlStrcmp
			 (layercontentsvalue->name,
			  (const xmlChar *) "string")==0) {
			if (layercontentsvaluecount==0)
			   layerlabel =
			      xmlNodeListGetString(layercontentsdoc,
						   layercontentsvalue->
						   xmlChildrenNode, true);
			if (layercontentsvaluecount==1)
			   layerglyphdirname =
			      xmlNodeListGetString(layercontentsdoc,
						   layercontentsvalue->
						   xmlChildrenNode, true);
			layercontentsvaluecount++;
		     }
		  }
		  // We need two values (as noted above) per layer entry and ignore any layer lacking those.
		  if ((layercontentsvaluecount > 1)
		      && (layernamesbuffersize < INT_MAX / 2)) {
		     // Resize the layer names array as necessary.
		     if (layercontentslayercount >= layernamesbuffersize) {
			layernamesbuffersize *= 2;
			layernames =
			   realloc(layernames,
				   2 * sizeof(char *) * layernamesbuffersize);
		     }
		     // Fail silently on allocation failure; it's highly unlikely.
		     if (layernames != NULL) {
			layernames[2 * layercontentslayercount] =
			   fastrdup((char *) (layerlabel));
			if (layernames[2 * layercontentslayercount]) {
			   layernames[(2 * layercontentslayercount) + 1] =
			      fastrdup((char *) (layerglyphdirname));
			   if (layernames[(2 * layercontentslayercount) + 1])
			      layercontentslayercount++;	// We increment only if both pointers are valid so as to avoid read problems later.
			   else
			      free(layernames[2 * layercontentslayercount]);
			}
		     }
		  }
		  if (layerlabel != NULL) {
		     xmlFree(layerlabel);
		     layerlabel=NULL;
		  }
		  if (layerglyphdirname != NULL) {
		     xmlFree(layerglyphdirname);
		     layerglyphdirname=NULL;
		  }
	       }
	    }
	    if (layernames != NULL) {
	       int lcount=0;
	       int auxpos=2;
	       int layerdest=0;
	       int bg=1;

	       if (layercontentslayercount > 0) {
		  // Start reading layers.
		  for (lcount=0; lcount < layercontentslayercount; lcount++) {
		     if ((glyphdir =
			  buildname(basedir, layernames[2 * lcount + 1]))) {
			if ((glyphlist =
			     buildname(glyphdir, "contents.plist"))) {
			   if (!GFileExists(glyphlist)) {
			      ErrorMsg(2,"No glyphs directory or no contents file\n");
			   } else {
			      // Only public.default gets mapped as a foreground layer.
			      bg=1;
			      // public.default and public.background have fixed mappings. Other layers start at 2.
			      if (strcmp
				  (layernames[2 * lcount],
				   "public.default")==0) {
				 layerdest=ly_fore;
				 bg=0;
			      } else
				 if (strcmp
				     (layernames[2 * lcount],
				      "public.background")==0) {
				 layerdest=ly_back;
			      } else {
				 layerdest=auxpos++;
			      }

			      // We ensure that the splinefont layer list has sufficient space.
			      if (layerdest + 1 > sf->layer_cnt) {
				 sf->layers =
				    realloc(sf->layers,
					    (layerdest +
					     1) * sizeof(LayerInfo));
				 memset(sf->layers + sf->layer_cnt, 0,
					((layerdest + 1) -
					 sf->layer_cnt) * sizeof(LayerInfo));
			      }
			      sf->layer_cnt=layerdest + 1;

			      // The check is redundant, but it allows us to copy from sfd.c.
			      if ((layerdest < sf->layer_cnt) && sf->layers) {
				 if (sf->layers[layerdest].name)
				    free(sf->layers[layerdest].name);
				 sf->layers[layerdest].name =
				    layernames[2 * lcount];
				 sf->layers[layerdest].background=bg;
				 // Fetch glyphs.
				 UFOLoadGlyphs(sf, glyphdir, layerdest);
				 // Determine layer spline order.
				 sf->layers[layerdest].order2 =
				    SFLFindOrder(sf, layerdest);
				 // Conform layer spline order (reworking control points if necessary).
				 SFLSetOrder(sf, layerdest,
					     sf->layers[layerdest].order2);
				 // Set the grid order to the foreground order if appropriate.
				 if (layerdest==ly_fore)
				    sf->grid.order2 =
				       sf->layers[layerdest].order2;
			      }
			   }
			   free(glyphlist);
			}
			free(glyphdir);
		     }
		  }
	       } else
		 ErrorMsg(2,"layercontents.plist lists no valid layers.\n");

	       // Free layer names.
	       for (lcount=0; lcount < layercontentslayercount; lcount++) {
		  if (layernames[2 * lcount])
		     free(layernames[2 * lcount]);
		  if (layernames[2 * lcount + 1])
		     free(layernames[2 * lcount + 1]);
	       }
	       free(layernames);
	    }
	 }
	 xmlFreeDoc(layercontentsdoc);
      }
   } else {
      glyphdir=buildname(basedir, "glyphs");
      glyphlist=buildname(glyphdir, "contents.plist");
      if (!GFileExists(glyphlist)) {
	 ErrorMsg(2,"No glyphs directory or no contents file\n");
      } else {
	 UFOLoadGlyphs(sf, glyphdir, ly_fore);
	 sf->layers[ly_fore].order2=sf->layers[ly_back].order2 =
	    sf->grid.order2=SFFindOrder(sf);
	 SFSetOrder(sf, sf->layers[ly_fore].order2);
      }
      free(glyphlist);
      free(glyphdir);
   }
   free(layercontentsname);

   sf->map=EncMapFromEncoding(sf, FindOrMakeEncoding("Unicode"));

   /* Might as well check for feature files even if version 1 */
   temp=buildname(basedir, "features.fea");
   if (GFileExists(temp))
      SFApplyFeatureFilename(sf, temp);
   free(temp);

   UFOHandleKern(sf, basedir, 0);
   UFOHandleKern(sf, basedir, 1);

   setlocale(LC_NUMERIC, oldloc);
   return (sf);
}

SplineSet *SplinePointListInterpretGlif(SplineFont *sf, char *filename,
					char *memory, int memlen, int em_size,
					int ascent, int is_stroked) {
   xmlDocPtr doc;
   char oldloc[25];
   SplineChar *sc;
   SplineSet *ss;

   if (!libxml_init_base()) {
      ErrorMsg(2,"Can't find libxml2.\n");
      return (NULL);
   }
   if (filename != NULL)
      doc=xmlParseFile(filename);
   else
      doc=xmlParseMemory(memory, memlen);
   if (doc==NULL)
      return (NULL);

   strncpy(oldloc, setlocale(LC_NUMERIC, NULL), 24);
   oldloc[24]=0;
   setlocale(LC_NUMERIC, "C");
   sc=_UFOLoadGlyph(sf, doc, filename, NULL, NULL, ly_fore);
   setlocale(LC_NUMERIC, oldloc);

   if (sc==NULL)
      return (NULL);

   ss=sc->layers[ly_fore].splines;
   sc->layers[ly_fore].splines=NULL;
   SplineCharFree(sc);
   return (ss);
}

int HasUFO(void) {
   return (libxml_init_base());
}
#endif
