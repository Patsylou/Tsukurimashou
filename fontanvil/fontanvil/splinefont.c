/* $Id: splinefont.c 4302 2015-10-24 15:00:46Z mskala $ */
/* Copyright (C) 2000-2012  George Williams
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

#include "fontanvilvw.h"
#include <utype.h>
#include <ustring.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gfile.h>
#include <time.h>
#include "psfont.h"
#include <locale.h>

void SFUntickAll(SplineFont *sf) {
   int i;

   for (i=0; i < sf->glyphcnt; ++i)
      if (sf->glyphs[i] != NULL)
	 sf->glyphs[i]->ticked=false;
}

void SFOrderBitmapList(SplineFont *sf) {
   BDFFont *bdf, *prev, *next;
   BDFFont *bdf2, *prev2;

   for (prev=NULL, bdf=sf->bitmaps; bdf != NULL; bdf=bdf->next) {
      for (prev2=NULL, bdf2=bdf->next; bdf2 != NULL; bdf2=bdf2->next) {
	 if (bdf->pixelsize > bdf2->pixelsize ||
	     (bdf->pixelsize==bdf2->pixelsize
	      && BDFDepth(bdf) > BDFDepth(bdf2))) {
	    if (prev==NULL)
	       sf->bitmaps=bdf2;
	    else
	       prev->next=bdf2;
	    if (prev2==NULL) {
	       bdf->next=bdf2->next;
	       bdf2->next=bdf;
	    } else {
	       next=bdf->next;
	       bdf->next=bdf2->next;
	       bdf2->next=next;
	       prev2->next=bdf;
	    }
	    next=bdf;
	    bdf=bdf2;
	    bdf2=next;
	 }
	 prev2=bdf2;
      }
      prev=bdf;
   }
}

SplineChar *SCBuildDummy(SplineChar *dummy,SplineFont *sf,EncMap *map,
			 int i) {
   static char namebuf[100];
   static Layer layers[2];

   memset(dummy, '\0', sizeof(*dummy));

   dummy->color=COLOR_DEFAULT;
   dummy->layer_cnt=2;
   dummy->layers=layers;

   if (sf->cidmaster != NULL) {
      /* CID fonts don't have encodings, instead we must look up the cid */
      if (sf->cidmaster->loading_cid_map)
	dummy->unicodeenc=-1;
      else
	dummy->unicodeenc =
	CID2NameUni(FindCidMap
		    (sf->cidmaster->cidregistry, sf->cidmaster->ordering,
			sf->cidmaster->supplement, sf->cidmaster), i,
		    namebuf, sizeof(namebuf));
   } else
     dummy->unicodeenc=UniFromEnc(i, map->enc);

   if (sf->cidmaster != NULL)
     dummy->name=namebuf;
   else if (map->enc->psnames!=NULL && i<map->enc->char_cnt &&
	    map->enc->psnames[i]!=NULL)
      dummy->name=map->enc->psnames[i];
   else if (dummy->unicodeenc==-1)
     dummy->name=NULL;

   else
     dummy->name=
     (char *)StdGlyphName(namebuf,dummy->unicodeenc,sf->uni_interp,
			  sf->for_new_glyphs);

   if (dummy->name==NULL) {
      int j;

      sprintf(namebuf, "NameMe.%d", i);
      j=0;
      while (SFFindExistingSlot(sf,-1,namebuf)!=-1)
	sprintf(namebuf,"NameMe.%d.%d",i,++j);
      dummy->name=namebuf;
   }

   dummy->width=sf->ascent+sf->descent;
   dummy->vwidth=dummy->width;

   if (dummy->unicodeenc>0 && dummy->unicodeenc<0x10000 &&
       iscombining(dummy->unicodeenc)) {
      /* Mark characters should be 0 width */
      dummy->width=0;
      /* Except in monospaced fonts on windows, where they should be the */
      /*  same width as everything else */
   }

   /* Actually, in a monospace font, all glyphs should be the same width */
   /*  whether mark or not */
   if (sf->pfminfo.panose_set && sf->pfminfo.panose[3]==9 &&
       sf->glyphcnt>0) {
      for (i=sf->glyphcnt-1;i>=0;--i)
	if (SCWorthOutputting(sf->glyphs[i])) {
	   dummy->width=sf->glyphs[i]->width;
	   break;
	}
   }

   dummy->parent=sf;
   dummy->orig_pos=0xffff;
   return (dummy);
}

static SplineChar *_SFMakeChar(SplineFont *sf,EncMap *map,int enc) {
   SplineChar dummy, *sc;
   SplineFont *ssf;
   int j, real_uni, gid;
   extern const int cns14pua[], amspua[];

   if (enc >= map->enccount)
     gid=-1;
   else
     gid=map->map[enc];

   if (sf->subfontcnt != 0 && gid != -1) {
      ssf=NULL;
      for (j=0; j < sf->subfontcnt; ++j)
	if (gid < sf->subfonts[j]->glyphcnt) {
	   ssf=sf->subfonts[j];
	   if (ssf->glyphs[gid] != NULL) {
	      return (ssf->glyphs[gid]);
	   }
	}
      sf=ssf;
   }

   if (gid==-1 || (sc=sf->glyphs[gid])==NULL) {
      if ((map->enc->is_unicodebmp || map->enc->is_unicodefull) &&
	  (enc >= 0xe000 && enc <= 0xf8ff) &&
	  (sf->uni_interp==ui_ams || sf->uni_interp==ui_trad_chinese) &&
	  (real_uni =
	      (sf->uni_interp==ui_ams ? amspua : cns14pua)[enc - 0xe000]) !=
	  0) {

	 if (real_uni < map->enccount) {
	    SplineChar *sc;

	    /* if necessary, create the real unicode code point */
	    /*  and then make us be a duplicate of it */
	    sc=_SFMakeChar(sf, map, real_uni);
	    map->map[enc]=gid=sc->orig_pos;
	    SCCharChangedUpdate(sc, ly_all, true);
	    return (sc);
	 }
      }

      SCBuildDummy(&dummy, sf, map, enc);

      /* Let's say a user has a postscript encoding where the glyph ".notdef" */
      /*  is assigned to many slots. Once the user creates a .notdef glyph */
      /*  all those slots should fill in. If they don't they damn well better */
      /*  when the user clicks on one to edit it */
      /* Used to do that with all encodings. It just confused people */
      if (map->enc->psnames != NULL &&
	  (sc=SFGetChar(sf, dummy.unicodeenc, dummy.name)) != NULL) {
	 map->map[enc]=sc->orig_pos;
	 AltUniAdd(sc, dummy.unicodeenc);
	 return (sc);
      }

      sc=SFSplineCharCreate(sf);
      sc->unicodeenc=dummy.unicodeenc;
      sc->name=fastrdup(dummy.name);
      sc->width=dummy.width;
      sc->vwidth=dummy.vwidth;
      sc->orig_pos=0xffff;

      if (sf->cidmaster != NULL)
	sc->altuni =
	CIDSetAltUnis(FindCidMap
		      (sf->cidmaster->cidregistry,
			  sf->cidmaster->ordering, sf->cidmaster->supplement,
			  sf->cidmaster), enc);
      SFAddGlyphAndEncode(sf, sc, map, enc);
   }

   return (sc);
}

SplineChar *SFMakeChar(SplineFont *sf, EncMap * map, int enc) {
   int gid;

   if (enc==-1)
      return (NULL);
   if (enc >= map->enccount)
      gid=-1;
   else
      gid=map->map[enc];
   if (sf->mm != NULL && (gid==-1 || sf->glyphs[gid]==NULL)) {
      int j;

      _SFMakeChar(sf->mm->normal, map, enc);
      for (j=0; j < sf->mm->instance_count; ++j)
	 _SFMakeChar(sf->mm->instances[j], map, enc);
   }
   return (_SFMakeChar(sf, map, enc));
}

int NameToEncoding(SplineFont *sf, EncMap * map, const char *name) {
   int enc, uni, i, ch;
   char *end, *freeme=NULL;
   const char *upt=name;

   ch=utf8_ildb(&upt);
   if (*upt=='\0') {
      enc=SFFindSlot(sf, map, ch, NULL);
      if (enc != -1)
	 return (enc);
   }

   enc=uni=-1;

   enc=SFFindSlot(sf, map, -1, name);
   if (enc != -1) {
      free(freeme);
      return (enc);
   }
   if ((*name=='U' || *name=='u') && name[1]=='+') {
      uni=strtol(name + 2, &end, 16);
      if (*end != '\0')
	 uni=-1;
   } else if (name[0]=='u' && name[1]=='n' && name[2]=='i') {
      uni=strtol(name + 3, &end, 16);
      if (*end != '\0')
	 uni=-1;
   } else if (name[0]=='g' && name[1]=='l' && name[2]=='y'
	      && name[3]=='p' && name[4]=='h') {
      int orig=strtol(name + 5, &end, 10);

      if (*end != '\0')
	 orig=-1;
      if (orig != -1)
	 enc=map->backmap[orig];
   } else if (isdigit(*name)) {
      enc=strtoul(name, &end, 0);
      if (*end != '\0')
	 enc=-1;
      if (map->remap != NULL && enc != -1) {
	 struct remap *remap=map->remap;

	 while (remap->infont != -1) {
	    if (enc >= remap->firstenc && enc <= remap->lastenc) {
	       enc += remap->infont - remap->firstenc;
	       break;
	    }
	    ++remap;
	 }
      }
   } else {
      if (enc==-1) {
	 uni=UniFromName(name, sf->uni_interp, map->enc);
	 if (uni < 0 && name[1]=='\0')
	    uni=name[0];
      }
   }
   if (enc >= map->enccount || enc < 0)
      enc=-1;
   if (enc==-1 && uni != -1)
      enc=SFFindSlot(sf, map, uni, NULL);
   /* Used to have code to remove dotted variant names and apply extensions */
   /*  like ".initial" to get the unicode for arabic init/medial/final variants */
   /*  But that doesn't sound like a good idea. And it would also map "a.sc" */
   /*  to "a" -- which was confusing */
   return (enc);
}

static char *scaleString(char *string,double scale) {
   char *result;
   char *pt;
   char *end;
   double val;

   if (string==NULL)
      return (NULL);

   while (*string==' ')
      ++string;
   result=malloc(10 * strlen(string) + 1);
   if (*string != '[') {
      val=strtod(string, &end);
      if (end==string) {
	 free(result);
	 return (NULL);
      }
      sprintf(result, "%g", val * scale);
      return (result);
   }

   pt=result;
   *pt++='[';
   ++string;
   while (*string != '\0' && *string != ']') {
      val=strtod(string, &end);
      if (end==string) {
	 free(result);
	 return (NULL);
      }
      sprintf(pt, "%g ", val * scale);
      pt += strlen(pt);
      string=end;
   }
   if (pt[-1]==' ')
      pt[-1]=']';
   else
      *pt++=']';
   *pt='\0';
   return (result);
}

static char *iscaleString(char *string,double scale) {
   char *result;
   char *pt;
   char *end;
   double val;

   if (string==NULL)
      return (NULL);

   while (*string==' ')
      ++string;
   result=malloc(10 * strlen(string) + 1);
   if (*string != '[') {
      val=strtod(string, &end);
      if (end==string) {
	 free(result);
	 return (NULL);
      }
      sprintf(result, "%g", rint(val * scale));
      return (result);
   }

   pt=result;
   *pt++='[';
   ++string;
   while (*string != '\0' && *string != ']') {
      val=strtod(string, &end);
      if (end==string) {
	 free(result);
	 return (NULL);
      }
      sprintf(pt, "%g ", rint(val * scale));
      pt += strlen(pt);
      string=end;
      while (*string==' ')
	 ++string;
   }
   if (pt[-1]==' ')
      pt[-1]=']';
   else
      *pt++=']';
   *pt='\0';
   return (result);
}

static void SFScalePrivate(SplineFont *sf,double scale) {
   static char *scalethese[]={
      NULL
   };
   static char *integerscalethese[]={
      "BlueValues", "OtherBlues",
      "FamilyBlues", "FamilyOtherBlues",
      "BlueShift", "BlueFuzz",
      "StdHW", "StdVW", "StemSnapH", "StemSnapV",
      NULL
   };
   int i;

   for (i=0; integerscalethese[i] != NULL; ++i) {
      char *str=PSDictHasEntry(sf->private, integerscalethese[i]);
      char *new=iscaleString(str, scale);

      if (new != NULL)
	 PSDictChangeEntry(sf->private, integerscalethese[i], new);
      free(new);
   }
   for (i=0; scalethese[i] != NULL; ++i) {
      char *str=PSDictHasEntry(sf->private, scalethese[i]);
      char *new=scaleString(str, scale);

      if (new != NULL)
	 PSDictChangeEntry(sf->private, scalethese[i], new);
      free(new);
   }
}

static void ScaleBase(struct Base *base,double scale) {
   struct basescript *bs;
   struct baselangextent *bl, *feat;
   int i;

   for (bs=base->scripts; bs != NULL; bs=bs->next) {
      for (i=0; i < base->baseline_cnt; ++i)
	 bs->baseline_pos[i]=(int) rint(bs->baseline_pos[i] * scale);
      for (bl=bs->langs; bl != NULL; bl=bl->next) {
	 bl->ascent=(int) rint(scale * bl->ascent);
	 bl->descent=(int) rint(scale * bl->descent);
	 for (feat=bl->features; feat != NULL; feat=feat->next) {
	    feat->ascent=(int) rint(scale * feat->ascent);
	    feat->descent=(int) rint(scale * feat->descent);
	 }
      }
   }
}

int SFScaleToEm(SplineFont *sf, int as, int des) {
   bigreal scale;
   real transform[6];
   BVTFunc bvts;
   uint8_t *oldselected=sf->fv->selected;
   enum fvtrans_flags trans_flags =
      fvt_alllayers | fvt_round_to_int | fvt_dontsetwidth |
      fvt_scalekernclasses | fvt_scalepstpos | fvt_dogrid;

   scale=(as + des) / (bigreal) (sf->ascent + sf->descent);
   sf->pfminfo.hhead_ascent=rint(sf->pfminfo.hhead_ascent * scale);
   sf->pfminfo.hhead_descent=rint(sf->pfminfo.hhead_descent * scale);
   sf->pfminfo.linegap=rint(sf->pfminfo.linegap * scale);
   sf->pfminfo.vlinegap=rint(sf->pfminfo.vlinegap * scale);
   sf->pfminfo.os2_winascent=rint(sf->pfminfo.os2_winascent * scale);
   sf->pfminfo.os2_windescent=rint(sf->pfminfo.os2_windescent * scale);
   sf->pfminfo.os2_typoascent=rint(sf->pfminfo.os2_typoascent * scale);
   sf->pfminfo.os2_typodescent=rint(sf->pfminfo.os2_typodescent * scale);
   sf->pfminfo.os2_typolinegap=rint(sf->pfminfo.os2_typolinegap * scale);

   sf->pfminfo.os2_subxsize=rint(sf->pfminfo.os2_subxsize * scale);
   sf->pfminfo.os2_subysize=rint(sf->pfminfo.os2_subysize * scale);
   sf->pfminfo.os2_subxoff=rint(sf->pfminfo.os2_subxoff * scale);
   sf->pfminfo.os2_subyoff=rint(sf->pfminfo.os2_subyoff * scale);
   sf->pfminfo.os2_supxsize=rint(sf->pfminfo.os2_supxsize * scale);
   sf->pfminfo.os2_supysize=rint(sf->pfminfo.os2_supysize * scale);
   sf->pfminfo.os2_supxoff=rint(sf->pfminfo.os2_supxoff * scale);
   sf->pfminfo.os2_supyoff=rint(sf->pfminfo.os2_supyoff * scale);
   sf->pfminfo.os2_strikeysize=rint(sf->pfminfo.os2_strikeysize * scale);
   sf->pfminfo.os2_strikeypos=rint(sf->pfminfo.os2_strikeypos * scale);
   sf->upos *= scale;
   sf->uwidth *= scale;
   sf->ufo_ascent *= scale;
   sf->ufo_descent *= scale;

   if (sf->private != NULL)
      SFScalePrivate(sf, scale);
   if (sf->horiz_base != NULL)
      ScaleBase(sf->horiz_base, scale);
   if (sf->vert_base != NULL)
      ScaleBase(sf->vert_base, scale);

   if (as + des==sf->ascent + sf->descent) {
      if (as != sf->ascent && des != sf->descent) {
	 sf->ascent=as;
	 sf->descent=des;
	 sf->changed=true;
      }
      return (false);
   }

   transform[0]=transform[3]=scale;
   transform[1]=transform[2]=transform[4]=transform[5]=0;
   bvts.func=bvt_none;
   sf->fv->selected=malloc(sf->fv->map->enccount);
   memset(sf->fv->selected, 1, sf->fv->map->enccount);

   sf->ascent=as;
   sf->descent=des;

   /* If someone has set an absurdly small em size, try to contain
      the damage by not rounding to int. */
   if ((as + des) < 32) {
      trans_flags &= ~fvt_round_to_int;
   }

   FVTransFunc(sf->fv, transform, 0, &bvts, trans_flags);
   free(sf->fv->selected);
   sf->fv->selected=oldselected;

   if (!sf->changed) {
      sf->changed=true;
   }

   return (true);
}

void SFSetModTime(SplineFont *sf) {
   time_t now;

   time(&now);
   sf->modificationtime=now;
}

static SplineFont *_SFReadPostScript(AFILE *file,char *filename) {
   FontDict *fd=NULL;
   SplineFont *sf=NULL;

   fd=_ReadPSFont(file);
   if (fd != NULL) {
      sf=SplineFontFromPSFont(fd);
      PSFontFree(fd);
      if (sf != NULL)
	 CheckAfmOfPostScript(sf, filename, sf->map);
   }
   return (sf);
}

static SplineFont *SFReadPostScript(char *filename) {
   FontDict *fd=NULL;
   SplineFont *sf=NULL;

   fd=ReadPSFont(filename);
   if (fd != NULL) {
      sf=SplineFontFromPSFont(fd);
      PSFontFree(fd);
      if (sf != NULL)
	 CheckAfmOfPostScript(sf, filename, sf->map);
   }
   return (sf);
}

struct archivers archivers[]={
   {".tar", "tar", "tar", "tf", "xf", "rf", ars_tar},
   {".tgz", "tar", "tar", "tfz", "xfz", "rfz", ars_tar},
   {".tar.gz", "tar", "tar", "tfz", "xfz", "rfz", ars_tar},
   {".tar.bz2", "tar", "tar", "tfj", "xfj", "rfj", ars_tar},
   {".tbz2", "tar", "tar", "tfj", "xfj", "rfj", ars_tar},
   {".tbz", "tar", "tar", "tfj", "xfj", "rfj", ars_tar},
   {".zip", "unzip", "zip", "-l", "", "", ars_zip},
   /* { ".tar.lzma", ? } */
   ARCHIVERS_EMPTY
};

void ArchiveCleanup(char *archivedir) {
   /* Free this directory and all files within it */
   char *cmd;

   cmd=malloc(strlen(archivedir) + 20);
   sprintf(cmd, "rm -rf %s", archivedir);
   system(cmd);
   free(cmd);
   free(archivedir);
}

static char *ArchiveParseTOC(char *listfile,enum archive_list_style ars,
			     int *doall) {
   AFILE *file;
   int nlcnt, ch, linelen, linelenmax, fcnt, choice, i, def, def_prio, prio;
   char **files, *linebuffer, *pt, *name;

   *doall=false;
   file=afopen(listfile, "r");
   if (file==NULL)
      return (NULL);

   nlcnt=linelenmax=linelen=0;
   while ((ch=agetc(file)) != EOF) {
      if (ch=='\n') {
	 ++nlcnt;
	 if (linelen > linelenmax)
	    linelenmax=linelen;
	 linelen=0;
      } else
	 ++linelen;
   }
   afseek(file,0,SEEK_SET);

   /* tar outputs its table of contents as a simple list of names */
   /* zip includes a bunch of other info, headers (and lines for directories) */

   linebuffer=malloc(linelenmax + 3);
   fcnt=0;
   files=malloc((nlcnt + 1) * sizeof(char *));

   if (ars==ars_tar) {
      pt=linebuffer;
      while ((ch=agetc(file)) != EOF) {
	 if (ch=='\n') {
	    *pt='\0';
	    /* Blessed if I know what encoded was used for filenames */
	    /*  inside the tar file. I shall assume utf8, faut de mieux */
	    files[fcnt++]=fastrdup(linebuffer);
	    pt=linebuffer;
	 } else
	    *pt++=ch;
      }
   } else {
      /* Skip the first three lines, header info */
      afgets(linebuffer, linelenmax + 3, file);
      afgets(linebuffer, linelenmax + 3, file);
      afgets(linebuffer, linelenmax + 3, file);
      pt=linebuffer;
      while ((ch=agetc(file)) != EOF) {
	 if (ch=='\n') {
	    *pt='\0';
	    if (linebuffer[0]==' ' && linebuffer[1]=='-'
		&& linebuffer[2]=='-')
	       break;		/* End of file list */
	    /* Blessed if I know what encoded was used for filenames */
	    /*  inside the zip file. I shall assume utf8, faut de mieux */
	    if (pt - linebuffer >= 28 && pt[-1] != '/')
	       files[fcnt++]=fastrdup(linebuffer + 28);
	    pt=linebuffer;
	 } else
	    *pt++=ch;
      }
   }
   files[fcnt]=NULL;
   afclose(file);

   free(linebuffer);
   if (fcnt==0) {
      free(files);
      return (NULL);
   } else if (fcnt==1) {
      char *onlyname=files[0];

      free(files);
      return (onlyname);
   }

   /* Suppose they've got an archive of a UFO directory? */
   /*  It won't show up in the list of files (because either */
   /*  tar or I have removed all directories from that list) */
   pt=strrchr(files[0], '/');
   if (pt != NULL) {
      if ((pt-files[0]>4
	   && (strncasecmp(pt-4,".ufo",4)==0
	       || strncasecmp(pt-4,"_ufo",4)==0))) {
	 /* Ok, looks like a potential directory font. Now is EVERYTHING */
	 /*  in the archive inside this guy? */
	 for (i=0; i < fcnt; ++i)
	    if (strncmp(files[i], files[0], pt - files[0] + 1) != 0)
	       break;
	 if (i==fcnt) {
	    char *onlydirfont=copyn(files[0], pt - files[0] + 1);

	    for (i=0; i < fcnt; ++i)
	       free(files[i]);
	    free(files);
	    *doall=true;
	    return (onlydirfont);
	 }
      }
   }

   def=0;
   def_prio=-1;
   for (i=0; i < fcnt; ++i) {
      pt=strrchr(files[i], '.');
      if (pt==NULL)
	 continue;
      if (strcasecmp(pt, ".svg")==0)
	 prio=10;
      else if (strcasecmp(pt, ".pfb")==0 || strcasecmp(pt, ".pfa")==0 ||
	       strcasecmp(pt, ".cff")==0 || strcasecmp(pt, ".cid")==0)
	 prio=20;
      else if (strcasecmp(pt, ".otf")==0 || strcasecmp(pt, ".ttf")==0
	       || strcasecmp(pt, ".ttc")==0)
	 prio=30;
      else if (strcasecmp(pt, ".sfd")==0)
	 prio=40;
      else
	 continue;
      if (prio > def_prio) {
	 def=i;
	 def_prio=prio;
      }
   }

   choice=def;
   if (choice==-1)
      name=NULL;
   else
      name=fastrdup(files[choice]);

   for (i=0; i < fcnt; ++i)
      free(files[i]);
   free(files);
   return (name);
}

#define TOC_NAME	"ff-archive-table-of-contents"

char *Unarchive(char *name, char **_archivedir) {
   char *dir=getenv("TMPDIR");
   char *pt, *archivedir, *listfile, *listcommand, *unarchivecmd,
      *desiredfile;
   char *finalfile;
   int i;
   int doall=false;
   static int cnt=0;

   *_archivedir=NULL;

   pt=strrchr(name, '.');
   if (pt==NULL)
      return (NULL);
   for (i=0; archivers[i].ext != NULL; ++i)
      if (strcmp(archivers[i].ext, pt)==0)
	 break;
   if (archivers[i].ext==NULL) {
      /* some of those endings have two extensions... */
      while (pt > name && pt[-1] != '.')
	 --pt;
      if (pt==name)
	 return (NULL);
      --pt;
      for (i=0; archivers[i].ext != NULL; ++i)
	 if (strcmp(archivers[i].ext, pt)==0)
	    break;
      if (archivers[i].ext==NULL)
	 return (NULL);
   }

   if (dir==NULL)
      dir=P_tmpdir;
   archivedir=malloc(strlen(dir) + 100);
   sprintf(archivedir, "%s/ffarchive-%d-%d", dir, getpid(), ++cnt);
   if (GFileMkDir(archivedir) != 0) {
      free(archivedir);
      return (NULL);
   }

   listfile=malloc(strlen(archivedir) + strlen("/" TOC_NAME) + 1);
   sprintf(listfile, "%s/" TOC_NAME, archivedir);

   listcommand=malloc(strlen(archivers[i].unarchive) + 1 +
			strlen(archivers[i].listargs) + 1 +
			strlen(name) + 3 + strlen(listfile) + 4);
   sprintf(listcommand, "%s %s %s > %s", archivers[i].unarchive,
	   archivers[i].listargs, name, listfile);
   if (system(listcommand) != 0) {
      free(listcommand);
      free(listfile);
      ArchiveCleanup(archivedir);
      return (NULL);
   }
   free(listcommand);

   desiredfile=ArchiveParseTOC(listfile, archivers[i].ars, &doall);
   free(listfile);
   if (desiredfile==NULL) {
      ArchiveCleanup(archivedir);
      return (NULL);
   }

   /* I tried sending everything to stdout, but that doesn't work if the */
   /*  output is a directory file */
   unarchivecmd=malloc(strlen(archivers[i].unarchive) + 1 +
			 strlen(archivers[i].listargs) + 1 +
			 strlen(name) + 1 +
			 strlen(desiredfile) + 3 + strlen(archivedir) + 30);
   sprintf(unarchivecmd, "( cd %s ; %s %s %s %s ) > /dev/null", archivedir,
	   archivers[i].unarchive,
	   archivers[i].extractargs, name, doall ? "" : desiredfile);
   if (system(unarchivecmd) != 0) {
      free(unarchivecmd);
      free(desiredfile);
      ArchiveCleanup(archivedir);
      return (NULL);
   }
   free(unarchivecmd);

   finalfile=malloc(strlen(archivedir) + 1 + strlen(desiredfile) + 1);
   sprintf(finalfile, "%s/%s", archivedir, desiredfile);
   free(desiredfile);

   *_archivedir=archivedir;
   return (finalfile);
}

struct compressors compressors[]={
   {".gz", "gunzip", "gzip"},
   {".bz2", "bunzip2", "bzip2"},
   {".bz", "bunzip2", "bzip2"},
   {".Z", "gunzip", "compress"},
   {".lzma", "unlzma", "lzma"},
/* file types which are both archived and compressed (.tgz, .zip) are handled */
/*  by the archiver above */
   COMPRESSORS_EMPTY
};

char *Decompress(char *name, int compression) {
   char *dir=getenv("TMPDIR");
   char buf[1500];
   char *tmpfile;

   if (dir==NULL)
      dir=P_tmpdir;
   tmpfile=malloc(strlen(dir) + strlen(GFileNameTail(name)) + 2);
   strcpy(tmpfile, dir);
   strcat(tmpfile, "/");
   strcat(tmpfile, GFileNameTail(name));
   *strrchr(tmpfile, '.')='\0';
   snprintf(buf, sizeof(buf), "%s < %s > %s", compressors[compression].decomp,
	    name, tmpfile);
   if (system(buf)==0)
      return (tmpfile);
   free(tmpfile);
   return (NULL);
}

static char *ForceFileToHaveName(AFILE *file,char *exten) {
   char tmpfilename[L_tmpnam + 100];
   static int try=0;
   AFILE *newfile;

   while (1) {
      sprintf(tmpfilename, P_tmpdir "/fontanvil%d-%d", getpid(), try++);
      if (exten != NULL)
	 strcat(tmpfilename, exten);
      if (access(tmpfilename, F_OK)==-1 &&
	  (newfile=afopen(tmpfilename, "w")) != NULL) {
	 char buffer[1024];
	 int len;

	 while ((len=afread(buffer, 1, sizeof(buffer), file)) > 0)
	    afwrite(buffer, 1, len, newfile);
	 afclose(newfile);
      }
      return (fastrdup(tmpfilename));	/* The filename does not exist */
   }
}

/* This does not check currently existing fontviews, and should only be used */
/*  by LoadSplineFont (which does) and by RevertFile (which knows what it's doing) */
SplineFont *_ReadSplineFont(AFILE *file, char *filename,
			    enum openflags openflags) {
   SplineFont *sf;
   char ubuf[251], *temp;
   int fromsfd=false;
   int i;
   char *pt, *ext2, *strippedname, *oldstrippedname, *tmpfile=NULL, *paren =
      NULL, *fullname=filename, *rparen;
   char *archivedir=NULL;
   int len;
   int checked;
   int compression=0;
   int wasurl=false, nowlocal=true, wasarchived=false;

   if (filename==NULL)
      return (NULL);

   strippedname=filename;
   pt=strrchr(filename, '/');
   if (pt==NULL)
      pt=filename;
   /* Someone gave me a font "Nafees Nastaleeq(Updated).ttf" and complained */
   /*  that ff wouldn't open it */
   /* Now someone will complain about "Nafees(Updated).ttc(fo(ob)ar)" */
   if ((paren=strrchr(pt, '(')) != NULL &&
       (rparen=strrchr(paren, ')')) != NULL && rparen[1]=='\0') {
      strippedname=fastrdup(filename);
      strippedname[paren - filename]='\0';
   }

   pt=strrchr(strippedname, '.');
   if (pt != NULL) {
      for (ext2=pt - 1; ext2 > strippedname && *ext2 != '.'; --ext2);
      for (i=0; archivers[i].ext != NULL; ++i) {
	 /* some of the archive "extensions" are actually two like ".tar.bz2" */
	 if (strcmp(archivers[i].ext, pt)==0
	     || strcmp(archivers[i].ext, ext2)==0) {
	    if (file != NULL) {
	       char *spuriousname =
		  ForceFileToHaveName(file, archivers[i].ext);
	       strippedname=Unarchive(spuriousname, &archivedir);
	       afclose(file);
	       file=NULL;
	       unlink(spuriousname);
	       free(spuriousname);
	    } else
	       strippedname=Unarchive(strippedname, &archivedir);
	    if (strippedname==NULL)
	       return (NULL);
	    if (strippedname != filename && paren != NULL) {
	       fullname=malloc(strlen(strippedname) + strlen(paren) + 1);
	       strcpy(fullname, strippedname);
	       strcat(fullname, paren);
	    } else
	       fullname=strippedname;
	    pt=strrchr(strippedname, '.');
	    wasarchived=true;
	    break;
	 }
      }
   }

   i=-1;
   if (pt != NULL)
      for (i=0; compressors[i].ext != NULL; ++i)
	 if (strcmp(compressors[i].ext, pt)==0)
	    break;
   oldstrippedname=strippedname;
   if (i==-1 || compressors[i].ext==NULL)
      i=-1;
   else {
      if (file != NULL) {
	 char *spuriousname=ForceFileToHaveName(file, compressors[i].ext);

	 tmpfile=Decompress(spuriousname, i);
	 afclose(file);
	 file=NULL;
	 unlink(spuriousname);
	 free(spuriousname);
      } else
	 tmpfile=Decompress(strippedname, i);
      if (tmpfile != NULL) {
	 strippedname=tmpfile;
      } else {
	 ErrorMsg(2,"Decompression failed.\n");
	 return (NULL);
      }
      compression=i + 1;
      if (strippedname != filename && paren != NULL) {
	 fullname=malloc(strlen(strippedname) + strlen(paren) + 1);
	 strcpy(fullname, strippedname);
	 strcat(fullname, paren);
      } else
	 fullname=strippedname;
   }

   /* If there are no pfaedit windows, give them something to look at */
   /*  immediately. Otherwise delay a bit */
   strncpy(ubuf,"Loading font from ",sizeof(ubuf)-1);
   len=strlen(ubuf);
   if (!wasurl || i==-1)	/* If it wasn't compressed, or it wasn't an url, then the fullname is reasonable, else use the original name */
      strncat(ubuf, temp=fastrdup(GFileNameTail(fullname)), 100);
   else
      strncat(ubuf, temp=fastrdup(GFileNameTail(filename)), 100);
   free(temp);
   ubuf[100 + len]='\0';

   if (file==NULL) {
      file=afopen(strippedname, "rb");
      nowlocal=true;
   }

   sf=NULL;
   checked=false;
/* checked==false => not checked */
/* checked=='u'   => UFO */
/* checked=='t'   => TTF/OTF */
/* checked=='p'   => pfb/general postscript */
/* checked=='P'   => pdf */
/* checked=='c'   => cff */
/* checked=='S'   => svg */
/* checked=='f'   => sfd */
/* checked=='b'   => bdf */
/* checked=='i'   => ikarus */
   if (!wasurl && GFileIsDir(strippedname)) {
      char *temp =
	 malloc(strlen(strippedname) + strlen("/glyphs/contents.plist") + 1);
      strcpy(temp, strippedname);
      strcat(temp, "/glyphs/contents.plist");
      if (GFileExists(temp)) {
	 sf=SFReadUFO(strippedname, 0);
	 checked='u';
      }
      free(temp);
      if (file != NULL)
	 afclose(file);
   } else if (file != NULL) {
      /* Try to guess the file type from the first few characters... */
      int ch1=agetc(file);
      int ch2=agetc(file);
      int ch3=agetc(file);
      int ch4=agetc(file);
      int ch5=agetc(file);
      int ch6=agetc(file);
      int ch7=agetc(file);
      int ch9, ch10;

      afseek(file, 98, SEEK_SET);
      ch9=agetc(file);
      ch10=agetc(file);
      afseek(file,0,SEEK_SET);
      if ((ch1==0 && ch2==1 && ch3==0 && ch4==0) ||
	  (ch1=='O' && ch2=='T' && ch3=='T' && ch4=='O') ||
	  (ch1=='t' && ch2=='r' && ch3=='u' && ch4=='e') ||
	  (ch1=='t' && ch2=='t' && ch3=='c' && ch4=='f')) {
	 sf=_SFReadTTF(file, 0, openflags, fullname, NULL);
	 checked='t';
      } else if (ch1=='w' && ch2=='O' && ch3=='F' && ch4=='F') {
	 sf=_SFReadWOFF(file, 0, openflags, fullname, NULL);
	 checked='w';
      } else if ((ch1=='%' && ch2=='!') || (ch1==0x80 && ch2=='\01')) {	/* PFB header */
	 sf=_SFReadPostScript(file, fullname);
	 checked='p';
      } else if (ch1=='%' && ch2=='P' && ch3=='D' && ch4=='F') {
	 sf=_SFReadPdfFont(file, fullname, openflags);
	 checked='P';
      } else if (ch1==1 && ch2==0 && ch3==4) {
	 int len;

	 afseek(file, 0, SEEK_END);
	 len=aftell(file);
	 afseek(file, 0, SEEK_SET);
	 sf=_CFFParse(file, len, NULL);
	 checked='c';
      } else
	 if ((ch1=='<' && ch2=='?' && (ch3=='x' || ch3=='X')
	      && (ch4=='m' || ch4=='M')) ||
	     /* or UTF-8 SVG with initial byte ordering mark */
	     ((ch1==0xef && ch2==0xbb && ch3==0xbf &&
	       ch4=='<' && ch5=='?' && (ch6=='x' || ch6=='X')
	       && (ch7=='m' || ch7=='M')))) {
	 if (nowlocal)
	    sf=SFReadSVG(fullname, 0);
	 else {
	    char *spuriousname=ForceFileToHaveName(file, NULL);

	    sf=SFReadSVG(spuriousname, 0);
	    unlink(spuriousname);
	    free(spuriousname);
	 }
	 checked='S';
      } else if (ch1=='S' && ch2=='p' && ch3=='l' && ch4=='i') {
	 sf=_SFDRead(fullname, file);
	 file=NULL;
	 checked='f';
	 fromsfd=true;
      } else if (ch1=='S' && ch2=='T' && ch3=='A' && ch4=='R') {
	 sf=SFFromBDF(fullname, 0, false);
	 checked='b';
      } else if (ch1=='\1' && ch2=='f' && ch3=='c' && ch4=='p') {
	 sf=SFFromBDF(fullname, 2, false);
      } else if (ch9=='I' && ch10=='K' && ch3==0 && ch4==55) {
	 /* Ikarus font type appears at word 50 (byte offset 98) */
	 /* Ikarus name section length (at word 2, byte offset 2) was 55 in the 80s at URW */
	 checked='i';
	 sf=SFReadIkarus(fullname);
      }				/* Too hard to figure out a valid mark for a mac resource file */
      if (file != NULL)
	 afclose(file);
   }

   if (sf != NULL)
      /* good */ ;
   else if ((strmatch(fullname + strlen(fullname) - 4, ".sfd")==0 ||
	     strmatch(fullname + strlen(fullname) - 5, ".sfd~")==0)
	    && checked != 'f') {
      sf=SFDRead(fullname);
      fromsfd=true;
   } else if ((strmatch(fullname + strlen(fullname) - 4, ".ttf")==0 ||
	       strmatch(fullname + strlen(strippedname) - 4, ".ttc")==0 ||
	       strmatch(fullname + strlen(fullname) - 4, ".gai")==0 ||
	       strmatch(fullname + strlen(fullname) - 4, ".otf")==0 ||
	       strmatch(fullname + strlen(fullname) - 4, ".otb")==0)
	      && checked != 't') {
      sf=SFReadTTF(fullname, 0, openflags);
   } else if (strmatch(fullname + strlen(strippedname) - 4, ".svg")==0
	      && checked != 'S') {
      sf=SFReadSVG(fullname, 0);
   } else if (strmatch(fullname + strlen(fullname) - 4, ".ufo")==0
	      && checked != 'u') {
      sf=SFReadUFO(fullname, 0);
   } else if (strmatch(fullname + strlen(fullname) - 4, ".bdf")==0
	      && checked != 'b') {
      sf=SFFromBDF(fullname, 0, false);
   } else if (strmatch(fullname + strlen(fullname) - 2, "pk")==0) {
      sf=SFFromBDF(fullname, 1, true);
   } else if (strmatch(fullname + strlen(fullname) - 2, "gf")==0) {
      sf=SFFromBDF(fullname, 3, true);
   } else if (strmatch(fullname + strlen(fullname) - 4, ".pcf")==0 ||
	      strmatch(fullname + strlen(fullname) - 4, ".pmf")==0) {
      /* Sun seems to use a variant of the pcf format which they call pmf */
      /*  the encoding actually starts at 0x2000 and the one I examined was */
      /*  for a pixel size of 200. Some sort of printer font? */
      sf=SFFromBDF(fullname, 2, false);
   } else if (strmatch(fullname + strlen(strippedname) - 4, ".bin")==0 ||
	      strmatch(fullname + strlen(strippedname) - 4, ".hqx")==0 ||
	      strmatch(fullname + strlen(strippedname) - 6, ".dfont")==0) {
      sf=SFReadMacBinary(fullname, 0, openflags);
   } else if (strmatch(fullname + strlen(strippedname) - 4, ".fon")==0 ||
	      strmatch(fullname + strlen(strippedname) - 4, ".fnt")==0) {
      sf=SFReadWinFON(fullname, 0);
   } else if (strmatch(fullname + strlen(strippedname) - 4, ".pdb")==0) {
      sf=SFReadPalmPdb(fullname, 0);
   } else if ((strmatch(fullname + strlen(fullname) - 4, ".pfa")==0 ||
	       strmatch(fullname + strlen(fullname) - 4, ".pfb")==0 ||
	       strmatch(fullname + strlen(fullname) - 4, ".pf3")==0 ||
	       strmatch(fullname + strlen(fullname) - 4, ".cid")==0 ||
	       strmatch(fullname + strlen(fullname) - 4, ".gsf")==0 ||
	       strmatch(fullname + strlen(fullname) - 4, ".pt3")==0 ||
	       strmatch(fullname + strlen(fullname) - 3, ".ps")==0)
	      && checked != 'p') {
      sf=SFReadPostScript(fullname);
   } else if (strmatch(fullname + strlen(fullname) - 4, ".cff")==0
	      && checked != 'c') {
      sf=CFFParse(fullname);
   } else if (strmatch(fullname + strlen(fullname) - 3, ".mf")==0) {
      sf=SFFromMF(fullname);
   } else if (strmatch(strippedname + strlen(strippedname) - 4, ".pdf")==0
	      && checked != 'P') {
      sf=SFReadPdfFont(fullname, openflags);
   } else if (strmatch(fullname + strlen(fullname) - 3, ".ik")==0
	      && checked != 'i') {
      sf=SFReadIkarus(fullname);
   } else {
      sf=SFReadMacBinary(fullname, 0, openflags);
   }

   if (sf != NULL) {
      SplineFont *norm=sf->mm != NULL ? sf->mm->normal : sf;

      if (compression != 0) {
	 free(sf->filename);
	 *strrchr(oldstrippedname, '.')='\0';
	 sf->filename=fastrdup(oldstrippedname);
      }
      if (fromsfd)
	 sf->compression=compression;
      free(norm->origname);
      if (wasarchived) {
	 norm->origname=NULL;
	 free(norm->filename);
	 norm->filename=NULL;
	 norm->new=true;
      } else if (sf->chosenname != NULL && strippedname==filename) {
	 norm->origname =
	    malloc(strlen(filename) + strlen(sf->chosenname) + 8);
	 strcpy(norm->origname, filename);
	 strcat(norm->origname, "(");
	 strcat(norm->origname, sf->chosenname);
	 strcat(norm->origname, ")");
      } else
	 norm->origname=fastrdup(filename);
      free(norm->chosenname);
      norm->chosenname=NULL;
      if (sf->mm != NULL) {
	 int j;

	 for (j=0; j < sf->mm->instance_count; ++j) {
	    free(sf->mm->instances[j]->origname);
	    sf->mm->instances[j]->origname=fastrdup(norm->origname);
	 }
      }
   } else if (!GFileExists(filename))
      ErrorMsg(2,"The requested file, %.100s, does not exist.\n",
                 GFileNameTail(filename));
   else if (!GFileReadable(filename))
      ErrorMsg(2,"No read permission for %.100s\n",GFileNameTail(filename));
   else
      ErrorMsg(2,"The file %.100s is not in a known format, uses "
                 "features FontAnvil does not support, or is so badly "
                 "corrupted as to be unreadable.\n",
               GFileNameTail(filename));

   if (oldstrippedname != filename)
      free(oldstrippedname);
   if (fullname != filename && fullname != strippedname)
      free(fullname);
   if (tmpfile != NULL) {
      unlink(tmpfile);
      free(tmpfile);
   }
   if (wasarchived)
      ArchiveCleanup(archivedir);
   if ((openflags & of_fstypepermitted) && sf != NULL
       && (sf->pfminfo.fstype & 0xff)==0x0002) {
      /* Ok, they have told us from a script they have access to the font */
   } else if (!fromsfd && sf != NULL && (sf->pfminfo.fstype & 0xff)==0x0002) {
      SplineFontFree(sf);
      return NULL;
   }
   return sf;
}

SplineFont *ReadSplineFont(char *filename, enum openflags openflags) {
   return (_ReadSplineFont(NULL, filename, openflags));
}

char *ToAbsolute(char *filename) {
   char buffer[1025];

   GFileGetAbsoluteName(filename, buffer, sizeof(buffer));
   return (fastrdup(buffer));
}

SplineFont *LoadSplineFont(char *filename, enum openflags openflags) {
   SplineFont *sf;
   char *pt, *ept, *tobefreed1=NULL, *tobefreed2=NULL;
   static char *extens[] =
      { ".sfd", ".pfa", ".pfb", ".ttf", ".otf", ".ps", ".cid", ".bin",
".dfont", ".PFA", ".PFB", ".TTF", ".OTF", ".PS", ".CID", ".BIN", ".DFONT", NULL };
   int i;

   if (filename==NULL)
      return (NULL);

   if ((pt=strrchr(filename, '/'))==NULL)
      pt=filename;
   if (strchr(pt, '.')==NULL) {
      /* They didn't give an extension. If there's a file with no extension */
      /*  see if it's a valid font file (and if so use the extensionless */
      /*  filename), otherwise guess at an extension */
      /* For some reason Adobe distributes CID keyed fonts (both OTF and */
      /*  postscript) as extensionless files */
      int ok=false;
      AFILE *test=afopen(filename, "rb");

      if (test != NULL) {
	 ok=true;		/* Mac resource files are too hard to check for */
	 /* If file exists, assume good */
	 afclose(test);
      }
      if (!ok) {
	 tobefreed1=malloc(strlen(filename) + 8);
	 strcpy(tobefreed1, filename);
	 ept=tobefreed1 + strlen(tobefreed1);
	 for (i=0; extens[i] != NULL; ++i) {
	    strcpy(ept, extens[i]);
	    if (GFileExists(tobefreed1))
	       break;
	 }
	 if (extens[i] != NULL)
	    filename=tobefreed1;
	 else {
	    free(tobefreed1);
	    tobefreed1=NULL;
	 }
      }
   } else
      tobefreed1=NULL;

   sf=NULL;
   sf=FontWithThisFilename(filename);
   if (sf==NULL && *filename != '/')
      filename=tobefreed2=ToAbsolute(filename);

   if (sf==NULL)
      sf=ReadSplineFont(filename, openflags);

   free(tobefreed1);
   free(tobefreed2);
   return (sf);
}

/* Use URW 4 letter abbreviations */
const char *knownweights[]={ "Demi", "Bold", "Regu", "Medi", "Book", "Thin",
   "Ligh", "Heav", "Blac", "Ultr", "Nord", "Norm", "Gras", "Stan", "Halb",
   "Fett", "Mage", "Mitt", "Buch", NULL
};

const char *realweights[] =
   { "Demi", "Bold", "Regular", "Medium", "Book", "Thin",
   "Light", "Heavy", "Black", "Ultra", "Nord", "Normal", "Gras", "Standard",
      "Halbfett",
   "Fett", "Mager", "Mittel", "Buchschrift", NULL
};
static const char *moreweights[]={ "ExtraLight","VeryLight",NULL };
static const char *modifierlist[] =
   { "Ital", "Obli", "Kursive", "Cursive", "Slanted",
   "Expa", "Cond", NULL
};

static const char *modifierlistfull[] =
   { "Italic", "Oblique", "Kursive", "Cursive", "Slanted",
   "Expanded", "Condensed", NULL
};
static const char **mods[]={ knownweights,modifierlist,NULL };
static const char **fullmods[]={ realweights,modifierlistfull,NULL };

char *_GetModifiers(char *fontname, char *familyname, char *weight) {
   char *pt, *fpt;
   static char space[20];
   int i, j;

   /* URW fontnames don't match the familyname */
   /* "NimbusSanL-Regu" vs "Nimbus Sans L" (note "San" vs "Sans") */
   /* so look for a '-' if there is one and use that as the break point... */

   if ((fpt=strchr(fontname, '-')) != NULL) {
      ++fpt;
      if (*fpt=='\0')
	 fpt=NULL;
   } else if (familyname != NULL) {
      for (pt=fontname, fpt=familyname; *fpt != '\0' && *pt != '\0';) {
	 if (*fpt==*pt) {
	    ++fpt;
	    ++pt;
	 } else if (*fpt==' ')
	    ++fpt;
	 else if (*pt==' ')
	    ++pt;
	 else if (*fpt=='a' || *fpt=='e' || *fpt=='i' || *fpt=='o'
		  || *fpt=='u')
	    ++fpt;		/* allow vowels to be omitted from family when in fontname */
	 else
	    break;
      }
      if (*fpt=='\0' && *pt != '\0')
	 fpt=pt;
      else
	 fpt=NULL;
   }

   if (fpt==NULL) {
      for (i=0; mods[i] != NULL; ++i)
	 for (j=0; mods[i][j] != NULL; ++j) {
	    pt=strstr(fontname, mods[i][j]);
	    if (pt != NULL && (fpt==NULL || pt < fpt))
	       fpt=pt;
	 }
   }
   if (fpt != NULL) {
      for (i=0; mods[i] != NULL; ++i)
	 for (j=0; mods[i][j] != NULL; ++j) {
	    if (strcmp(fpt, mods[i][j])==0) {
	       strncpy(space, fullmods[i][j], sizeof(space) - 1);
	       return (space);
	    }
	 }
      if (strcmp(fpt, "BoldItal")==0)
	 return ("BoldItalic");
      else if (strcmp(fpt, "BoldObli")==0)
	 return ("BoldOblique");

      return (fpt);
   }

   return (weight==NULL || *weight=='\0' ? "Regular" : weight);
}

char *SFGetModifiers(SplineFont *sf) {
   return (_GetModifiers(sf->fontname, sf->familyname, sf->weight));
}

enum flatness { mt_flat, mt_round, mt_pointy, mt_unknown };

static bigreal SPLMaxHeight(SplineSet *spl,enum flatness *isflat) {
   enum flatness f=mt_unknown;
   bigreal max=-1.0e23;
   Spline *s, *first;
   extended ts[2];
   int i;

   for (; spl != NULL; spl=spl->next) {
      first=NULL;
      for (s=spl->first->next; s != first && s != NULL; s=s->to->next) {
	 if (first==NULL)
	    first=s;
	 if (s->from->me.y >= max ||
	     s->to->me.y >= max ||
	     s->from->nextcp.y > max || s->to->prevcp.y > max) {
	    if (!s->knownlinear) {
	       if (s->from->me.y > max) {
		  f=mt_round;
		  max=s->from->me.y;
	       }
	       if (s->to->me.y > max) {
		  f=mt_round;
		  max=s->to->me.y;
	       }
	       SplineFindExtrema(&s->splines[1], &ts[0], &ts[1]);
	       for (i=0; i < 2; ++i)
		  if (ts[i] != -1) {
		     bigreal y =
			((s->splines[1].a * ts[i] + s->splines[1].b) * ts[i] +
			 s->splines[1].c) * ts[i] + s->splines[1].d;
		     if (y > max) {
			f=mt_round;
			max=y;
		     }
		  }
	    } else if (s->from->me.y==s->to->me.y) {
	       if (s->from->me.y >= max) {
		  max=s->from->me.y;
		  f=mt_flat;
	       }
	    } else {
	       if (s->from->me.y > max) {
		  f=mt_pointy;
		  max=s->from->me.y;
	       }
	       if (s->to->me.y > max) {
		  f=mt_pointy;
		  max=s->to->me.y;
	       }
	    }
	 }
      }
   }
   *isflat=f;
   return (max);
}

static bigreal SCMaxHeight(SplineChar *sc,int layer,enum flatness *isflat) {
   /* Find the max height of this layer of the glyph. Also find whether that */
   /* max is flat (as in "z", curved as in "o" or pointy as in "A") */
   enum flatness f=mt_unknown, curf;
   bigreal max=-1.0e23, test;
   RefChar *r;

   max=SPLMaxHeight(sc->layers[layer].splines, &curf);
   f=curf;
   for (r=sc->layers[layer].refs; r != NULL; r=r->next) {
      test=SPLMaxHeight(r->layers[0].splines, &curf);
      if (test > max || (test==max && curf==mt_flat)) {
	 max=test;
	 f=curf;
      }
   }
   *isflat=f;
   return (max);
}

static bigreal SPLMinHeight(SplineSet *spl,enum flatness *isflat) {
   enum flatness f=mt_unknown;
   bigreal min=1.0e23;
   Spline *s, *first;
   extended ts[2];
   int i;

   for (; spl != NULL; spl=spl->next) {
      first=NULL;
      for (s=spl->first->next; s != first && s != NULL; s=s->to->next) {
	 if (first==NULL)
	    first=s;
	 if (s->from->me.y <= min ||
	     s->to->me.y <= min ||
	     s->from->nextcp.y < min || s->to->prevcp.y < min) {
	    if (!s->knownlinear) {
	       if (s->from->me.y < min) {
		  f=mt_round;
		  min=s->from->me.y;
	       }
	       if (s->to->me.y < min) {
		  f=mt_round;
		  min=s->to->me.y;
	       }
	       SplineFindExtrema(&s->splines[1], &ts[0], &ts[1]);
	       for (i=0; i < 2; ++i)
		  if (ts[i] != -1) {
		     bigreal y =
			((s->splines[1].a * ts[i] + s->splines[1].b) * ts[i] +
			 s->splines[1].c) * ts[i] + s->splines[1].d;
		     if (y < min) {
			f=mt_round;
			min=y;
		     }
		  }
	    } else if (s->from->me.y==s->to->me.y) {
	       if (s->from->me.y <= min) {
		  min=s->from->me.y;
		  f=mt_flat;
	       }
	    } else {
	       if (s->from->me.y < min) {
		  f=mt_pointy;
		  min=s->from->me.y;
	       }
	       if (s->to->me.y < min) {
		  f=mt_pointy;
		  min=s->to->me.y;
	       }
	    }
	 }
      }
   }
   *isflat=f;
   return (min);
}

static bigreal SCMinHeight(SplineChar *sc,int layer,enum flatness *isflat) {
   /* Find the min height of this layer of the glyph. Also find whether that */
   /* min is flat (as in "z", curved as in "o" or pointy as in "A") */
   enum flatness f=mt_unknown, curf;
   bigreal min=1.0e23, test;
   RefChar *r;

   min=SPLMinHeight(sc->layers[layer].splines, &curf);
   f=curf;
   for (r=sc->layers[layer].refs; r != NULL; r=r->next) {
      test=SPLMinHeight(r->layers[0].splines, &curf);
      if (test < min || (test==min && curf==mt_flat)) {
	 min=test;
	 f=curf;
      }
   }
   *isflat=f;
   return (min);
}

#define RANGE	0x40ffffff

struct dimcnt {
   bigreal pos;
   int cnt;
};

static int dclist_insert(struct dimcnt *arr,int cnt,bigreal val) {
   int i;

   for (i=0; i < cnt; ++i) {
      if (arr[i].pos==val) {
	 ++arr[i].cnt;
	 return (cnt);
      }
   }
   arr[i].pos=val;
   arr[i].cnt=1;
   return (i + 1);
}

static bigreal SFStandardHeight(SplineFont *sf,int layer,int do_max,
				unichar_t * list) {
   struct dimcnt flats[200], curves[200];
   bigreal test;
   enum flatness curf;
   int fcnt=0, ccnt=0, cnt, tot, i, useit;
   unichar_t ch, top;
   bigreal result, bestheight, bestdiff, diff, val;
   char *blues, *end;

   while (*list) {
      ch=top=*list;
      if (list[1]==RANGE && list[2] != 0) {
	 list += 2;
	 top=*list;
      }
      for (; ch <= top; ++ch) {
	 SplineChar *sc=SFGetChar(sf, ch, NULL);

	 if (sc != NULL) {
	    if (do_max)
	       test=SCMaxHeight(sc, layer, &curf);
	    else
	       test=SCMinHeight(sc, layer, &curf);
	    if (curf==mt_flat)
	       fcnt=dclist_insert(flats, fcnt, test);
	    else if (curf != mt_unknown)
	       ccnt=dclist_insert(curves, ccnt, test);
	 }
      }
      ++list;
   }

   /* All flat surfaces at tops of glyphs are at the same level */
   if (fcnt==1)
      result=flats[0].pos;
   else if (fcnt > 1) {
      cnt=0;
      for (i=0; i < fcnt; ++i) {
	 if (flats[i].cnt > cnt)
	    cnt=flats[i].cnt;
      }
      test=0;
      tot=0;
      /* find the mode. If multiple values have the same high count, average them */
      for (i=0; i < fcnt; ++i) {
	 if (flats[i].cnt==cnt) {
	    test += flats[i].pos;
	    ++tot;
	 }
      }
      result=test / tot;
   } else if (ccnt==0)
      return (do_max ? -1e23 : 1e23);	/* We didn't find any glyphs */
   else {
      /* Italic fonts will often have no flat surfaces for x-height just wavies */
      test=0;
      tot=0;
      /* find the mean */
      for (i=0; i < ccnt; ++i) {
	 test += curves[i].pos;
	 ++tot;
      }
      result=test / tot;
   }

   /* Do we have a BlueValues entry? */
   /* If so, snap height to the closest alignment zone (bottom of the zone) */
   if (sf->private != NULL
       && (blues =
	   PSDictHasEntry(sf->private,
			  do_max ? "BlueValues" : "OtherBlues")) != NULL) {
      while (*blues==' ' || *blues=='[')
	 ++blues;
      /* Must get at least this close, else we'll just use what we found */
      bestheight=result;
      bestdiff=(sf->ascent + sf->descent) / 100.0;
      useit=true;
      while (*blues != '\0' && *blues != ']') {
	 val=strtod(blues, &end);
	 if (blues==end)
	    break;
	 blues=end;
	 while (*blues==' ')
	    ++blues;
	 if (useit) {
	    if ((diff=val - result) < 0)
	       diff=-diff;
	    if (diff < bestdiff) {
	       bestheight=val;
	       bestdiff=diff;
	    }
	 }
	 useit=!useit;	/* Only interested in every other BV entry */
      }
      result=bestheight;
   }
   return (result);
}

static unichar_t capheight_str[]={ 'A',RANGE,'Z',
   0x391, RANGE, 0x3a9,
   0x402, 0x404, 0x405, 0x406, 0x408, RANGE, 0x40b, 0x40f, RANGE, 0x418,
      0x41a, 0x42f,
   0
};

static unichar_t xheight_str[] =
   { 'a', 'c', 'e', 'g', 'm', 'n', 'o', 'p', 'q', 'r', 's', 'u', 'v', 'w',
'x', 'y', 'z', 0x131,
   0x3b3, 0x3b9, 0x3ba, 0x3bc, 0x3bd, 0x3c0, 0x3c3, 0x3c4, 0x3c5, 0x3c7,
      0x3c8, 0x3c9,
   0x432, 0x433, 0x438, 0x43a, RANGE, 0x43f, 0x442, 0x443, 0x445, 0x44c,
      0x44f, 0x459, 0x45a,
   0
};

static unichar_t ascender_str[]={ 'b','d','f','h','k','l',
   0x3b3, 0x3b4, 0x3b6, 0x3b8,
   0x444, 0x452,
   0
};

static unichar_t descender_str[]={ 'g','j','p','q','y',
   0x3b2, 0x3b3, 0x3c7, 0x3c8,
   0x434, 0x440, 0x443, 0x444, 0x452, 0x458,
   0
};

bigreal SFCapHeight(SplineFont *sf, int layer, int return_error) {
   bigreal result=SFStandardHeight(sf, layer, true, capheight_str);

   if (result==-1e23 && !return_error)
      result=(8 * sf->ascent) / 10;
   return (result);
}

bigreal SFXHeight(SplineFont *sf, int layer, int return_error) {
   bigreal result=SFStandardHeight(sf, layer, true, xheight_str);

   if (result==-1e23 && !return_error)
      result=(6 * sf->ascent) / 10;
   return (result);
}

bigreal SFAscender(SplineFont *sf, int layer, int return_error) {
   bigreal result=SFStandardHeight(sf, layer, true, ascender_str);

   if (result==-1e23 && !return_error)
      result=(81 * sf->ascent) / 100;
   return (result);
}

bigreal SFDescender(SplineFont *sf, int layer, int return_error) {
   bigreal result=SFStandardHeight(sf, layer, false, descender_str);

   if (result==1e23 && !return_error)
      result=-sf->descent / 2;
   return (result);
}

static void arraystring(char *buffer,real *array,int cnt) {
   int i, ei;

   for (ei=cnt; ei > 1 && array[ei - 1]==0; --ei);
   *buffer++='[';
   for (i=0; i < ei; ++i) {
      sprintf(buffer, "%d ", (int) array[i]);
      buffer += strlen(buffer);
   }
   if (buffer[-1]==' ')
      --buffer;
   *buffer++=']';
   *buffer='\0';
}

static void SnapSet(struct psdict *private,real stemsnap[12],
		    real snapcnt[12], char *name1, char *name2, int which) {
   int i, mi;
   char buffer[211];

   mi=-1;
   for (i=0; i < 12 && stemsnap[i] != 0; ++i)
      if (mi==-1)
	 mi=i;
      else if (snapcnt[i] > snapcnt[mi])
	 mi=i;
   if (mi==-1)
      return;
   if (which < 2) {
      sprintf(buffer, "[%d]", (int) stemsnap[mi]);
      PSDictChangeEntry(private, name1, buffer);
   }
   if (which==0 || which==2) {
      arraystring(buffer, stemsnap, 12);
      PSDictChangeEntry(private, name2, buffer);
   }
}

int SFPrivateGuess(SplineFont *sf, int layer, struct psdict *private,
		   char *name, int onlyone) {
   real bluevalues[14], otherblues[10];
   real snapcnt[12];
   real stemsnap[12];
   char buffer[211];
   char *oldloc;
   int ret;

   oldloc=fastrdup(setlocale(LC_NUMERIC, NULL));
   setlocale(LC_NUMERIC, "C");
   ret=true;

   if (strcmp(name, "BlueValues")==0 || strcmp(name, "OtherBlues")==0) {
      FindBlues(sf, layer, bluevalues, otherblues);
      if (!onlyone || strcmp(name, "BlueValues")==0) {
	 arraystring(buffer, bluevalues, 14);
	 PSDictChangeEntry(private, "BlueValues", buffer);
      }
      if (!onlyone || strcmp(name, "OtherBlues")==0) {
	 if (otherblues[0] != 0 || otherblues[1] != 0) {
	    arraystring(buffer, otherblues, 10);
	    PSDictChangeEntry(private, "OtherBlues", buffer);
	 } else
	    PSDictRemoveEntry(private, "OtherBlues");
      }
   } else if (strcmp(name, "StdHW")==0 || strcmp(name, "StemSnapH")==0) {
      FindHStems(sf, stemsnap, snapcnt);
      SnapSet(private, stemsnap, snapcnt, "StdHW", "StemSnapH",
	      !onlyone ? 0 : strcmp(name, "StdHW")==0 ? 1 : 0);
   } else if (strcmp(name, "StdVW")==0 || strcmp(name, "StemSnapV")==0) {
      FindVStems(sf, stemsnap, snapcnt);
      SnapSet(private, stemsnap, snapcnt, "StdVW", "StemSnapV",
	      !onlyone ? 0 : strcmp(name, "StdVW")==0 ? 1 : 0);
   } else if (strcmp(name, "BlueScale")==0) {
      bigreal val=-1;

      if (PSDictFindEntry(private, "BlueValues") != -1) {
	 /* Can guess BlueScale if we've got a BlueValues */
	 val=BlueScaleFigureForced(private, NULL, NULL);
      }
      if (val==-1)
	 val=.039625;
      sprintf(buffer, "%g", (double) val);
      PSDictChangeEntry(private, "BlueScale", buffer);
   } else if (strcmp(name, "BlueShift")==0) {
      PSDictChangeEntry(private, "BlueShift", "7");
   } else if (strcmp(name, "BlueFuzz")==0) {
      PSDictChangeEntry(private, "BlueFuzz", "1");
   } else if (strcmp(name, "ForceBold")==0) {
      int isbold=false;

      if (sf->weight != NULL &&
	  (strstrmatch(sf->weight, "Bold") != NULL ||
	   strstrmatch(sf->weight, "Heavy") != NULL ||
	   strstrmatch(sf->weight, "Black") != NULL ||
	   strstrmatch(sf->weight, "Grass") != NULL ||
	   strstrmatch(sf->weight, "Fett") != NULL))
	 isbold=true;
      if (sf->pfminfo.pfmset && sf->pfminfo.weight >= 700)
	 isbold=true;
      PSDictChangeEntry(private, "ForceBold", isbold ? "true" : "false");
   } else if (strcmp(name, "LanguageGroup")==0) {
      PSDictChangeEntry(private, "LanguageGroup", "0");
   } else if (strcmp(name, "ExpansionFactor")==0) {
      PSDictChangeEntry(private, "ExpansionFactor", "0.06");
   } else
      ret=false;

   setlocale(LC_NUMERIC, oldloc);
   free(oldloc);
   return (ret);
}

void SPLFirstVisitSplines(SplinePoint * splfirst,
			  SPLFirstVisitSplinesVisitor f, void *udata) {
   Spline *spline=0;
   Spline *first=0;
   Spline *next=0;

   if (splfirst != NULL) {
      first=NULL;
      for (spline=splfirst->next; spline != NULL && spline != first;
	   spline=next) {
	 next=spline->to->next;

	 // callback
	 f(splfirst, spline, udata);

	 if (first==NULL) {
	    first=spline;
	 }
      }
   }
}

typedef struct SPLFirstVisitorFoundSoughtDataS {
   SplinePoint *sought;
   int found;
} SPLFirstVisitorFoundSoughtData;

typedef struct SPLFirstVisitorFoundSoughtXYDataS {
   int use_x;
   int use_y;
   real x;
   real y;

   // outputs
   int found;
   Spline *spline;
   SplinePoint *sp;

} SPLFirstVisitorFoundSoughtXYData;

static void SPLFirstVisitorFoundSoughtXY(SplinePoint *splfirst,
					 Spline * spline, void *udata) {
   SPLFirstVisitorFoundSoughtXYData *d =
      (SPLFirstVisitorFoundSoughtXYData *) udata;
   int found=0;

   if (d->found)
      return;

   if (d->use_x) {
      if (spline->from->me.x==d->x) {
	 found=1;
	 d->spline=spline;
	 d->sp=spline->from;
      }

      if (spline->to->me.x==d->x) {
	 found=1;
	 d->spline=spline;
	 d->sp=spline->to;
      }
   }
   if (d->use_x && found && d->use_y) {
      if (d->sp->me.y != d->y) {
	 found=0;
      }
   } else if (d->use_y) {
      if (spline->from->me.y==d->y) {
	 found=1;
	 d->spline=spline;
	 d->sp=spline->from;
      }

      if (spline->to->me.y==d->y) {
	 found=1;
	 d->spline=spline;
	 d->sp=spline->to;
      }
   }

   if (found) {
      d->found=found;
      d->spline=spline;
   } else {
      d->sp=0;
   }
}

SplinePoint *SplinePointListContainsPointAtX(SplinePointList * container,
					     real x) {
   SplinePointList *spl;

   for (spl=container; spl != NULL; spl=spl->next) {
      SPLFirstVisitorFoundSoughtXYData d;

      d.use_x=1;
      d.use_y=0;
      d.x=x;
      d.y=0;
      d.found=0;
      SPLFirstVisitSplines(spl->first, SPLFirstVisitorFoundSoughtXY, &d);
      if (d.found)
	 return d.sp;
   }
   return 0;
}
