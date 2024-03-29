/* $Id: scripting.c 4300 2015-10-24 13:03:29Z mskala $ */
/* Copyright (C) 2002-2012  George Williams
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

#include "fontanvil.h"
#include <gfile.h>
#include <utype.h>
#include <ustring.h>
#include <chardata.h>
#include <unistd.h>
#include <math.h>
#include <setjmp.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#ifdef HAVE_IEEEFP_H
#   include <ieeefp.h> /* Solaris defines isnan in ieeefp rather than math.h */
#endif
#ifndef _NO_LIBREADLINE
#   include <readline/readline.h>
#   include <readline/history.h>
#endif
#include "ttf.h"
#include "scripting.h"
#include "scriptfuncs.h"
#include "flaglist.h"
#include "views.h"

#include "unicodelibinfo.h"

int running_script=true;

static int verbose=-1;

static struct dictionary globals;

static struct keywords {
   enum token_type tok;
   char *name;
} keywords[]={
   {
   tt_if, "if"}, {
   tt_else, "else"}, {
   tt_elseif, "elseif"}, {
   tt_endif, "endif"}, {
   tt_while, "while"}, {
   tt_foreach, "foreach"}, {
   tt_endloop, "endloop"}, {
   tt_shift, "shift"}, {
   tt_return, "return"}, {
   tt_break, "break"}, {
   0, NULL}
};

static const char *toknames[]={
   "name", "string", "number", "unicode id", "real number",
   "lparen", "rparen", "comma", "end of ff_statement",
   "lbracket", "rbracket",
   "minus", "plus", "logical not", "bitwise not", "colon",
   "multiply", "divide", "mod", "logical and", "logical or", "bitwise and",
      "bitwise or", "bitwise xor",
   "equal to", "not equal to", "greater than", "less than",
      "greater than or equal to", "less than or equal to",
   "assignment", "plus equals", "minus equals", "mul equals", "div equals",
      "mod equals",
   "increment", "decrement",
   "if", "else", "elseif", "endif", "while", "foreach", "endloop",
   "shift", "return",
   "End Of File",
   NULL
};

static void ff_statement(Context *c);
static char **GetFontNames(char *filename);

static char *utf82script_copy(const char *ustr) {
   return fastrdup(ustr);
}

static char *script2utf8_copy(const char *str) {
   return fastrdup(str);
}

static char *script2latin1_copy(const char *str) {
   unichar_t *t=utf82u_copy(str);
   char *ret=cu_copy(t);

   free(t);
   return (ret);
}

void arrayfree(Array * a) {
   int i;

   for (i=0; i < a->argc; ++i) {
      if (a->vals[i].type==v_str)
	 free(a->vals[i].u.sval);
      else if (a->vals[i].type==v_arr)
	 arrayfree(a->vals[i].u.aval);
   }
   free(a->vals);
   free(a);
}

static Array *arraycopy(Array *a) {
   int i;
   Array *c;

   c=malloc(sizeof(Array));
   c->argc=a->argc;
   c->vals=malloc(c->argc * sizeof(Val));
   memcpy(c->vals, a->vals, c->argc * sizeof(Val));
   for (i=0; i < a->argc; ++i) {
      if (a->vals[i].type==v_str)
	 c->vals[i].u.sval=fastrdup(a->vals[i].u.sval);
      else if (a->vals[i].type==v_arr)
	 c->vals[i].u.aval=arraycopy(a->vals[i].u.aval);
   }
   return (c);
}

static void array_copy_into(Array *dest,int offset,Array *src) {
   int i;

   memcpy(dest->vals + offset, src->vals, src->argc * sizeof(Val));
   for (i=0; i < src->argc; ++i) {
      if (src->vals[i].type==v_str)
	 dest->vals[i + offset].u.sval=fastrdup(src->vals[i].u.sval);
      else if (src->vals[i].type==v_arr)
	 dest->vals[i + offset].u.aval=arraycopy(src->vals[i].u.aval);
   }
}

void DictionaryFree(struct dictionary *dica) {
   int i;

   if (dica==NULL)
      return;

   for (i=0; i < dica->cnt; ++i) {
      free(dica->entries[i].name);
      if (dica->entries[i].val.type==v_str)
	 free(dica->entries[i].val.u.sval);
      if (dica->entries[i].val.type==v_arr)
	 arrayfree(dica->entries[i].val.u.aval);
   }
   free(dica->entries);
}

static int DicaLookup(struct dictionary *dica,char *name,Val *val) {
   int i;

   if (dica != NULL && dica->entries != NULL) {
      for (i=0; i < dica->cnt; ++i)
	 if (strcmp(dica->entries[i].name, name)==0) {
	    val->type=v_lval;
	    val->u.lval=&dica->entries[i].val;
	    return (true);
	 }
   }
   return (false);
}

static void DicaNewEntry(struct dictionary *dica,char *name,Val *val) {
   if (dica->entries==NULL) {
      dica->max=10;
      dica->entries=malloc(dica->max * sizeof(struct dictentry));
   } else if (dica->cnt >= dica->max) {
      dica->max += 10;
      dica->entries =
	 realloc(dica->entries, dica->max * sizeof(struct dictentry));
   }
   dica->entries[dica->cnt].name=fastrdup(name);
   dica->entries[dica->cnt].val.type=v_void;
   val->type=v_lval;
   val->u.lval=&dica->entries[dica->cnt].val;
   ++dica->cnt;
}

static void calldatafree(Context *c) {
   int i;

   for (i=1; i < c->a.argc; ++i) {	/* child may have freed some args itself by shifting, but argc will reflect the proper values none the less */
      if (c->a.vals[i].type==v_str)
	 free(c->a.vals[i].u.sval);
      if (c->a.vals[i].type==v_arrfree
	  || (c->a.vals[i].type==v_arr
	      && c->dontfree[i] != c->a.vals[i].u.aval))
	 arrayfree(c->a.vals[i].u.aval);
      c->a.vals[i].type=v_void;
   }
   DictionaryFree(&c->locals);

   if (c->script != NULL) {
      afclose(c->script);
      c->script=NULL;
   }
}

static void traceback(Context *c) {
   int cnt=0;

   while (c != NULL) {
      if (c->interactive) {
	 if (c->err_env != NULL) {
	    longjmp(*c->err_env, 1);
	 }
	 c->error=true;
	 return;
      }
      if (cnt==1)
	 ErrorMsg(2,"Called from...\n");
      if (cnt > 0)
	 ErrorMsg(2," %s: line %d\n", c->filename, c->lineno);
      calldatafree(c);
      if (c->err_env != NULL)
	 longjmp(*c->err_env, 1);
      c=c->caller;
      ++cnt;
   }
   exit(1);
}

static void showtoken(Context *c,enum token_type got) {
   if (got==tt_name || got==tt_string)
      ErrorMsg(2," \"%s\"\n", c->tok_text);
   else if (got==tt_number)
      ErrorMsg(2," %d (0x%x)\n", c->tok_val.u.ival, c->tok_val.u.ival);
   else if (got==tt_unicode)
      ErrorMsg(2," 0u%x\n", c->tok_val.u.ival);
   else if (got==tt_real)
      ErrorMsg(2," %g\n", c->tok_val.u.fval);
   else
      ErrorMsg(2,"\n");
   traceback(c);
}

static void expect(Context *c,enum token_type expected,enum token_type got) {
   if (got != expected) {
      if (verbose > 0)
	 fflush(stdout);
      if (c->interactive)
	 ErrorMsg(2,"Error: Expected %s, got %s\n",
		  toknames[expected], toknames[got]);
      else
	 ErrorMsg(2,"%s: %d Expected %s, got %s\n",
		  c->filename, c->lineno, toknames[expected], toknames[got]);
      showtoken(c, got);
   }
}

static void unexpected(Context *c,enum token_type got) {
   if (verbose > 0)
      fflush(stdout);
   if (c->interactive)
      ErrorMsg(2,"Error: Unexpected %s found\n", toknames[got]);
   else
      ErrorMsg(2,"%s: %d Unexpected %s found\n",
	       c->filename, c->lineno, toknames[got]);
   showtoken(c, got);
}

void ScriptError(Context * c, const char *msg) {
   char *t1=script2utf8_copy(msg);
   char *ufile=fastrdup(c->filename);

   /* All of fontanvil's internal errors are in ASCII where there is */
   /* no difference between latin1 and utf8. User errors are a different */
   /* matter */

   if (verbose > 0)
      fflush(stdout);
   if (c->interactive)
      ErrorMsg(2,"Error: %s\n", t1);
   else if (c->lineno != 0)
      ErrorMsg(2,"%s line: %d %s\n", ufile, c->lineno, t1);
   else
      ErrorMsg(2,"%s: %s\n", ufile, t1);
   free(ufile);
   free(t1);
   traceback(c);
}

void ScriptErrorString(Context * c, const char *msg, const char *name) {
   char *t1=script2utf8_copy(msg);
   char *t2=script2utf8_copy(name);
   char *ufile=fastrdup(c->filename);

   if (verbose > 0)
      fflush(stdout);
   if (c->interactive)
      ErrorMsg(2,"Error: %s: %s\n", t1, t2);
   else if (c->lineno != 0)
      ErrorMsg(2,"%s line: %d %s: %s\n", ufile, c->lineno, t1, t2);
   else
      ErrorMsg(2,"%s: %s: %s\n", ufile, t1, t2);
   free(ufile);
   free(t1);
   free(t2);
   traceback(c);
}

void ScriptErrorF(Context * c, const char *format, ...) {
   char *ufile=fastrdup(c->filename);

   /* All string arguments here assumed to be utf8 */
   char errbuf[400];
   va_list ap;

   va_start(ap, format);
   vsnprintf(errbuf, sizeof(errbuf), format, ap);
   va_end(ap);

   if (verbose > 0)
      fflush(stdout);
   if (c->interactive)
      ErrorMsg(2,"Error: %s\n", errbuf);
   else if (c->lineno != 0)
      ErrorMsg(2,"%s line: %d %s\n", ufile, c->lineno, errbuf);
   else
      ErrorMsg(2,"%s: %s\n", ufile, errbuf);
   free(ufile);
   traceback(c);
}

static char *forcePSName_copy(Context *c,char *str) {
   char *pt;

   for (pt=str; *pt; ++pt) {
      if (*pt <= ' ' || *pt >= 0x7f ||
	  *pt=='(' || *pt==')' ||
	  *pt=='[' || *pt==']' ||
	  *pt=='{' || *pt=='}' ||
	  *pt=='<' || *pt=='>' || *pt=='%' || *pt=='/')
	 ScriptErrorString(c,
			   "Invalid character in PostScript name token (probably fontname): ",
			   str);
   }
   return (fastrdup(str));
}

static char *forceASCIIcopy(Context *c,char *str) {
   char *pt;

   for (pt=str; *pt; ++pt) {
      if (*pt < ' ' || *pt >= 0x7f)
	 ScriptErrorString(c, "Invalid ASCII character in: ", str);
   }
   return (fastrdup(str));
}

static void dereflvalif(Val *val) {
   if (val->type==v_lval) {
      *val=*val->u.lval;
      if (val->type==v_str)
	 val->u.sval=fastrdup(val->u.sval);
   }
}

static void PrintVal(Val *val) {
   int j;

   if (val->type==v_str) {
      char *t1=script2utf8_copy(val->u.sval);
      char *loc=fastrdup(t1);

      printf("%s", loc);
      free(loc);
      free(t1);
   } else if (val->type==v_arr || val->type==v_arrfree) {
      putchar('[');
      if (val->u.aval->argc > 0) {
	 PrintVal(&val->u.aval->vals[0]);
	 for (j=1; j < val->u.aval->argc; ++j) {
	    putchar(',');
	    if (val->u.aval->vals[j - 1].type==v_arr
		|| val->u.aval->vals[j - 1].type==v_arrfree)
	       putchar('\n');
	    PrintVal(&val->u.aval->vals[j]);
	 }
      }
      putchar(']');
   } else if (val->type==v_int)
      printf("%d", val->u.ival);
   else if (val->type==v_unicode)
      printf("0u%04X", val->u.ival);
   else if (val->type==v_real)
      printf("%g", (double) val->u.fval);
   else if (val->type==v_void)
      printf("<void>");
   else
      printf("<" "???" ">");
}

static int GetOneSelCharIndex(Context *c) {
   FontViewBase *fv=c->curfv;
   EncMap *map=fv->map;
   int i, found=-1;

   for (i=0; i < map->enccount; ++i)
      if (fv->selected[i]) {
	 if (found==-1)
	    found=i;
	 else
	    ScriptError(c, "More than one character selected");
      }
   if (found==-1)
      ScriptError(c, "No characters selected");
   return (found);
}

static SplineChar *GetOneSelChar(Context *c) {
   int found=GetOneSelCharIndex(c);

   return (SFMakeChar(c->curfv->sf, c->curfv->map, found));
}

static void prterror(void *foo,char *msg,int pos) {
   ErrorMsg(2,"%s\n", msg);
}

static void TableAddInstrs(SplineFont *sf,uint32_t tag,int replace,
			   uint8_t * instrs, int icnt) {
   struct ttf_table *tab;

   for (tab=sf->ttf_tables; tab != NULL && tab->tag != tag;
	tab=tab->next);

   if (replace && tab != NULL) {
      free(tab->data);
      tab->data=NULL;
      tab->len=tab->maxlen=0;
   }
   if (icnt==0)
      return;
   if (tab==NULL) {
      tab=chunkalloc(sizeof(struct ttf_table));
      tab->tag=tag;
      tab->next=sf->ttf_tables;
      sf->ttf_tables=tab;
   }
   if (tab->data==NULL) {
      tab->data=malloc(icnt);
      memcpy(tab->data, instrs, icnt);
      tab->len=icnt;
   } else {
      uint8_t *newi=malloc(icnt + tab->len);

      memcpy(newi, tab->data, tab->len);
      memcpy(newi + tab->len, instrs, icnt);
      free(tab->data);
      tab->data=newi;
      tab->len += icnt;
   }
   tab->maxlen=tab->len;
}

static void GlyphAddInstrs(SplineChar *sc,int replace,
			   uint8_t * instrs, int icnt) {

   if (replace) {
      free(sc->ttf_instrs);
      sc->ttf_instrs=NULL;
      sc->ttf_instrs_len=0;
   }
   sc->instructions_out_of_date=false;
   if (icnt==0)
      return;
   if (sc->ttf_instrs==NULL) {
      SCNumberPoints(sc, ly_fore);	/* If the point numbering is wrong then we'll just throw away the instructions when we notice it */
      sc->ttf_instrs=malloc(icnt);
      memcpy(sc->ttf_instrs, instrs, icnt);
      sc->ttf_instrs_len=icnt;
   } else {
      uint8_t *newi=malloc(icnt + sc->ttf_instrs_len);

      memcpy(newi, sc->ttf_instrs, sc->ttf_instrs_len);
      memcpy(newi + sc->ttf_instrs_len, instrs, icnt);
      free(sc->ttf_instrs);
      sc->ttf_instrs=newi;
      sc->ttf_instrs_len += icnt;
   }
}

static int32_t ParseTag(Context *c,Val *tagstr,int macok,int *wasmac) {
   char tag[4];
   int feat, set;
   char *str;

   memset(tag, ' ', 4);
   str=tagstr->u.sval;
   if (macok && *str=='<') {
      if (sscanf(str, "<%d,%d>", &feat, &set) != 2 || feat < 0 || feat > 65535
	  || set < 0 || set > 65535)
	 ScriptError(c, "Bad Apple feature/setting");
      *wasmac=true;
      return ((feat << 16) | set);
   } else if (*str) {
      tag[0]=*str;
      if (str[1]) {
	 tag[1]=str[1];
	 if (str[2]) {
	    tag[2]=str[2];
	    if (str[3]) {
	       tag[3]=str[3];
	       if (str[4])
		  ScriptError(c,
			      "Tags/Scripts/Languages are represented by strings which are at most 4 characters long");
	    }
	 }
      }
   }
   *wasmac=false;
   return ((tag[0] << 24) | (tag[1] << 16) | (tag[2] << 8) | tag[3]);
}

static FeatureScriptLangList *ParseFeatureList(Context *c,Array *a) {
   FeatureScriptLangList *flhead=NULL, *fltail, *fl;
   struct scriptlanglist *sltail, *sl;
   int f, s, l;
   int wasmac;
   Array *scripts, *langs;

   for (f=0; f < a->argc; ++f) {
      if (a->vals[f].type != v_arr && a->vals[f].type != v_arrfree)
	 ScriptError(c, "A feature list is composed of an array of arrays");
      else if (a->vals[f].u.aval->argc != 2)
	 ScriptError(c,
		     "A feature list is composed of an array of arrays each containing two elements");
      else if (a->vals[f].u.aval->vals[0].type != v_str
	       || (a->vals[f].u.aval->vals[1].type != v_arr
		   && a->vals[f].u.aval->vals[1].type != v_arrfree))
	 ScriptError(c, "Bad type for argument");
      fl=chunkalloc(sizeof(FeatureScriptLangList));
      fl->featuretag =
	 ParseTag(c, &a->vals[f].u.aval->vals[0], true, &wasmac);
      fl->ismac=wasmac;
      if (flhead==NULL)
	 flhead=fl;
      else
	 fltail->next=fl;
      fltail=fl;
      scripts=a->vals[f].u.aval->vals[1].u.aval;
      if (scripts->argc==0)
	 ScriptErrorString(c, "No scripts specified for feature",
			   a->vals[f].u.aval->vals[0].u.sval);
      sltail=NULL;
      for (s=0; s < scripts->argc; ++s) {
	 if (scripts->vals[s].type != v_arr
	     && scripts->vals[s].type != v_arrfree)
	    ScriptError(c, "A script list is composed of an array of arrays");
	 else if (scripts->vals[s].u.aval->argc != 2)
	    ScriptError(c,
			"A script list is composed of an array of arrays each containing two elements");
	 else if (scripts->vals[s].u.aval->vals[0].type != v_str
		  || (scripts->vals[s].u.aval->vals[1].type != v_arr
		      && scripts->vals[s].u.aval->vals[1].type != v_arrfree))
	    ScriptError(c, "Bad type for argument");
	 sl=chunkalloc(sizeof(struct scriptlanglist));
	 sl->script =
	    ParseTag(c, &scripts->vals[s].u.aval->vals[0], false, &wasmac);
	 if (sltail==NULL)
	    fl->scripts=sl;
	 else
	    sltail->next=sl;
	 sltail=sl;
	 langs=scripts->vals[s].u.aval->vals[1].u.aval;
	 if (langs->argc==0) {
	    sl->lang_cnt=1;
	    sl->langs[0]=DEFAULT_LANG;
	 } else {
	    sl->lang_cnt=langs->argc;
	    if (langs->argc > MAX_LANG)
	       sl->morelangs =
		  malloc((langs->argc - MAX_LANG) * sizeof(uint32_t));
	    for (l=0; l < langs->argc; ++l) {
	       uint32_t lang=ParseTag(c, &langs->vals[l], false, &wasmac);

	       if (l < MAX_LANG)
		  sl->langs[l]=lang;
	       else
		  sl->morelangs[l - MAX_LANG]=lang;
	    }
	 }
      }
   }
   return (flhead);
}

static int FSLMatch(FeatureScriptLangList *fl,uint32_t feat_tag,
		    uint32_t script, uint32_t lang) {
   struct scriptlanglist *sl;
   int l;

   while (fl != NULL) {
      if (fl->featuretag==feat_tag || feat_tag==CHR('*', ' ', ' ', ' ')) {
	 for (sl=fl->scripts; sl != NULL; sl=sl->next) {
	    if (sl->script==script || script==CHR('*', ' ', ' ', ' ')) {
	       for (l=0; l < sl->lang_cnt; ++l) {
		  uint32_t lng =
		     l <
		     MAX_LANG ? sl->langs[l] : sl->morelangs[l - MAX_LANG];
		  if (lng==lang || lang==CHR('*', ' ', ' ', ' '))
		     return (true);
	       }
	    }
	 }
      }
      fl=fl->next;
   }
   return (false);
}

static void SCReplaceWith(SplineChar *dest,SplineChar *src) {
   int opos=dest->orig_pos, uenc=dest->unicodeenc;
   Undoes *u[2], *r1;
   struct splinecharlist *scl=dest->dependents;
   RefChar *refs;
   int layer, last;
   int lc;
   Layer *layers;

   if (src==dest)
      return;

   SCPreserveLayer(dest, ly_fore, 2);
   u[0]=dest->layers[ly_fore].undoes;
   u[1]=dest->layers[ly_back].undoes;
   r1=dest->layers[ly_back].redoes;

   free(dest->name);
   last=ly_fore;
   if (dest->parent->multilayer)
      last=dest->layer_cnt - 1;
   for (layer=ly_fore; layer <= last; ++layer) {
      SplinePointListsFree(dest->layers[layer].splines);
      RefCharsFree(dest->layers[layer].refs);
      ImageListsFree(dest->layers[layer].images);
   }
   StemInfosFree(dest->hstem);
   StemInfosFree(dest->vstem);
   DStemInfosFree(dest->dstem);
   MinimumDistancesFree(dest->md);
   KernPairsFree(dest->kerns);
   KernPairsFree(dest->vkerns);
   AnchorPointsFree(dest->anchor);
   PSTFree(dest->possub);
   free(dest->ttf_instrs);

   layers=dest->layers;
   lc=dest->layer_cnt;
   *dest=*src;
   dest->layers=malloc(dest->layer_cnt * sizeof(Layer));
   memcpy(&dest->layers[ly_back], &layers[ly_back], sizeof(Layer));
   for (layer=ly_fore; layer < dest->layer_cnt; ++layer) {
      memcpy(&dest->layers[layer], &src->layers[layer], sizeof(Layer));
      dest->layers[layer].undoes=dest->layers[layer].redoes=NULL;
      src->layers[layer].splines=NULL;
      src->layers[layer].images=NULL;
      src->layers[layer].refs=NULL;
   }
   for (layer=ly_fore; layer < dest->layer_cnt && layer < lc; ++layer)
      dest->layers[layer].undoes=layers[layer].undoes;
   for (; layer < lc; ++layer)
      UndoesFree(layers[layer].undoes);
   free(layers);
   dest->orig_pos=opos;
   dest->unicodeenc=uenc;
   dest->dependents=scl;
   dest->namechanged=true;

   src->name=NULL;
   src->unicodeenc=-1;
   src->hstem=NULL;
   src->vstem=NULL;
   src->dstem=NULL;
   src->md=NULL;
   src->kerns=NULL;
   src->vkerns=NULL;
   src->anchor=NULL;
   src->possub=NULL;
   src->ttf_instrs=NULL;
   src->ttf_instrs_len=0;
   SplineCharFree(src);

   /* Fix up dependant info */
   for (layer=ly_fore; layer < dest->layer_cnt; ++layer)
      for (refs=dest->layers[layer].refs; refs != NULL; refs=refs->next) {
	 for (scl=refs->sc->dependents; scl != NULL; scl=scl->next)
	    if (scl->sc==src)
	       scl->sc=dest;
      }
   SCCharChangedUpdate(dest, ly_fore, true);
}

static void FVApplySubstitution(FontViewBase *fv,uint32_t script,uint32_t lang,
				uint32_t feat_tag) {
   SplineFont *sf=fv->sf, *sf_sl=sf;
   SplineChar *sc, *replacement, *sc2;
   PST *pst;
   EncMap *map=fv->map;
   int i, gid, gid2;
   SplineChar **replacements;
   uint8_t *removes;
   int flags=-1;

   if (sf_sl->cidmaster != NULL)
      sf_sl=sf_sl->cidmaster;
   else if (sf_sl->mm != NULL)
      sf_sl=sf_sl->mm->normal;

   /* I used to do replaces and removes as I went along, and then Werner */
   /*  gave me a font were "a" was replaced by "b" replaced by "a" */
   replacements=calloc(sf->glyphcnt, sizeof(SplineChar *));
   removes=calloc(sf->glyphcnt, sizeof(uint8_t));

   for (i=0; i < map->enccount; ++i)
      if (fv->selected[i] &&
	  (gid=map->map[i]) != -1 && (sc=sf->glyphs[gid]) != NULL) {
	 for (pst=sc->possub; pst != NULL; pst=pst->next) {
	    if (pst->type==pst_substitution &&
		FSLMatch(pst->subtable->lookup->features, feat_tag, script,
			 lang))
	       break;
	 }
	 if (pst != NULL) {
	    replacement=SFGetChar(sf, -1, pst->u.subs.variant);
	    if (replacement != NULL) {
	       replacements[gid]=SplineCharCopy(replacement, sf, NULL);
	       removes[replacement->orig_pos]=true;
	    }
	 }
      }

   for (gid=0; gid < sf->glyphcnt; ++gid)
      if ((sc=sf->glyphs[gid]) != NULL) {
	 if (replacements[gid] != NULL) {
	    SCReplaceWith(sc, replacements[gid]);
	 } else if (removes[gid]) {
	    /* This is deliberatly in the else. We don't want to remove a glyph */
	    /*  we are about to replace */
	    for (gid2=0; gid2 < sf->glyphcnt; ++gid2)
	       if ((sc2=replacements[gid2]) != NULL) {
		  if (sc2->layers && ly_fore < sc2->layer_cnt) {
		     RefChar *rf, *rnext;

		     for (rf=sc2->layers[ly_fore].refs; rf != NULL;
			  rf=rnext) {
			rnext=rf->next;
			if (rf->sc==sc)
			   SCRefToSplines(sc2, rf, ly_fore);
		     }
		  }
	       }
	    SFRemoveGlyph(sf, sc, &flags);
	 }
      }

   free(removes);
   free(replacements);
   GlyphHashFree(sf);
}

static void Bitmapper(Context *c,int isavail) {
   int32_t *sizes;
   int i;
   int rasterize=true;

/* bControlAfmLigatureOutputc->a.argc != 2 && (!isavail || c->a.argc != 3) */
   if (c->a.vals[1].type != v_arr)
      ScriptError(c, "Bad type of argument");
   for (i=0; i < c->a.vals[1].u.aval->argc; ++i)
      if (c->a.vals[1].u.aval->vals[i].type != v_int ||
	  c->a.vals[1].u.aval->vals[i].u.ival <= 2)
	 ScriptError(c, "Bad type of array component");
   if (c->a.argc==3) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad type of argument");
      rasterize=c->a.vals[2].u.ival;
   }
   sizes=malloc((c->a.vals[1].u.aval->argc + 1) * sizeof(int32_t));
   for (i=0; i < c->a.vals[1].u.aval->argc; ++i) {
      sizes[i]=c->a.vals[1].u.aval->vals[i].u.ival;
      if ((sizes[i] >> 16)==0)
	 sizes[i] |= 0x10000;
   }
   sizes[i]=0;

   if (!BitmapControl(c->curfv, sizes, isavail, rasterize))
      ScriptError(c, "Bitmap operation failed");	/* Storage leak here longjmp avoids free */
   free(sizes);
}

static void _SetFontNames(Context *c,SplineFont *sf) {
   int i;

   for (i=1; i < c->a.argc; ++i)
      if (c->a.vals[i].type != v_str)
	 ScriptError(c, "Bad argument type");
   if (*c->a.vals[1].u.sval != '\0') {
      free(sf->fontname);
      sf->fontname=forcePSName_copy(c, c->a.vals[1].u.sval);
   }
   if (c->a.argc > 2 && *c->a.vals[2].u.sval != '\0') {
      free(sf->familyname);
      sf->familyname=script2latin1_copy(c->a.vals[2].u.sval);
   }
   if (c->a.argc > 3 && *c->a.vals[3].u.sval != '\0') {
      free(sf->fullname);
      sf->fullname=script2latin1_copy(c->a.vals[3].u.sval);
   }
   if (c->a.argc > 4 && *c->a.vals[4].u.sval != '\0') {
      free(sf->weight);
      sf->weight=script2latin1_copy(c->a.vals[4].u.sval);
   }
   if (c->a.argc > 5 && *c->a.vals[5].u.sval != '\0') {
      free(sf->copyright);
      sf->copyright=script2latin1_copy(c->a.vals[5].u.sval);
   }
   if (c->a.argc > 6 && *c->a.vals[6].u.sval != '\0') {
      free(sf->version);
      sf->version=script2latin1_copy(c->a.vals[6].u.sval);
   }
   SFReplaceFontnameBDFProps(c->curfv->sf);
}

static char *Tag2Str(uint32_t tag,int ismac) {
   char buffer[20];

   if (ismac)
      sprintf(buffer, "<%d,%d>", tag >> 16, tag & 0xffff);
   else {
      buffer[0]=tag >> 24;
      buffer[1]=tag >> 16;
      buffer[2]=tag >> 8;
      buffer[3]=tag & 0xff;
      buffer[4]='\0';
   }
   return (fastrdup(buffer));
}

static void os2getint(int val,Context *c) {
   c->return_val.type=v_int;
   c->return_val.u.ival=val;
}

static ItalicInfo default_ii={
   -13,				/* Italic angle (in degrees) */
   .95,				/* xheight percent */
   /* horizontal squash, lsb, stemsize, countersize, rsb */
   {.91, .89, .90, .91},	/* For lower case */
   {.91, .93, .93, .91},	/* For upper case */
   {.91, .93, .93, .91},	/* For things which are neither upper nor lower case */
   srf_flat,			/* Secondary serifs (initial, medial on "m", descender on "p", "q" */
   true,			/* Transform bottom serifs */
   true,			/* Transform serifs at x-height */
   false,			/* Transform serifs on ascenders */
   true,			/* Transform serifs on diagonal stems at baseline and x-height */

   true,			/* Change the shape of an "a" to look like a "d" without ascender */
   false,			/* Change the shape of "f" so it descends below baseline (straight down no flag at end) */
   true,			/* Change the shape of "f" so the bottom looks like the top */
   true,			/* Remove serifs from the bottom of descenders */

   true,			/* Make the cyrillic "phi" glyph have a top like an "f" */
   true,			/* Make the cyrillic "i" glyph look like a latin "u" */
   true,			/* Make the cyrillic "pi" glyph look like a latin "n" */
   true,			/* Make the cyrillic "te" glyph look like a latin "m" */
   true,			/* Make the cyrillic "sha" glyph look like a latin "m" rotated 180 */
   true,			/* Make the cyrillic "dje" glyph look like a latin smallcaps T (not implemented) */
   true,			/* Make the cyrillic "dzhe" glyph look like a latin "u" (same glyph used for cyrillic "i") */

   ITALICINFO_REMAINDER
};

static void Reblend(Context *c,int tonew) {
   real blends[MmMax];
   MMSet *mm=c->curfv->sf->mm;
   int i;

   if (mm==NULL)
     ScriptError(c, "Not a multiple master font");
   else if (c->a.vals[1].type==v_arr)
     ScriptError(c, "Bad type of argument");
   else if (c->a.vals[1].u.aval->argc != mm->axis_count)
     ScriptError(c, "Incorrect number of blend values");

   for (i=0; i < mm->axis_count; ++i) {
      if (c->a.vals[1].u.aval->vals[i].type != v_int)
	ScriptError(c, "Bad type of array element");
      blends[i]=c->a.vals[1].u.aval->vals[i].u.ival / 65536.0;
      if (blends[i] < mm->axismaps[i].min || blends[i] > mm->axismaps[i].max)
	ErrorMsg(2,"Warning: %dth axis value (%g) is outside the allowed range [%g,%g]\n",
		 i, blends[i], mm->axismaps[i].min, mm->axismaps[i].max);
   }
   c->curfv=MMCreateBlendedFont(mm, c->curfv, blends, tonew);
}

static void setint16(int16_t *val,Context *c) {
   if (c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");
   *val=c->a.vals[2].u.ival;
}

static void setss16(int16_t *val,SplineFont *sf,Context *c) {
   if (c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");
   *val=c->a.vals[2].u.ival;
   sf->pfminfo.subsuper_set=true;
}

/**********************************************************************/

static void bATan2(Context *c) {
   double val1, val2;

   if (c->a.vals[1].type==v_real)
     val1=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
     val1=c->a.vals[1].u.ival;
   else
     ScriptError(c, "Bad type for argument");
   if (c->a.vals[2].type==v_real)
     val2=c->a.vals[2].u.fval;
   else if (c->a.vals[2].type==v_int)
     val2=c->a.vals[2].u.ival;
   else
     ScriptError(c, "Bad type for argument");
   c->return_val.type=v_real;
   c->return_val.u.fval=atan2(val1, val2);
}

static void bAddAnchorClass(Context *c) {
   AnchorClass *ac, *t;
   unichar_t *ustr;
   SplineFont *sf=c->curfv->sf;

   if (sf->cidmaster)
      sf=sf->cidmaster;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str ||
	    c->a.vals[3].type != v_str)
      ScriptError(c, "Bad type for argument");

   ac=chunkalloc(sizeof(AnchorClass));

   ac->name=fastrdup(c->a.vals[1].u.sval);
   for (t=sf->anchor; t != NULL; t=t->next)
      if (strcmp(ac->name, t->name)==0)
	 break;
   if (t != NULL)
      ScriptErrorString(c,
			"This font already contains an anchor class with this name: ",
			c->a.vals[1].u.sval);

   ac->subtable=SFFindLookupSubtable(sf, c->a.vals[3].u.sval);
   /* if ( ac->subtable==NULL )
      ScriptErrorString(c,"Unknown lookup subtable",c->a.vals[3].u.sval); */

   if (strmatch(c->a.vals[2].u.sval, "default")==0
       || strmatch(c->a.vals[2].u.sval, "mark")==0)
      ac->type=act_mark;
   else if (strmatch(c->a.vals[2].u.sval, "mk-mk")==0
	    || strmatch(c->a.vals[2].u.sval, "mkmk")==0)
      ac->type=act_mkmk;
   else if (strmatch(c->a.vals[2].u.sval, "cursive")==0
	    || strmatch(c->a.vals[2].u.sval, "curs")==0)
      ac->type=act_curs;
   else
      ScriptErrorString(c,
			"Unknown type of anchor class. Must be one of \"default\", \"mk-mk\", or \"cursive\". ",
			c->a.vals[2].u.sval);

   ustr=uc_copy(c->a.vals[3].u.sval);
   free(ustr);
   ac->next=sf->anchor;
   sf->anchor=ac;
   sf->changed=true;
}

static void bAddAnchorPoint(Context *c) {
   AnchorClass *t;
   SplineFont *sf=c->curfv->sf;
   int type;
   SplineChar *sc;
   AnchorPoint *ap;
   int ligindex=0;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str ||
       (c->a.vals[3].type != v_int && c->a.vals[3].type != v_real) ||
       (c->a.vals[4].type != v_int && c->a.vals[4].type != v_real))
     ScriptError(c, "Bad type for argument");

   for (t=sf->anchor; t != NULL; t=t->next)
     if (strcmp(c->a.vals[1].u.sval, t->name)==0)
       break;
   if (t==NULL)
     ScriptErrorString(c,
		       "This font does not contain an anchor class with this name: ",
		       c->a.vals[1].u.sval);

   sc=GetOneSelChar(c);
   if (strmatch(c->a.vals[2].u.sval, "mark")==0)
     type=at_mark;
   else if (strmatch(c->a.vals[2].u.sval, "basechar")==0
	    || strmatch(c->a.vals[2].u.sval, "base")==0)
     type=at_basechar;
   else if (strmatch(c->a.vals[2].u.sval, "baselig")==0
	    || strmatch(c->a.vals[2].u.sval, "ligature")==0)
     type=at_baselig;
   else if (strmatch(c->a.vals[2].u.sval, "basemark")==0)
     type=at_basemark;
   else if (strmatch(c->a.vals[2].u.sval, "cursentry")==0
	    || strmatch(c->a.vals[2].u.sval, "entry")==0)
     type=at_centry;
   else if (strmatch(c->a.vals[2].u.sval, "cursexit")==0
	    || strmatch(c->a.vals[2].u.sval, "exit")==0)
     type=at_cexit;
   else if (strmatch(c->a.vals[2].u.sval, "default")==0) {
      int val=IsAnchorClassUsed(sc, t);
      PST *pst;

      for (pst=sc->possub; pst != NULL && pst->type != pst_ligature;
	   pst=pst->next);
      type =
	t->type==act_mark ? at_basechar : t->type ==
	act_mkmk ? at_basemark : at_centry;
      if (val < -1 && t->type==act_mkmk)
	type=val==-2 ? at_basemark : at_mark;
      else if (val==-2 && t->type==act_curs)
	type=at_cexit;
      else if (val==-3 || t->type==act_curs)
	type=at_centry;
      else if ((sc->unicodeenc != -1 && sc->unicodeenc < 0x10000 &&
		iscombining(sc->unicodeenc)) || sc->width==0
	       || sc->glyph_class==2 /* base class+1 */ )
	type=at_mark;
      else if (pst != NULL && type==at_basechar)
	type=at_baselig;
   } else
     ScriptErrorString(c, "Unknown type for anchor point: ",
		       c->a.vals[2].u.sval);

   if (type==at_baselig) {
      if (c->a.argc != 6)
	ScriptError(c, "Wrong number of arguments");
      else if (c->a.vals[5].type != v_int)
	ScriptError(c, "Bad type for argument");
      ligindex=c->a.vals[5].u.ival;
   } else {
      if (c->a.argc != 5)
	ScriptError(c, "Wrong number of arguments");
   }

   if ((type==at_baselig && t->type != act_mklg) ||
       (type==at_basechar && t->type != act_mark) ||
       (type==at_basemark && t->type != act_mkmk) ||
       (type==at_mark
	&& !(t->type==act_mark || t->type==act_mkmk
	     || t->type==act_mklg)) || ((type==at_centry
					   || type==at_cexit)
					  && t->type != act_curs))
     ScriptError(c,
		 "Type of anchor class does not match type requested for anchor point");

   for (ap=sc->anchor; ap != NULL; ap=ap->next) {
      if (ap->anchor==t) {
	 if (type==at_centry || type==at_cexit) {
	    if (ap->type==type)
	      break;
	 } else if (type==at_baselig) {
	    if (ap->lig_index==ligindex)
	      break;
	 } else if (ap->type==type)
	      break;
      }
   }
   if (ap != NULL)
     ScriptError(c,
		 "This character already has an Anchor Point in the given anchor class");

   ap=chunkalloc(sizeof(AnchorPoint));
   ap->anchor=t;
   ap->me.x =
     (c->a.vals[3].type ==
	 v_int) ? c->a.vals[3].u.ival : rint(c->a.vals[3].u.fval);
   ap->me.y =
     (c->a.vals[4].type ==
	 v_int) ? c->a.vals[4].u.ival : rint(c->a.vals[4].u.fval);
   ap->type=type;
   ap->lig_index=ligindex;
   ap->next=sc->anchor;
   sc->anchor=ap;
   sc->parent->changed=true;
}

static void bAddDHint(Context *c) {
   int i, any, gid;
   BasePoint left, right, unit;
   real args[6];
   double len, width;
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;
   SplineChar *sc;
   DStemInfo *d;

   for (i=1; i < 7; i++) {
      if (c->a.vals[i].type==v_int)
	 args[i - 1]=c->a.vals[i].u.ival;
      else if (c->a.vals[1].type==v_real)
	 args[i - 1]=c->a.vals[i].u.fval;
      else
	 ScriptError(c, "Bad argument type");
   }
   if (args[4]==0 && args[5]==0)
      ScriptError(c, "Invalid unit vector for a diagonal hint");
   else if (args[4]==0)
      ScriptError(c, "Use AddVHint to add a vertical hint");
   else if (args[5]==0)
      ScriptError(c, "Use AddHHint to add a horizontal hint");
   len=sqrt(pow(args[4], 2) + pow(args[5], 2));
   args[4] /= len;
   args[5] /= len;
   if (args[4] < 0) {
      unit.x=-args[4];
      unit.y=-args[5];
   } else {
      unit.x=args[4];
      unit.y=args[5];
   }
   width=(args[2] - args[0]) * unit.y - (args[3] - args[1]) * unit.x;
   if (width < 0) {
      left.x=args[0];
      left.y=args[1];
      right.x=args[2];
      right.y=args[3];
   } else {
      left.x=args[2];
      left.y=args[3];
      right.x=args[0];
      right.y=args[1];
   }

   any=false;
   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && (sc=sf->glyphs[gid]) != NULL
	  && fv->selected[i]) {
	 d=chunkalloc(sizeof(DStemInfo));
	 d->where=NULL;
	 d->left=left;
	 d->right=right;
	 d->unit=unit;
	 SCGuessDHintInstances(sc, ly_fore, d);
	 if (d->where==NULL) {
	    DStemInfoFree(d);
	    ErrorMsg(2,"Warning: could not figure out where the hint (%d,%d %d,%d %d,%d) is valid\n",
		     args[0], args[1], args[2], args[3], args[4], args[5]);
	 } else
	    MergeDStemInfo(sc->parent, &sc->dstem, d);
	 sc->manualhints=true;
	 any=true;
      }
   if (!any)
      ErrorMsg(2,"Warning: No characters selected in AddDHint(%d,%d %d,%d %d,%d)\n",
	       args[0], args[1], args[2], args[3], args[4], args[5]);
}

static void _AddHint(Context *c,int ish) {
   int i, any, gid;
   int start, width;
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;
   SplineChar *sc;
   StemInfo *h;

   if (c->a.vals[1].type==v_int)
      start=c->a.vals[1].u.ival;
   else if (c->a.vals[1].type==v_real)
      start=c->a.vals[1].u.fval;
   else
      ScriptError(c, "Bad argument type");
   if (c->a.vals[2].type==v_int)
      width=c->a.vals[2].u.ival;
   else if (c->a.vals[2].type==v_real)
      start=c->a.vals[2].u.fval;
   else
      ScriptError(c, "Bad argument type");
   if (width <= 0 && width != -20 && width != -21)
      ScriptError(c, "Bad hint width");
   any=false;
   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && (sc=sf->glyphs[gid]) != NULL
	  && fv->selected[i]) {
	 h=chunkalloc(sizeof(StemInfo));
	 h->start=start;
	 h->width=width;
	 if (ish) {
	    SCGuessHHintInstancesAndAdd(sc, ly_fore, h, 0x80000000,
					0x80000000);
	    sc->hconflicts=StemListAnyConflicts(sc->hstem);
	 } else {
	    SCGuessVHintInstancesAndAdd(sc, ly_fore, h, 0x80000000,
					0x80000000);
	    sc->vconflicts=StemListAnyConflicts(sc->vstem);
	 }
	 sc->manualhints=true;
	 SCClearHintMasks(sc, ly_fore, true);
	 any=true;
      }
   if (!any)
      ErrorMsg(2,"Warning: No characters selected in AddHint(%d,%d,%d)\n",
	       ish, start, width);
}

static void bAddExtrema(Context *c) {
   if (c->a.argc==1)
      FVAddExtrema(c->curfv, false);
   else if (c->a.argc==2) {
      if (c->a.vals[1].type != v_int)
	 ScriptError(c, "Bad type for argument");
      FVAddExtrema(c->curfv, (c->a.vals[1].u.ival != 0));
   }
}

static void SCMakeLine(SplineChar *sc) {
   int ly, last;
   SplinePointList *spl;
   SplinePoint *sp;
   int changed=false;

   last=ly_fore;
   if (sc->parent->multilayer)
      last=sc->layer_cnt - 1;
   for (ly=ly_fore; ly <= last; ++ly) {
      for (spl=sc->layers[ly].splines; spl != NULL; spl=spl->next) {
	 for (sp=spl->first;;) {
	    if (!sp->nonextcp || !sp->noprevcp) {
	       if (!changed) {
		  SCPreserveState(sc, false);
		  changed=false;
	       }
	       sp->prevcp=sp->me;
	       sp->noprevcp=true;
	       if (sp->prev)
		  SplineRefigure(sp->prev);
	       sp->nextcp=sp->me;
	       sp->nonextcp=true;
	       if (sp->next)
		  SplineRefigure(sp->next);
	    }
	    sp->pointtype=pt_corner;
	    if (sp->next==NULL)
	       break;
	    sp=sp->next->to;
	    if (sp==spl->first)
	       break;
	 }
      }
   }
   if (changed)
      SCCharChangedUpdate(sc, ly_fore, true);
}

static void bAddHHint(Context *c) {
   _AddHint(c, true);
}

static void bAddInstrs(Context *c) {
   int replace;
   SplineChar *sc=NULL;
   int icnt;
   uint8_t *instrs;
   uint32_t tag=0;
   SplineFont *sf=c->curfv->sf;
   int i;
   EncMap *map=c->curfv->map;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_int
       || c->a.vals[3].type != v_str)
      ScriptError(c, "Bad argument type");
   replace=c->a.vals[2].u.ival;
   if (strcmp(c->a.vals[1].u.sval, "fpgm")==0)
      tag=CHR('f', 'p', 'g', 'm');
   else if (strcmp(c->a.vals[1].u.sval, "prep")==0)
      tag=CHR('p', 'r', 'e', 'p');
   else if (c->a.vals[1].u.sval[0] != '\0') {
      sc=SFGetChar(sf, -1, c->a.vals[1].u.sval);
      if (sc==NULL)
	 ScriptErrorString(c, "Character/Table not found",
			   c->a.vals[1].u.sval);
   }

   instrs=_IVParse(sf, c->a.vals[3].u.sval, &icnt, prterror, NULL);
   if (instrs==NULL)
      ScriptError(c, "Failed to parse instructions");
   if (tag != 0)
      TableAddInstrs(sf, tag, replace, instrs, icnt);
   else if (sc != NULL)
      GlyphAddInstrs(sc, replace, instrs, icnt);
   else {
      for (i=0; i < map->enccount; ++i)
	 if (c->curfv->selected[i]) {
	    int gid=map->map[i];

	    if (gid != -1 && sf->glyphs[gid] != NULL)
	       GlyphAddInstrs(sf->glyphs[gid], replace, instrs, icnt);
	 }
   }
}

static void bAddLookup(Context *c) {
   OTLookup *otl, *after=NULL;
   SplineFont *sf=c->curfv->sf;
   int type;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str ||
	    c->a.vals[3].type != v_int ||
	    (c->a.vals[4].type != v_arr && c->a.vals[4].type != v_arrfree) ||
	    (c->a.argc==6 && c->a.vals[5].type != v_str))
      ScriptError(c, "Bad type for argument");

   if (strmatch(c->a.vals[2].u.sval, "gsub_single")==0)
      type=gsub_single;
   else if (strmatch(c->a.vals[2].u.sval, "gsub_multiple")==0)
      type=gsub_multiple;
   else if (strmatch(c->a.vals[2].u.sval, "gsub_alternate")==0)
      type=gsub_alternate;
   else if (strmatch(c->a.vals[2].u.sval, "gsub_ligature")==0)
      type=gsub_ligature;
   else if (strmatch(c->a.vals[2].u.sval, "gsub_context")==0)
      type=gsub_context;
   else if (strmatch(c->a.vals[2].u.sval, "gsub_contextchain")==0)
      type=gsub_contextchain;
   else if (strmatch(c->a.vals[2].u.sval, "gsub_reversecchain")==0)
      type=gsub_reversecchain;
   else if (strmatch(c->a.vals[2].u.sval, "morx_indic")==0)
      type=morx_indic;
   else if (strmatch(c->a.vals[2].u.sval, "morx_context")==0)
      type=morx_context;
   else if (strmatch(c->a.vals[2].u.sval, "morx_insert")==0)
      type=morx_insert;
   else if (strmatch(c->a.vals[2].u.sval, "gpos_single")==0)
      type=gpos_single;
   else if (strmatch(c->a.vals[2].u.sval, "gpos_pair")==0)
      type=gpos_pair;
   else if (strmatch(c->a.vals[2].u.sval, "gpos_cursive")==0)
      type=gpos_cursive;
   else if (strmatch(c->a.vals[2].u.sval, "gpos_mark2base")==0
	    || strmatch(c->a.vals[2].u.sval, "gpos_marktobase")==0)
      type=gpos_mark2base;
   else if (strmatch(c->a.vals[2].u.sval, "gpos_mark2ligature")==0
	    || strmatch(c->a.vals[2].u.sval, "gpos_marktoligature")==0)
      type=gpos_mark2ligature;
   else if (strmatch(c->a.vals[2].u.sval, "gpos_mark2mark")==0
	    || strmatch(c->a.vals[2].u.sval, "gpos_marktomark")==0)
      type=gpos_mark2mark;
   else if (strmatch(c->a.vals[2].u.sval, "gpos_context")==0)
      type=gpos_context;
   else if (strmatch(c->a.vals[2].u.sval, "gpos_contextchain")==0)
      type=gpos_contextchain;
   else if (strmatch(c->a.vals[2].u.sval, "kern_statemachine")==0)
      type=kern_statemachine;
   else
      ScriptErrorString(c, "Unknown lookup type", c->a.vals[2].u.sval);

   otl=SFFindLookup(c->curfv->sf, c->a.vals[1].u.sval);
   if (otl != NULL)
      ScriptErrorString(c, "Lookup name in use", c->a.vals[1].u.sval);
   if (c->a.argc==6) {
      after=SFFindLookup(c->curfv->sf, c->a.vals[5].u.sval);
      if (after==NULL)
	 ScriptErrorString(c, "Unknown after lookup", c->a.vals[5].u.sval);
      else if ((after->lookup_type >= gpos_start) != (type >= gpos_start))
	 ScriptErrorString(c, "After lookup is in a different table",
			   c->a.vals[5].u.sval);
   }

   if (sf->cidmaster)
      sf=sf->cidmaster;

   otl=chunkalloc(sizeof(OTLookup));
   if (after != NULL) {
      otl->next=after->next;
      after->next=otl;
   } else if (type >= gpos_start) {
      otl->next=sf->gpos_lookups;
      sf->gpos_lookups=otl;
   } else {
      otl->next=sf->gsub_lookups;
      sf->gsub_lookups=otl;
   }
   otl->lookup_type=type;
   otl->lookup_flags=c->a.vals[3].u.ival;
   otl->lookup_name=fastrdup(c->a.vals[1].u.sval);
   otl->features=ParseFeatureList(c, c->a.vals[4].u.aval);
   if (otl->features != NULL
       && (otl->features->featuretag==CHR('l', 'i', 'g', 'a')
	   || otl->features->featuretag==CHR('r', 'l', 'i', 'g')))
      otl->store_in_afm=true;
}

static void bAddLookupSubtable(Context *c) {
   int isgpos;
   OTLookup *otl, *test;
   struct lookup_subtable *sub, *after=NULL;
   SplineFont *sf=c->curfv->sf;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str ||
	    (c->a.argc==4 && c->a.vals[3].type != v_str))
      ScriptError(c, "Bad type for argument");

   otl=SFFindLookup(c->curfv->sf, c->a.vals[1].u.sval);
   if (otl==NULL)
      ScriptErrorString(c, "Unknown lookup", c->a.vals[1].u.sval);
   if (c->a.argc==4) {
      after=SFFindLookupSubtable(c->curfv->sf, c->a.vals[3].u.sval);
      if (after==NULL)
	 ScriptErrorString(c, "Unknown subtable", c->a.vals[3].u.sval);
      else if (after->lookup != otl)
	 ScriptErrorString(c, "Subtable is not in lookup",
			   c->a.vals[3].u.sval);
   }

   if (sf->cidmaster)
      sf=sf->cidmaster;

   for (isgpos=0; isgpos < 2; ++isgpos) {
      for (test=isgpos ? sf->gpos_lookups : sf->gsub_lookups; test != NULL;
	   test=test->next) {
	 for (sub=test->subtables; sub != NULL; sub=sub->next)
	    if (strcmp(sub->subtable_name, c->a.vals[2].u.sval)==0)
	       ScriptErrorString(c,
				 "A lookup subtable with this name already exists",
				 c->a.vals[2].u.sval);
      }
   }
   sub=chunkalloc(sizeof(struct lookup_subtable));
   sub->lookup=otl;
   sub->subtable_name=fastrdup(c->a.vals[2].u.sval);
   if (after != NULL) {
      sub->next=after->next;
      after->next=sub;
   } else {
      sub->next=otl->subtables;
      otl->subtables=sub;
   }
   switch (otl->lookup_type) {
     case gpos_cursive:
     case gpos_mark2base:
     case gpos_mark2ligature:
     case gpos_mark2mark:
	sub->anchor_classes=true;
	break;
     case gsub_single:
     case gsub_multiple:
     case gsub_alternate:
     case gsub_ligature:
     case gpos_single:
     case gpos_pair:
	sub->per_glyph_pst_or_kern=true;
	break;
   }
}

static void bAddPosSub(Context *c) {
   SplineChar *sc;
   PST temp, *pst;
   struct lookup_subtable *sub;
   memset(&temp, 0, sizeof(temp));

/* bAddPosSub c->a.argc != 3 && c->a.argc != 6 && c->a.argc != 11 */
if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");
   else if (c->a.argc==3 && c->a.vals[2].type != v_str)
      ScriptError(c, "Bad type for argument");
   else if (c->a.argc==6 && (c->a.vals[2].type != v_int ||
			       c->a.vals[3].type != v_int
			       || c->a.vals[4].type != v_int
			       || c->a.vals[5].type != v_int))
      ScriptError(c, "Bad type for argument");
   else if (c->a.argc==11 && (c->a.vals[2].type != v_str ||
				c->a.vals[3].type != v_int
				|| c->a.vals[4].type != v_int
				|| c->a.vals[5].type != v_int
				|| c->a.vals[6].type != v_int
				|| c->a.vals[7].type != v_int
				|| c->a.vals[8].type != v_int
				|| c->a.vals[9].type != v_int
				|| c->a.vals[10].type != v_int))
      ScriptError(c, "Bad type for argument");

   sub=SFFindLookupSubtable(c->curfv->sf, c->a.vals[1].u.sval);
   if (sub==NULL)
      ScriptErrorString(c, "Unknown lookup subtable", c->a.vals[2].u.sval);

   if (sub->lookup->lookup_type==gpos_single) {
      temp.type=pst_position;
      if (c->a.argc != 6)
	 ScriptError(c, "Wrong number of arguments");
   } else if (sub->lookup->lookup_type==gpos_pair) {
      temp.type=pst_pair;
      if (c->a.argc != 11)
	 ScriptError(c, "Wrong number of arguments");
   } else {
      if (c->a.argc != 3)
	 ScriptError(c, "Wrong number of arguments");
      if (sub->lookup->lookup_type==gsub_single)
	 temp.type=pst_substitution;
      else if (sub->lookup->lookup_type==gsub_alternate)
	 temp.type=pst_alternate;
      else if (sub->lookup->lookup_type==gsub_multiple)
	 temp.type=pst_multiple;
      else if (sub->lookup->lookup_type==gsub_ligature)
	 temp.type=pst_ligature;
      else
	 ScriptErrorString(c, "Unexpected lookup type",
			   sub->lookup->lookup_name);
   }

   temp.subtable=sub;

   sc=GetOneSelChar(c);

   if (temp.type==pst_position) {
      temp.u.pos.xoff=c->a.vals[2].u.ival;
      temp.u.pos.yoff=c->a.vals[3].u.ival;
      temp.u.pos.h_adv_off=c->a.vals[4].u.ival;
      temp.u.pos.v_adv_off=c->a.vals[5].u.ival;
   } else if (temp.type==pst_pair) {
      temp.u.pair.paired=fastrdup(c->a.vals[2].u.sval);
      temp.u.pair.vr=chunkalloc(sizeof(struct vr[2]));
      temp.u.pair.vr[0].xoff=c->a.vals[3].u.ival;
      temp.u.pair.vr[0].yoff=c->a.vals[4].u.ival;
      temp.u.pair.vr[0].h_adv_off=c->a.vals[5].u.ival;
      temp.u.pair.vr[0].v_adv_off=c->a.vals[6].u.ival;
      temp.u.pair.vr[1].xoff=c->a.vals[7].u.ival;
      temp.u.pair.vr[1].yoff=c->a.vals[8].u.ival;
      temp.u.pair.vr[1].h_adv_off=c->a.vals[9].u.ival;
      temp.u.pair.vr[1].v_adv_off=c->a.vals[10].u.ival;
   } else {
      temp.u.subs.variant=fastrdup(c->a.vals[2].u.sval);
      if (temp.type==pst_ligature)
	 temp.u.lig.lig=sc;
   }
   pst=chunkalloc(sizeof(PST));
   *pst=temp;
   pst->next=sc->possub;
   sc->possub=pst;
}

static void bAddSizeFeature(Context *c) {
   int i, found_english;
   struct array *arr, *subarr;
   struct otfname *cur, *last;
   SplineFont *sf=c->curfv->sf;

   sf->fontstyle_id=sf->design_range_bottom=sf->design_range_top=0;
   OtfNameListFree(sf->fontstyle_name);
   sf->fontstyle_name=NULL;

/* bAddSizeFeature c->a.argc != 6 && c->a.argc != 2 */
   if ((c->a.vals[1].type != v_int && c->a.vals[1].type != v_real) ||
	    (c->a.argc==6
	     && ((c->a.vals[2].type != v_int && c->a.vals[2].type != v_real)
		 || (c->a.vals[3].type != v_int
		     && c->a.vals[3].type != v_real)
		 || c->a.vals[4].type != v_int || (c->a.vals[5].type != v_arr
						   && c->a.vals[5].type !=
						   v_arrfree))))
      ScriptError(c, "Bad type for argument");
   else if (c->a.vals[1].type==v_int)
      sf->design_size=c->a.vals[1].u.ival * 10;
   else if (c->a.vals[1].type==v_real)
      sf->design_size=rint(c->a.vals[1].u.fval * 10);

   if (c->a.argc==6) {
      sf->design_range_bottom=c->a.vals[2].type==v_int
	 ? c->a.vals[2].u.ival * 10 : rint(c->a.vals[2].u.fval * 10);
      sf->design_range_top=c->a.vals[3].type==v_int
	 ? c->a.vals[3].u.ival * 10 : rint(c->a.vals[3].u.fval * 10);
      if (sf->design_size < sf->design_range_bottom ||
	  sf->design_size > sf->design_range_top)
	 ScriptError(c, "Design size must be between design range bounds");
      sf->fontstyle_id=c->a.vals[4].u.ival;
      arr=c->a.vals[5].u.aval;
      found_english=false;
      last=NULL;
      for (i=0; i < arr->argc; ++i) {
	 if (arr->vals[i].type != v_arr && arr->vals[i].type != v_arrfree)
	    ScriptError(c, "Array must be an array of arrays");
	 subarr=arr->vals[i].u.aval;
	 if (subarr->argc != 2 || subarr->vals[0].type != v_int ||
	     subarr->vals[1].type != v_str)
	    ScriptError(c, "Array must consist of lanuage-id, string pairs");
	 if (subarr->vals[0].u.ival==0x409)
	    found_english=true;
	 cur=chunkalloc(sizeof(*cur));
	 cur->lang=subarr->vals[0].u.ival;
	 cur->name=fastrdup(subarr->vals[1].u.sval);
	 if (last==NULL)
	    sf->fontstyle_name=cur;
	 else
	    last->next=cur;
	 last=cur;
      }
      if (!found_english)
	 ScriptError(c, "Array must contain an English language entry");
   }
}

static void FigureSplExt(SplineSet *spl,int pos,int xextrema,
			 double minmax[2]) {
   Spline *s, *first;
   extended ts[3];
   int oth=!xextrema, i;
   double val;

   while (spl != NULL) {
      first=NULL;
      for (s=spl->first->next; s != NULL && s != first; s=s->to->next) {
	 if (first==NULL)
	    first=s;
	 if ((xextrema &&
	      pos > s->from->me.y && pos > s->from->nextcp.y &&
	      pos > s->to->me.y && pos > s->to->prevcp.y) ||
	     (xextrema &&
	      pos < s->from->me.y && pos < s->from->nextcp.y &&
	      pos < s->to->me.y && pos < s->to->prevcp.y) ||
	     (!xextrema &&
	      pos > s->from->me.x && pos > s->from->nextcp.x &&
	      pos > s->to->me.x && pos > s->to->prevcp.x) ||
	     (!xextrema &&
	      pos < s->from->me.x && pos < s->from->nextcp.x &&
	      pos < s->to->me.x && pos < s->to->prevcp.x))
	    continue;		/* can't intersect spline */
	 if (CubicSolve(&s->splines[xextrema], pos, ts)==-1)
	    continue;		/* didn't intersect */
	 for (i=0; i < 3 && ts[i] != -1; ++i) {
	    val=((s->splines[oth].a * ts[i] + s->splines[oth].b) * ts[i] +
		   s->splines[oth].c) * ts[i] + s->splines[oth].d;
	    if (val < minmax[0])
	       minmax[0]=val;
	    if (val > minmax[1])
	       minmax[1]=val;
	 }
      }
      spl=spl->next;
   }
}

static void FigureExtrema(Context *c,SplineChar *sc,int pos,int xextrema) {
   /* If xextrema is true, then pos will be a y value and we want to */
   /*  find the minimum and maximum x values for the contours of the glyph */
   /*  at that y value */
   /* If xextrama is false, then pos is an x value and we want min/max y */
   double minmax[2];
   int layer, l, last;
   RefChar *r;

   minmax[0]=1e20;
   minmax[1]=-1e20;
   last=ly_fore;
   if (sc->parent->multilayer)
      last=sc->layer_cnt - 1;
   for (layer=ly_fore; layer <= last; ++layer) {
      FigureSplExt(sc->layers[layer].splines, pos, xextrema, minmax);
      for (r=sc->layers[layer].refs; r != NULL; r=r->next)
	 for (l=0; l < r->layer_cnt; ++l)
	    FigureSplExt(r->layers[l].splines, pos, xextrema, minmax);
   }

   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=2;
   c->return_val.u.aval->vals=malloc(2 * sizeof(Val));
   c->return_val.u.aval->vals[0].type=v_int;
   c->return_val.u.aval->vals[1].type=v_int;
   if (minmax[0] > 1e10) {	/* Unset. Presumably pos is outside bounding box */
      c->return_val.u.aval->vals[0].u.ival=1;
      c->return_val.u.aval->vals[1].u.ival=0;
   } else {
      c->return_val.u.aval->vals[0].u.ival=minmax[0];
      c->return_val.u.aval->vals[1].u.ival=minmax[1];
   }
}

static void FigureProfile(Context *c,SplineChar *sc,int pos,int xextrema) {
#   define MAXSECT 100
   SplineSet *spl;
   Spline *s, *first;
   extended ts[3];
   int oth=!xextrema, i, j=0, l, m;
   double *val=NULL, temp;
   RefChar *r;

   spl=sc->layers[ly_fore].splines;
   while (spl != NULL) {
      first=NULL;
      for (s=spl->first->next; s != NULL && s != first; s=s->to->next) {
	 if (first==NULL)
	    first=s;
	 if ((xextrema &&
	      pos > s->from->me.y && pos > s->from->nextcp.y &&
	      pos > s->to->me.y && pos > s->to->prevcp.y) ||
	     (xextrema &&
	      pos < s->from->me.y && pos < s->from->nextcp.y &&
	      pos < s->to->me.y && pos < s->to->prevcp.y) ||
	     (!xextrema &&
	      pos > s->from->me.x && pos > s->from->nextcp.x &&
	      pos > s->to->me.x && pos > s->to->prevcp.x) ||
	     (!xextrema &&
	      pos < s->from->me.x && pos < s->from->nextcp.x &&
	      pos < s->to->me.x && pos < s->to->prevcp.x))
	    continue;		/* can't intersect spline */
	 if (CubicSolve(&s->splines[xextrema], pos, ts)==-1)
	    continue;		/* didn't intersect */
	 for (i=0; i < 3 && ts[i] != -1; ++i) {
	    temp=((s->splines[oth].a * ts[i] + s->splines[oth].b) * ts[i] +
		    s->splines[oth].c) * ts[i] + s->splines[oth].d;
	    for (m=0; m < j; m++)
	       if (temp==val[m])
		  goto skip1;
	    val=(double *) realloc(val, (j + 1) * sizeof(double));
	    val[j]=temp;
	    if (j >= MAXSECT)
	       goto maxsect_reached;
	    j++;
	  skip1:
	    ;
	 }
      }
      spl=spl->next;
   }
   for (r=sc->layers[ly_fore].refs; r != NULL; r=r->next) {
      for (l=0; l < r->layer_cnt; ++l) {
	 spl=r->layers[l].splines;
	 while (spl != NULL) {
	    first=NULL;
	    for (s=spl->first->next; s != NULL && s != first;
		 s=s->to->next) {
	       if (first==NULL)
		  first=s;
	       if ((xextrema &&
		    pos > s->from->me.y && pos > s->from->nextcp.y &&
		    pos > s->to->me.y && pos > s->to->prevcp.y) ||
		   (xextrema &&
		    pos < s->from->me.y && pos < s->from->nextcp.y &&
		    pos < s->to->me.y && pos < s->to->prevcp.y) ||
		   (!xextrema &&
		    pos > s->from->me.x && pos > s->from->nextcp.x &&
		    pos > s->to->me.x && pos > s->to->prevcp.x) ||
		   (!xextrema &&
		    pos < s->from->me.x && pos < s->from->nextcp.x &&
		    pos < s->to->me.x && pos < s->to->prevcp.x))
		  continue;	/* can't intersect spline */
	       if (CubicSolve(&s->splines[xextrema], pos, ts)==-1)
		  continue;	/* didn't intersect */
	       for (i=0; i < 3 && ts[i] != -1; ++i) {
		  temp =
		     ((s->splines[oth].a * ts[i] +
		       s->splines[oth].b) * ts[i] +
		      s->splines[oth].c) * ts[i] + s->splines[oth].d;
		  for (m=0; m < j; m++)
		     if (temp==val[m])
			goto skip2;
		  val=(double *) realloc(val, (j + 1) * sizeof(double));
		  val[j]=temp;
		  if (j >= MAXSECT)
		     goto maxsect_reached;
		  j++;
		skip2:
		  ;
	       }
	    }
	    spl=spl->next;
	 }
      }
   }
 maxsect_reached:
/* Sort the array */
   for (i=0; i < j; i++)
      for (l=i + 1; l < j; l++)
	 if (val[i] > val[l]) {
	    temp=val[l];
	    val[l]=val[i];
	    val[i]=temp;
	 }

   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=j;
   c->return_val.u.aval->vals=malloc(j * sizeof(Val));
   for (i=0; i < j; i++) {
      c->return_val.u.aval->vals[i].type=v_real;
      c->return_val.u.aval->vals[i].u.fval=val[i];
   }
   if (val)
      free(val);
}

static void bAddVHint(Context *c) {
   _AddHint(c, false);
}

static void bAppendAccent(Context *c) {
   int pos=____NOPOSDATAGIVEN;	/* unicode char pos info, see #define for (uint32_t)(utype2[]) */
   char *glyph_name=NULL;	/* unicode char name */
   int uni=-1;		/* unicode char value */
   int ret;
   SplineChar *sc;

   if (c->a.vals[1].type != v_str && c->a.vals[1].type != v_int
       && c->a.vals[1].type != v_unicode)
     ScriptError(c, "Bad argument type");
   else if (c->a.argc==3 && c->a.vals[2].type != v_int)
     ScriptError(c, "Bad argument type");

   if (c->a.vals[1].type==v_str)
     glyph_name=c->a.vals[1].u.sval;
   else
     uni=c->a.vals[1].u.ival;
   if (c->a.argc==3)
     pos=(uint32_t) (c->a.vals[2].u.ival);

   sc=GetOneSelChar(c);
   ret=SCAppendAccent(sc, ly_fore, glyph_name, uni, (uint32_t) (pos));
   if (ret==1)
     ScriptError(c, "No base character reference found");
   else if (ret==2)
     ScriptError(c, "Could not find that accent");
}

static void bApplySubstitution(Context *c) {
   uint32_t tags[3];
   int i;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str ||
       c->a.vals[3].type != v_str)
     ScriptError(c, "Bad argument type");
   for (i=0; i < 3; ++i) {
      char *str=c->a.vals[i + 1].u.sval;
      char temp[4];

      memset(temp, ' ', 4);
      if (*str) {
	 temp[0]=*str;
	 if (str[1]) {
	    temp[1]=str[1];
	    if (str[2]) {
	       temp[2]=str[2];
	       if (str[3]) {
		  temp[3]=str[3];
		  if (str[4])
		     ScriptError(c,
				 "Tags/Scripts/Languages are represented by strings which are at most 4 characters long");
	       }
	    }
	 }
      }
      tags[i]=(temp[0] << 24) | (temp[1] << 16) | (temp[2] << 8) | temp[3];
   }
   FVApplySubstitution(c->curfv, tags[0], tags[1], tags[2]);
}

static void bArray(Context *c) {
   int i;

   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Expected integer argument");
   else if (c->a.vals[1].u.ival <= 0)
      ScriptError(c, "Argument must be positive");
   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=c->a.vals[1].u.ival;
   c->return_val.u.aval->vals=malloc(c->a.vals[1].u.ival * sizeof(Val));
   for (i=0; i < c->a.vals[1].u.ival; ++i)
      c->return_val.u.aval->vals[i].type=v_void;
}

static void bAskUser(Context *c) {
   char *quest, *def="";
   char *line=NULL;
   char *t1;
   char *loc;
   size_t lbsize=0;

   if (c->a.vals[1].type != v_str
       || (c->a.argc==3 && c->a.vals[2].type != v_str))
     ScriptError(c, "Expected string argument");
   quest=c->a.vals[1].u.sval;
   if (c->a.argc==3)
     def=c->a.vals[2].u.sval;

   t1=script2utf8_copy(quest);
   loc=fastrdup(t1);

   printf("%s",loc);
   free(t1);
   free(loc);

   c->return_val.type=v_str;

#   ifdef _NO_LIBREADLINE
   if (agetline(&line,&lbsize,astdin)<0) {
      if (line!=NULL) free(line);
      line=NULL;
   }
#   else
   line=readline("");
   lbsize=strlen(line);
   line=(char *)realloc(line,lbsize+2);
   strcat(line,"\n");
#   endif

   if (line==NULL) {
      c->return_val.u.sval=fastrdup("");
   } else if (line[0]=='\0' || line[0]=='\n' || line[0]=='\r') {
      free(line);
      c->return_val.u.sval=fastrdup(def);
   } else {
      t1=fastrdup(line);
      c->return_val.u.sval=utf82script_copy(t1);
      free(t1);
      free(line);
   }
}

static void bAutoCounter(Context *c) {
   FVAutoCounter(c->curfv);
}

static void bAutoHint(Context *c) {
   FVAutoHint(c->curfv);
}

static void bAutoInstr(Context *c) {
   FVAutoInstr(c->curfv);
}

static void bAutoKern(Context *c) {
   struct lookup_subtable *sub;

   if (c->a.vals[1].type != v_int || c->a.vals[2].type != v_int ||
       c->a.vals[3].type != v_str ||
       (c->a.argc==5 && c->a.vals[4].type != v_str))
      ScriptError(c, "Bad argument type");

   sub=SFFindLookupSubtable(c->curfv->sf, c->a.vals[3].u.sval);
   if (sub==NULL)
      ScriptErrorString(c, "Unknown lookup subtable", c->a.vals[3].u.sval);

   if (!AutoKernScript(c->curfv, c->a.vals[1].u.ival, c->a.vals[2].u.ival,
		       sub, c->a.argc==5 ? c->a.vals[4].u.sval : NULL))
      ScriptError(c, "No characters selected.");
}

static void bAutoWidth(Context *c) {
   int sep, min=10, max=-1;

   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type in AutoWidth");
   sep=c->a.vals[1].u.ival;
   max=2 * sep;
   if (c->a.argc > 2) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad argument type in AutoWidth");
      min=c->a.vals[2].u.ival;
      if (c->a.argc > 3) {
	 if (c->a.vals[3].type != v_int)
	    ScriptError(c, "Bad argument type in AutoWidth");
	 max=c->a.vals[3].u.ival;
      }
   }
   AutoWidth2(c->curfv, sep, min, max, 0, 1);
}

static void bAutoTrace(Context *c) {
   FVAutoTrace(c->curfv, false);
}

static void bBitmapsAvail(Context *c) {
   int shows_bitmap=false;
   BDFFont *bdf;

   if (c->curfv->active_bitmap != NULL) {
      for (bdf=c->curfv->sf->bitmaps; bdf != NULL; bdf=bdf->next)
	 if (bdf==c->curfv->active_bitmap)
	    break;
      shows_bitmap=bdf != NULL;
   }
   Bitmapper(c, true);
   if (shows_bitmap && c->curfv->active_bitmap != NULL) {
      BDFFont *bdf;

      for (bdf=c->curfv->sf->bitmaps; bdf != NULL; bdf=bdf->next)
	 if (bdf==c->curfv->active_bitmap)
	    break;
      if (bdf==NULL)
	 c->curfv->active_bitmap=c->curfv->sf->bitmaps;
   }
}

static void bBitmapsRegen(Context *c) {
   Bitmapper(c, false);
}

static void bBuildAccented(Context *c) {
   FVBuildAccent(c->curfv, true);
}

static void bBuildComposite(Context *c) {
   FVBuildAccent(c->curfv, false);
}

static void bBuildDuplicate(Context *c) {
   FVBuildDuplicate(c->curfv);
}

static void bCIDChangeSubFont(Context *c) {
   SplineFont *sf=c->curfv->sf, *newsf;
   EncMap *map=c->curfv->map;
   int i;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   if (sf->cidmaster==NULL)
      ScriptErrorString(c, "Not a cid-keyed font", sf->fontname);
   for (i=0; i < sf->cidmaster->subfontcnt; ++i)
      if (strcmp(sf->cidmaster->subfonts[i]->fontname, c->a.vals[1].u.sval) ==
	  0)
	 break;
   if (i==sf->cidmaster->subfontcnt)
      ScriptErrorString(c, "Not in the current cid font",
			c->a.vals[1].u.sval);
   newsf=sf->cidmaster->subfonts[i];

   if (newsf->glyphcnt > sf->glyphcnt) {
      free(c->curfv->selected);
      c->curfv->selected=calloc(newsf->glyphcnt, sizeof(char));
      if (newsf->glyphcnt > map->encmax)
	 map->map =
	    realloc(map->map, (map->encmax=newsf->glyphcnt) * sizeof(int32_t));
      if (newsf->glyphcnt > map->backmax)
	 map->backmap =
	    realloc(map->backmap,
		    (map->backmax=newsf->glyphcnt) * sizeof(int32_t));
      for (i=0; i < newsf->glyphcnt; ++i)
	 map->map[i]=map->backmap[i]=i;
      map->enccount=newsf->glyphcnt;
   }
   c->curfv->sf=newsf;
}

static void bCIDFlatten(Context *c) {
   SplineFont *sf=c->curfv->sf;

   if (sf->cidmaster==NULL)
      ScriptErrorString(c, "Not a cid-keyed font", sf->fontname);

   SFFlatten(sf->cidmaster);
}

static void bCIDFlattenByCMap(Context *c) {
   SplineFont *sf=c->curfv->sf;
   char *t;
   char *locfilename;

   if (sf->cidmaster==NULL)
      ScriptErrorString(c, "Not a cid-keyed font", sf->fontname);
   else if (c->a.vals[1].type != v_str)
      ScriptError(c, "Argument must be a filename");

   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   if (!SFFlattenByCMap(sf, locfilename))
      ScriptErrorString(c, "Can't find (or can't parse) cmap file",
			c->a.vals[1].u.sval);
   free(t);
   free(locfilename);
}

static void bCIDSetFontNames(Context *c) {
   SplineFont *sf=c->curfv->sf;

   if (sf->cidmaster==NULL)
      ScriptErrorString(c, "Not a cid-keyed font", sf->fontname);
   _SetFontNames(c, sf->cidmaster);
}

static void bCanonicalContours(Context *c) {
   FontViewBase *fv=c->curfv;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   int i, gid;

   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i])
	 CanonicalContours(sf->glyphs[gid], ly_fore);
}

static void bCanonicalStart(Context *c) {
   FontViewBase *fv=c->curfv;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   int i, gid;

   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i])
	 SPLsStartToLeftmost(sf->glyphs[gid], ly_fore);
}

static void bCeil(Context *c) {
if (c->a.vals[1].type != v_real)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_int;
   c->return_val.u.ival=ceil(c->a.vals[1].u.fval);
}

static void bCenterInWidth(Context *c) {
   FVMetricsCenter(c->curfv, true);
}

static int ParseCharIdent(Context *c,Val *val,int signal_error) {
   SplineFont *sf=c->curfv->sf;
   EncMap *map=c->curfv->map;
   int bottom=-1;

   if (val->type==v_int)
      bottom=val->u.ival;
   else if (val->type==v_unicode || val->type==v_str) {
      if (val->type==v_unicode)
	 bottom=SFFindSlot(sf, map, val->u.ival, NULL);
      else {
	 bottom=SFFindSlot(sf, map, -1, val->u.sval);
      }
   } else {
      if (signal_error)
	 ScriptError(c, "Bad type for argument");
      else
	 return (-1);
   }
   if (bottom < 0 || bottom >= map->enccount) {
      if (signal_error) {
	 char buffer[40], *name=buffer;

	 if (val->type==v_int)
	    sprintf(buffer, "%d", val->u.ival);
	 else if (val->type==v_unicode)
	    sprintf(buffer, "U+%04X", val->u.ival);
	 else
	    name=val->u.sval;
	 if (bottom==-1)
	    ScriptErrorString(c, "Character not found", name);
	 else
	    ScriptErrorString(c, "Character is not in font", name);
      }
      return (-1);
   }
   return (bottom);
}

static void _SetKern(Context *c,int isv) {
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;
   SplineChar *sc1, *sc2;
   int i, gid, kern, ch2;
   struct lookup_subtable *sub=NULL;
   KernPair *kp;

   ch2=ParseCharIdent(c, &c->a.vals[1], true);
   if (c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");
   if (c->a.argc==4) {
      if (c->a.vals[3].type != v_str)
	 ScriptError(c, "Bad argument type");
      else {
	 sub=SFFindLookupSubtable(sf, c->a.vals[3].u.sval);
	 if (sub==NULL)
	    ScriptErrorString(c, "Unknown lookup subtable",
			      c->a.vals[3].u.sval);
      }
   }
   kern=c->a.vals[2].u.ival;
   if (kern != 0)
      sc2=SFMakeChar(sf, map, ch2);
   else {
      gid=map->map[ch2];
      sc2=gid==-1 ? NULL : sf->glyphs[gid];
      if (sc2==NULL)
	 return;		/* It already has a kern==0 with everything */
   }

   for (i=0; i < map->enccount; ++i)
      if (c->curfv->selected[i]) {
	 struct lookup_subtable *local_sub=sub;

	 if (kern != 0)
	    sc1=SFMakeChar(sf, map, i);
	 else {
	    gid=map->map[i];
	    if (gid==-1 || (sc1=sf->glyphs[gid])==NULL)
	       continue;
	 }
	 for (kp=isv ? sc1->vkerns : sc1->kerns;
	      kp != NULL && kp->sc != sc2; kp=kp->next);
	 if (local_sub==NULL && kp != NULL)
	    local_sub=kp->subtable;
	 if (local_sub==NULL)
	    local_sub =
	       SFSubTableFindOrMake(sf,
				    isv ? CHR('v', 'k', 'r', 'n') : CHR('k',
									'e',
									'r',
									'n'),
				    SCScriptFromUnicode(sc1), gpos_pair);
	 if (kp==NULL && kern==0)
	    continue;
	 if (!isv)
	    MMKern(sc1->parent, sc1, sc2, kp==NULL ? kern : kern - kp->off,
		   local_sub, kp);
	 if (kp != NULL) {
	    kp->off=kern;
	    kp->subtable=local_sub;
	 } else {
	    kp=chunkalloc(sizeof(KernPair));
	    if (isv) {
	       kp->next=sc1->vkerns;
	       sc1->vkerns=kp;
	    } else {
	       kp->next=sc1->kerns;
	       sc1->kerns=kp;
	    }
	    kp->sc=sc2;
	    kp->off=kern;
	    kp->subtable=local_sub;
	 }
      }
}

static void bChangePrivateEntry(Context *c) {
   SplineFont *sf=c->curfv->sf;
   char *key, *val;

if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str)
      ScriptError(c, "Bad argument type");

   key=forceASCIIcopy(c, c->a.vals[1].u.sval);
   val=forceASCIIcopy(c, c->a.vals[2].u.sval);
   if (sf->private==NULL) {
      sf->private=calloc(1, sizeof(struct psdict));
      sf->private->cnt=10;
      sf->private->keys=calloc(10, sizeof(char *));
      sf->private->values=calloc(10, sizeof(char *));
   }
   PSDictChangeEntry(sf->private, key, val);
   free(key);
   free(val);
}

static void bChangeWeight(Context *c) {
   enum embolden_type type=embolden_auto;
   struct lcg_zones zones;

   memset(&zones, 0, sizeof(zones));
   zones.counter_type=ct_auto;
   zones.serif_fuzz=0.9;
   zones.removeoverlap=1;

   if (c->a.vals[1].type==v_real)
      zones.stroke_width=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
      zones.stroke_width=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad argument type in ChangeWeight");

   FVEmbolden(c->curfv, type, &zones);
}

static int RefMatchesNamesUni(RefChar *ref,char **refnames,int *refunis,
			      int refcnt) {
   int i;

   for (i=0; i < refcnt; ++i) {
      if (refunis[i] != -1 && refunis[i]==ref->unicode_enc)
	 return (true);
      else if (refnames[i] != NULL && strcmp(refnames[i], ref->sc->name)==0)
	 return (true);
   }
   return (false);
}

static RefChar *FindFirstRef(SplineChar *sc,char **refnames,int *refunis,
			     int refcnt) {
   RefChar *ref;

   for (ref=sc->layers[ly_fore].refs; ref != NULL; ref=ref->next)
      if (RefMatchesNamesUni(ref, refnames, refunis, refcnt))
	 return (ref);

   return (NULL);
}

static void _bMoveReference(Context *c,int position) {
   real translate[2], t[6];
   char **refnames;
   int *refunis;
   FontViewBase *fv;
   int i, j, gid, refcnt;
   EncMap *map;
   SplineFont *sf;
   SplineChar *sc;
   RefChar *ref;

   if (c->a.vals[1].type==v_int)
      translate[0]=c->a.vals[1].u.ival;
   else if (c->a.vals[1].type==v_real)
      translate[0]=c->a.vals[1].u.fval;
   else
      ScriptError(c, "Bad argument type");
   if (c->a.vals[2].type==v_int)
      translate[1]=c->a.vals[2].u.ival;
   else if (c->a.vals[2].type==v_real)
      translate[1]=c->a.vals[2].u.fval;
   else
      ScriptError(c, "Bad argument type");

   refcnt=c->a.argc - 3;
   refnames=calloc(refcnt, sizeof(char *));
   refunis=malloc(refcnt * sizeof(int));
   memset(refunis, -1, refcnt * sizeof(int));
   for (i=0; i < refcnt; ++i) {
      if (c->a.vals[3 + i].type==v_str)
	 refnames[i]=c->a.vals[3 + i].u.sval;
      else if (c->a.vals[3 + i].type==v_int
	       || c->a.vals[3 + i].type==v_unicode)
	 refunis[i]=c->a.vals[3 + i].u.ival;
      else
	 ScriptError(c, "Bad argument type");
   }

   fv=c->curfv;
   map=fv->map;
   sf=fv->sf;
   t[0]=t[3]=1;
   t[1]=t[2]=0;
   t[4]=translate[0];
   t[5]=translate[1];
   for (i=0; i < map->enccount; ++i)
      if (fv->selected[i]) {
	 if ((gid=map->map[i])==-1 || (sc=sf->glyphs[gid])==NULL ||
	     (ref=FindFirstRef(sc, refnames, refunis, refcnt))==NULL) {
	    char buffer[12];

	    sprintf(buffer, "%d", i);
	    if (gid != -1 && sc != NULL)
	       ScriptErrorString(c, "Failed to find a matching reference in",
				 sc->name);
	    else
	       ScriptErrorString(c,
				 "Failed to find a matching reference at encoding",
				 buffer);
	 } else {
	    SCPreserveLayer(sc, ly_fore, false);
	    for (ref=sc->layers[ly_fore].refs; ref != NULL; ref=ref->next) {
	       if (RefMatchesNamesUni(ref, refnames, refunis, refcnt)) {
		  if (position) {
		     t[4]=translate[0] - ref->transform[4];
		     t[5]=translate[1] - ref->transform[5];
		  }
		  ref->transform[4] += t[4];
		  ref->transform[5] += t[5];
		  for (j=0; j < ref->layer_cnt; ++j)
		     SplinePointListTransform(ref->layers[j].splines, t,
					      tpt_AllPoints);
		  ref->bb.minx += t[4];
		  ref->bb.miny += t[5];
		  ref->bb.maxx += t[4];
		  ref->bb.maxy += t[5];
	       }
	    }
	    SCCharChangedUpdate(sc, ly_fore, true);
	 }
      }
}

static void bCharCnt(Context *c) {
   c->return_val.type=v_int;
   c->return_val.u.ival=c->curfv->map->enccount;
}

static void bCharInfo(Context *c) {
   SplineFont *sf=c->curfv->sf;
   EncMap *map=c->curfv->map;
   SplineChar *sc;
   DBounds b;
   KernClass *kc;
   int i, j, found, gid, gid2, layer;
   SplineChar dummy;
   RefChar *ref;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");

   found=GetOneSelCharIndex(c);
   gid=map->map[found];
   sc=gid==-1 ? NULL : sf->glyphs[gid];
   if (sc==NULL)
      sc=SCBuildDummy(&dummy, sf, map, found);

   c->return_val.type=v_int;
   if (c->a.argc==3) {
      int ch2;

      if (strmatch(c->a.vals[1].u.sval, "XExtrema")==0 ||
	  strmatch(c->a.vals[1].u.sval, "YExtrema")==0) {
	 if (c->a.vals[2].type != v_int)
	    ScriptError(c, "Bad type for argument");
	 FigureExtrema(c, sc, c->a.vals[2].u.ival, *c->a.vals[1].u.sval=='x'
		       || *c->a.vals[1].u.sval=='X');
	 return;
      } else if (strmatch(c->a.vals[1].u.sval, "XProfile")==0 ||
		 strmatch(c->a.vals[1].u.sval, "YProfile")==0) {
	 if (c->a.vals[2].type != v_int)
	    ScriptError(c, "Bad type for argument");
	 FigureProfile(c, sc, c->a.vals[2].u.ival, *c->a.vals[1].u.sval=='x'
		       || *c->a.vals[1].u.sval=='X');
	 return;
      }
      ch2=ParseCharIdent(c, &c->a.vals[2], true);
      gid2=ch2==-1 ? -1 : map->map[ch2];
      if (gid2==-1)
	 /* Do Nothing */ ;
      else if (strmatch(c->a.vals[1].u.sval, "Kern")==0) {
	 c->return_val.u.ival=0;
	 if (sf->glyphs[gid2] != NULL) {
	    KernPair *kp;
	    KernClass *kc;

	    for (kp=sc->kerns; kp != NULL && kp->sc != sf->glyphs[gid2];
		 kp=kp->next);
	    if (kp != NULL)
	       c->return_val.u.ival=kp->off;
	    else {
	       for (kc=sf->kerns; kc != NULL; kc=kc->next) {
		  if ((c->return_val.u.ival =
		       KernClassContains(kc, sc->name, sf->glyphs[gid2]->name,
					 true)) != 0)
		     break;
	       }
	    }
	 }
      } else if (strmatch(c->a.vals[1].u.sval, "VKern")==0) {
	 c->return_val.u.ival=0;
	 if (sf->glyphs[gid2] != NULL) {
	    KernPair *kp;

	    for (kp=sc->vkerns; kp != NULL && kp->sc != sf->glyphs[gid2];
		 kp=kp->next);
	    if (kp != NULL)
	       c->return_val.u.ival=kp->off;
	    else {
	       for (kc=sf->vkerns; kc != NULL; kc=kc->next) {
		  if ((c->return_val.u.ival =
		       KernClassContains(kc, sc->name, sf->glyphs[gid2]->name,
					 true)) != 0)
		     break;
	       }
	    }
	 }
      } else
	 ScriptErrorString(c, "Unknown tag", c->a.vals[1].u.sval);
   } else {
      if (strmatch(c->a.vals[1].u.sval, "Name")==0) {
	 c->return_val.type=v_str;
	 c->return_val.u.sval=fastrdup(sc->name);
      } else if (strmatch(c->a.vals[1].u.sval, "Unicode")==0)
	 c->return_val.u.ival=sc->unicodeenc;
      else if (strmatch(c->a.vals[1].u.sval, "Encoding")==0)
	 c->return_val.u.ival=c->curfv->map->backmap[sc->orig_pos];
      else if (strmatch(c->a.vals[1].u.sval, "Width")==0)
	 c->return_val.u.ival=sc->width;
      else if (strmatch(c->a.vals[1].u.sval, "VWidth")==0)
	 c->return_val.u.ival=sc->vwidth;
      else if (strmatch(c->a.vals[1].u.sval, "TeXHeight")==0)
	 c->return_val.u.ival=sc->tex_height;
      else if (strmatch(c->a.vals[1].u.sval, "TeXDepth")==0)
	 c->return_val.u.ival=sc->tex_depth;
      else if (strmatch(c->a.vals[1].u.sval, "Changed")==0)
	 c->return_val.u.ival=sc->changed;
      else if (strmatch(c->a.vals[1].u.sval, "DontAutoHint")==0)
	 c->return_val.u.ival=sc->manualhints;
      else if (strmatch(c->a.vals[1].u.sval, "Color")==0)
	 c->return_val.u.ival=sc->color;
      else if (strmatch(c->a.vals[1].u.sval, "GlyphIndex")==0)
	 c->return_val.u.ival=sc->orig_pos;
      else if (strmatch(c->a.vals[1].u.sval, "LayerCount")==0)
	 c->return_val.u.ival=sc->layer_cnt;
      else if (strmatch(c->a.vals[1].u.sval, "PointCount")==0)
	 c->return_val.u.ival=SCNumberPoints(sc, ly_fore);
      else if (strmatch(c->a.vals[1].u.sval, "ValidationState")==0)
	 c->return_val.u.ival=sc->layers[ly_fore].validation_state;
      else if (strmatch(c->a.vals[1].u.sval, "RefCount")==0) {
	 for (i=0, layer=0; layer < sc->layer_cnt; ++layer)
	    for (ref=sc->layers[layer].refs; ref != NULL;
		 ref=ref->next, ++i);
	 c->return_val.u.ival=i;
      } else if (strmatch(c->a.vals[1].u.sval, "Class")==0) {
	 c->return_val.type=v_str;
	 switch (sc->glyph_class) {
	   case 0:
	      c->return_val.u.sval=fastrdup("automatic");
	      break;
	   case 1:
	      c->return_val.u.sval=fastrdup("none");
	      break;
	   case 2:
	      c->return_val.u.sval=fastrdup("base");
	      break;
	   case 3:
	      c->return_val.u.sval=fastrdup("ligature");
	      break;
	   case 4:
	      c->return_val.u.sval=fastrdup("mark");
	      break;
	   case 5:
	      c->return_val.u.sval=fastrdup("component");
	      break;
	   default:
	      c->return_val.u.sval=fastrdup("unknown");
	      break;
	 }
      } else if (strmatch(c->a.vals[1].u.sval, "RefName")==0 ||
		 strmatch(c->a.vals[1].u.sval, "RefNames")==0) {
	 for (i=0, layer=0; layer < sc->layer_cnt; ++layer)
	    for (ref=sc->layers[layer].refs; ref != NULL;
		 ref=ref->next, ++i);
	 c->return_val.type=v_arrfree;
	 c->return_val.u.aval=malloc(sizeof(Array));
	 c->return_val.u.aval->argc=i;
	 c->return_val.u.aval->vals=malloc(i * sizeof(Val));
	 for (i=0, layer=0; layer < sc->layer_cnt; ++layer) {
	    for (ref=sc->layers[layer].refs; ref != NULL;
		 ref=ref->next, ++i) {
	       c->return_val.u.aval->vals[i].u.sval=fastrdup(ref->sc->name);
	       c->return_val.u.aval->vals[i].type=v_str;
	    }
	 }
      } else if (strmatch(c->a.vals[1].u.sval, "RefTransform")==0) {
	 for (i=0, layer=0; layer < sc->layer_cnt; ++layer)
	    for (ref=sc->layers[layer].refs; ref != NULL;
		 ref=ref->next, ++i);
	 c->return_val.type=v_arrfree;
	 c->return_val.u.aval=malloc(sizeof(Array));
	 c->return_val.u.aval->argc=i;
	 c->return_val.u.aval->vals=malloc(i * sizeof(Val));
	 for (i=0, layer=0; layer < sc->layer_cnt; ++layer) {
	    for (ref=sc->layers[layer].refs; ref != NULL;
		 ref=ref->next, ++i) {
	       c->return_val.u.aval->vals[i].type=v_arr;
	       c->return_val.u.aval->vals[i].u.aval=malloc(sizeof(Array));
	       c->return_val.u.aval->vals[i].u.aval->argc=6;
	       c->return_val.u.aval->vals[i].u.aval->vals =
		  malloc(6 * sizeof(Val));
	       for (j=0; j < 6; ++j) {
		  c->return_val.u.aval->vals[i].u.aval->vals[j].type=v_real;
		  c->return_val.u.aval->vals[i].u.aval->vals[j].u.fval =
		     ref->transform[j];
	       }
	    }
	 }
      } else if (strmatch(c->a.vals[1].u.sval, "Comment")==0) {
	 c->return_val.type=v_str;
	 c->return_val.u.sval=sc->comment ? fastrdup(sc->comment) : fastrdup("");
      } else {
	 SplineCharFindBounds(sc, &b);
	 if (strmatch(c->a.vals[1].u.sval, "LBearing")==0)
	    c->return_val.u.ival=b.minx;
	 else if (strmatch(c->a.vals[1].u.sval, "RBearing")==0)
	    c->return_val.u.ival=sc->width - b.maxx;
	 else if (strmatch(c->a.vals[1].u.sval, "BBox")==0 ||
		  strmatch(c->a.vals[1].u.sval, "BoundingBox")==0 ||
		  strmatch(c->a.vals[1].u.sval, "BB")==0) {
	    c->return_val.type=v_arrfree;
	    c->return_val.u.aval=malloc(sizeof(Array));
	    c->return_val.u.aval->argc=4;
	    c->return_val.u.aval->vals=malloc(4 * sizeof(Val));
	    for (i=0; i < 4; ++i)
	       c->return_val.u.aval->vals[i].type=v_real;
	    c->return_val.u.aval->vals[0].u.fval=b.minx;
	    c->return_val.u.aval->vals[1].u.fval=b.miny;
	    c->return_val.u.aval->vals[2].u.fval=b.maxx;
	    c->return_val.u.aval->vals[3].u.fval=b.maxy;
	 } else
	    ScriptErrorString(c, "Unknown tag", c->a.vals[1].u.sval);
      }
   }
}

/* Anchor type: see 'enum anchor_type' in splinefont.h */
static struct flaglist ap_types[]={
   {"mark", at_mark},
   {"base", at_basechar},
   {"ligature", at_baselig},
   {"basemark", at_basemark},
   {"entry", at_centry},
   {"exit", at_cexit},
   {"baselig", at_baselig},
   FLAGLIST_EMPTY
};

static void bCheckForAnchorClass(Context *c) {
   AnchorClass *t;
   SplineFont *sf=c->curfv->sf;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");

   for (t=sf->anchor; t != NULL; t=t->next)
      if (strcmp(c->a.vals[1].u.sval, t->name)==0)
	 break;
   c->return_val.type=v_int;
   c->return_val.u.ival=(t != NULL);
   return;
}

static void bChr(Context *c) {
   char buf[2];
   char *temp;
   int i;

   if (c->a.vals[1].type==v_int) {
      if (c->a.vals[1].u.ival < -128 || c->a.vals[1].u.ival > 255)
	 ScriptError(c, "Bad value for argument");
      buf[0]=c->a.vals[1].u.ival;
      buf[1]=0;
      c->return_val.type=v_str;
      c->return_val.u.sval=fastrdup(buf);
   } else if (c->a.vals[1].type==v_arr || c->a.vals[1].type==v_arrfree) {
      Array *arr=c->a.vals[1].u.aval;

      temp=malloc((arr->argc + 1) * sizeof(char));
      for (i=0; i < arr->argc; ++i) {
	 if (arr->vals[i].type != v_int)
	    ScriptError(c, "Bad type for argument");
	 else if (arr->vals[i].u.ival < -128 || arr->vals[i].u.ival > 255)
	    ScriptError(c, "Bad value for argument");
	 temp[i]=arr->vals[i].u.ival;
      }
      temp[i]=0;
      c->return_val.type=v_str;
      c->return_val.u.sval=temp;
   } else
      ScriptError(c, "Bad type for argument");
}

static void bClear(Context *c) {
   FVClear(c->curfv);
}

static void bClearAllKerns(Context *c) {

   FVRemoveKerns(c->curfv);
}

static void bClearAllVKerns(Context *c) {

   FVRemoveVKerns(c->curfv);
}

static void bClearBackground(Context *c) {
   FVClearBackground(c->curfv);
}

static void bClearCharCounterMasks(Context *c) {
   SplineChar *sc;

   sc=GetOneSelChar(c);
   free(sc->countermasks);
   sc->countermasks=NULL;
   sc->countermask_cnt=0;
}

static void bClearHints(Context *c) {
   if (c->a.argc==1)
      FVClearHints(c->curfv);
   else if (c->a.vals[1].type==v_str) {
      int x_dir=0, y_dir=0;
      int i, gid;
      FontViewBase *fv=c->curfv;

      if (strmatch(c->a.vals[1].u.sval, "vertical")==0)
	 y_dir=1;
      else if (strmatch(c->a.vals[1].u.sval, "horizontal")==0)
	 x_dir=1;
      else if (strmatch(c->a.vals[1].u.sval, "diagonal")==0) {
	 x_dir=1;
	 y_dir=1;
      } else
	 ScriptError(c,
		     "Argument must be a string and must be \"Horizontal\", \"Vertical\" or \"Diagonal\".");

      for (i=0; i < fv->map->enccount; ++i)
	 if (fv->selected[i] &&
	     (gid=fv->map->map[i]) != -1
	     && SCWorthOutputting(fv->sf->glyphs[gid])) {
	    SplineChar *sc=fv->sf->glyphs[gid];

	    sc->manualhints=true;
	    if (y_dir && !x_dir) {
	       StemInfosFree(sc->vstem);
	       sc->vstem=NULL;
	       sc->vconflicts=false;
	    } else if (!y_dir && x_dir) {
	       StemInfosFree(sc->hstem);
	       sc->hstem=NULL;
	       sc->hconflicts=false;
	    } else if (y_dir && x_dir) {
	       DStemInfosFree(sc->dstem);
	       sc->dstem=NULL;
	    }
	 }
   } else
      ScriptError(c,
		  "Argument must be a string and must be \"Horizontal\", \"Vertical\" or \"Diagonal\".");
}

static void bClearInstrs(Context *c) {
   FVClearInstrs(c->curfv);
}

static void bClearPrivateEntry(Context *c) {
if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   if (c->curfv->sf->private != NULL)
      PSDictRemoveEntry(c->curfv->sf->private, c->a.vals[1].u.sval);
}

static void bClearTable(Context *c) {
   uint32_t tag;
   uint8_t _tag[4];
   SplineFont *sf=c->curfv->sf;
   struct ttf_table *table, *prev;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   if (strlen(c->a.vals[1].u.sval) > 4 || *c->a.vals[1].u.sval=='\0')
      ScriptError(c, "Table tag must be a 4 character ASCII string");
   _tag[0]=c->a.vals[1].u.sval[0];
   _tag[1]=_tag[2]=_tag[3]=' ';
   if (c->a.vals[1].u.sval[1] != '\0') {
      _tag[1]=c->a.vals[1].u.sval[1];
      if (c->a.vals[1].u.sval[2] != '\0') {
	 _tag[2]=c->a.vals[1].u.sval[2];
	 if (c->a.vals[1].u.sval[3] != '\0')
	    _tag[3]=c->a.vals[1].u.sval[3];
      }
   }
   tag=(_tag[0] << 24) | (_tag[1] << 16) | (_tag[2] << 8) | _tag[3];

   prev=NULL;
   for (table=sf->ttf_tables; table != NULL;
	prev=table, table=table->next)
      if (table->tag==tag)
	 break;

   c->return_val.type=v_int;
   c->return_val.u.ival=(table != NULL);
   if (table != NULL) {
      if (prev==NULL)
	 sf->ttf_tables=table->next;
      else
	 prev->next=table->next;
      free(table->data);
      chunkfree(table, sizeof(*table));
   } else {
      prev=NULL;
      for (table=sf->ttf_tab_saved; table != NULL;
	   prev=table, table=table->next)
	 if (table->tag==tag)
	    break;
      if (table != NULL) {
	 c->return_val.u.ival=true;
	 if (prev==NULL)
	    sf->ttf_tab_saved=table->next;
	 else
	    prev->next=table->next;
	 free(table->data);
	 chunkfree(table, sizeof(*table));
      }
   }
}

static void bClose(Context *c) {
   FontViewClose(c->curfv);
   c->curfv=NULL;
}

static void bCompareFonts(Context *c) {
   /* Compare the current font against the named one        */
   /* output to a file (used /dev/null if no output wanted) */
   /* flags control what tests are done                     */
   SplineFont *sf2=NULL;
   AFILE *diffs;
   int flags;
   char *t, *locfilename;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str
       || c->a.vals[3].type != v_int)
      ScriptError(c, "Bad type for argument");

   flags=c->a.vals[3].u.ival;

   if (strcmp(c->a.vals[2].u.sval, "-")==0)
      diffs=astdout;
   else
      diffs=afopen(c->a.vals[2].u.sval, "w");
   if (diffs==NULL)
      ScriptErrorString(c, "Failed to open output file", c->a.vals[2].u.sval);

   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   free(t);
   t=GFileMakeAbsoluteName(locfilename);
   free(locfilename);
   locfilename=t;
   sf2=FontWithThisFilename(locfilename);
   free(locfilename);
   if (sf2==NULL)
      ScriptErrorString(c,
			"Failed to find other font (it must be Open()ed first",
			c->a.vals[1].u.sval);

   c->return_val.type=v_int;
   c->return_val.u.ival =
      CompareFonts(c->curfv->sf, c->curfv->map, sf2, diffs, flags);
   afclose(diffs);
}

static void bCompareGlyphs(Context *c) {
   /* Compare selected glyphs against the contents of the clipboard    */
   /* Three comparisons:                                               */
   /*  1) Compare the points (base & control) of all contours          */
   /*  2) Compare that the splines themselves don't get too far appart */
   /*  3) Compare bitmaps                                              */
   /*  4) Compare hints/hintmasks                                      */
   real pt_err=.5, spline_err=1, bitmaps=-1;
   int bb_err=2, comp_hints=false, report_errors=true;

   if (c->a.argc >= 2) {
      if (c->a.vals[1].type==v_int)
	 pt_err=c->a.vals[1].u.ival;
      else if (c->a.vals[1].type==v_real)
	 pt_err=c->a.vals[1].u.fval;
      else
	 ScriptError(c, "Bad type for argument");
   }
   if (c->a.argc >= 3) {
      if (c->a.vals[2].type==v_int)
	 spline_err=c->a.vals[2].u.ival;
      else if (c->a.vals[2].type==v_real)
	 spline_err=c->a.vals[2].u.fval;
      else
	 ScriptError(c, "Bad type for argument");
   }
   if (c->a.argc >= 4) {
      if (c->a.vals[3].type==v_int)
	 bitmaps=c->a.vals[3].u.ival;
      else if (c->a.vals[3].type==v_real)
	 bitmaps=c->a.vals[3].u.fval;
      else
	 ScriptError(c, "Bad type for argument");
   }
   if (c->a.argc >= 5) {
      if (c->a.vals[4].type==v_int)
	 bb_err=c->a.vals[4].u.ival;
      else
	 ScriptError(c, "Bad type for argument");
   }
   if (c->a.argc >= 6) {
      if (c->a.vals[5].type==v_int)
	 comp_hints=c->a.vals[5].u.ival;
      else
	 ScriptError(c, "Bad type for argument");
   }
   if (c->a.argc >= 7) {
      if (c->a.vals[6].type==v_int)
	 report_errors=c->a.vals[6].u.ival;
      else
	 ScriptError(c, "Bad type for argument");
   }
   c->return_val.type=v_int;
   c->return_val.u.ival =
      CompareGlyphs(c, pt_err, spline_err, bitmaps, bb_err,
		    comp_hints, report_errors);
}

static void bControlAfmLigatureOutput(Context *c) {
   ScriptError(c, "This scripting function no longer works.");
}

static void bConvertByCMap(Context *c) {
   SplineFont *sf=c->curfv->sf;
   char *t;
   char *locfilename;

   if (c->a.vals[0].type != v_str)
      ScriptError(c, "Bad argument type");
   if (sf->cidmaster != NULL)
      ScriptErrorString(c, "Already a cid-keyed font",
			sf->cidmaster->fontname);
   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   MakeCIDMaster(sf, c->curfv->map, true, locfilename, NULL);
   free(t);
   free(locfilename);
}

static void bConvertToCID(Context *c) {
   SplineFont *sf=c->curfv->sf;
   struct cidmap *map;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str
       || c->a.vals[3].type != v_int)
      ScriptError(c, "Bad argument type");
   if (sf->cidmaster != NULL)
      ScriptErrorString(c, "Already a cid-keyed font",
			sf->cidmaster->fontname);
   map =
      FindCidMap(c->a.vals[1].u.sval, c->a.vals[2].u.sval,
		 c->a.vals[3].u.ival, sf);
   if (map==NULL)
      ScriptError(c, "No cidmap matching given ROS");
   MakeCIDMaster(sf, c->curfv->map, false, NULL, map);
}

static void bCopy(Context *c) {
   FVCopy(c->curfv, ct_fullcopy);
}

static void bCopyAnchors(Context *c) {
   FVCopyAnchors(c->curfv);
}

static void bCopyFgToBg(Context *c) {
   FVCopyFgtoBg(c->curfv);
}

static void bCopyGlyphFeatures(Context *c) {
   ScriptError(c, "This scripting function no longer works.");
}

static void bCopyLBearing(Context *c) {
   FVCopy(c->curfv, ut_lbearing);
}

static void bCopyRBearing(Context *c) {
   FVCopy(c->curfv, ut_rbearing);
}

static void bCopyReference(Context *c) {
   FVCopy(c->curfv, ct_reference);
}

static void bCopyUnlinked(Context *c) {
   FVCopy(c->curfv, ct_unlinkrefs);
}

static void bCopyVWidth(Context *c) {
   if (c->curfv != NULL && !c->curfv->sf->hasvmetrics)
      ScriptError(c, "Vertical metrics not enabled in this font");
   FVCopy(c->curfv, ut_vwidth);
}

static void bCopyWidth(Context *c) {
   FVCopy(c->curfv, ut_width);
}

static void bCorrectDirection(Context *c) {
   int i, gid;
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;
   int changed, refchanged;
   int checkrefs=true;
   RefChar *ref;
   SplineChar *sc;

   if (c->a.argc==2 && c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   else if (c->a.argc==2)
      checkrefs=c->a.vals[1].u.ival;
   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && (sc=sf->glyphs[gid]) != NULL
	  && fv->selected[i]) {
	 changed=refchanged=false;
	 if (checkrefs) {
	    for (ref=sc->layers[ly_fore].refs; ref != NULL; ref=ref->next) {
	       if (ref->transform[0] * ref->transform[3] < 0 ||
		   (ref->transform[0]==0
		    && ref->transform[1] * ref->transform[2] > 0)) {
		  if (!refchanged) {
		     refchanged=true;
		     SCPreserveState(sc, false);
		  }
		  SCRefToSplines(sc, ref, ly_fore);
	       }
	    }
	 }
	 if (!refchanged)
	    SCPreserveState(sc, false);
	 sc->layers[ly_fore].splines =
	    SplineSetsCorrect(sc->layers[ly_fore].splines, &changed);
	 if (changed || refchanged)
	    SCCharChangedUpdate(sc, ly_fore, true);
      }
}

static void bCos(Context *c) {
   double val;

   if (c->a.vals[1].type==v_real)
      val=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
      val=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_real;
   c->return_val.u.fval=cos(val);
}

static void bCut(Context *c) {
   FVCopy(c->curfv, ct_fullcopy);
   FVClear(c->curfv);
}

static void bDebugCrashFontForge(Context *c) {
   ErrorMsg(3,
	   "FontAnvil is crashing because you asked it to using the DebugCrashFontForge command\n");
   int *ptr=NULL;

   *ptr=1;
}

static void bDefaultOtherSubrs(Context *c) {
   DefaultOtherSubrs();
}

static void bDefaultRoundToGrid(Context *c) {
   int i, gid;
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;

   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i]) {
	 SplineChar *sc=sf->glyphs[gid];
	 RefChar *r;
	 int changed=false;

	 for (r=sc->layers[ly_fore].refs; r != NULL; r=r->next) {
	    if (!r->round_translation_to_grid && !r->point_match) {
	       if (!changed)
		  SCPreserveState(sc, false);
	       r->round_translation_to_grid=true;
	       changed=true;
	    }
	 }
	 if (changed)
	    SCCharChangedUpdate(sc, ly_fore, true);
      }
}

static void bDefaultUseMyMetrics(Context *c) {
   int i, gid;
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;

   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i]) {
	 SplineChar *sc=sf->glyphs[gid];
	 RefChar *r, *match=NULL, *goodmatch=NULL;
	 int already=false;

	 for (r=sc->layers[ly_fore].refs; r != NULL; r=r->next) {
	    if (r->use_my_metrics)
	       already=true;
	    if (r->sc->width==sc->width &&
		r->transform[0]==1 && r->transform[3]==1 &&
		r->transform[1]==0 && r->transform[2]==0 &&
		r->transform[4]==0 && r->transform[5]==0) {
	       if (match==NULL)
		  match=r;
	       /* I shan't bother checking for multiple matches */
	       /* I think the transform check is strict enough. */
	       if (r->unicode_enc >= 0 && r->unicode_enc < 0x10000 &&
		   isalpha(r->unicode_enc)) {
		  if (goodmatch==NULL) {
		     goodmatch=r;
		     break;
		  }
	       }
	    }
	 }

	 if (goodmatch==NULL)
	    goodmatch=match;
	 if (sc->layer_cnt==2 && !already && goodmatch != NULL) {
	    SCPreserveState(sc, false);
	    goodmatch->use_my_metrics=true;
	    SCCharChangedUpdate(sc, ly_fore, true);
	 }
      }
}

static void bDetachAndRemoveGlyphs(Context *c) {
   FontViewBase *fv=c->curfv;

   FVDetachAndRemoveGlyphs(fv);
}

static void bDetachGlyphs(Context *c) {
   FontViewBase *fv=c->curfv;

   FVDetachGlyphs(fv);
}

static void bDontAutoHint(Context *c) {
   FVDontAutoHint(c->curfv);
}

static void bDrawsSomething(Context *c) {
   SplineFont *sf=c->curfv->sf;
   EncMap *map=c->curfv->map;
   int gid;

   c->return_val.type=v_int;
   if (c->a.argc==1) {
      gid=map->map[GetOneSelCharIndex(c)];
   } else if (c->a.vals[1].type==v_int) {
      gid=c->a.vals[1].u.ival >= 0 && c->a.vals[1].u.ival < map->enccount ?
	 map->map[c->a.vals[1].u.ival] : -1;
   } else if (c->a.vals[1].type==v_unicode || c->a.vals[1].type==v_str) {
      int enc;

      if (c->a.vals[1].type==v_unicode)
	 enc=SFFindSlot(sf, map, c->a.vals[1].u.ival, NULL);
      else {
	 enc=NameToEncoding(sf, map, c->a.vals[1].u.sval);
      }
      gid=enc==-1 ? -1 : map->map[enc];
   } else
      ScriptError(c, "Bad type of argument");
   c->return_val.u.ival=gid != -1 && SCDrawsSomething(sf->glyphs[gid]);
}

static void bError(Context *c) {
   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Expected string argument");

   ScriptError(c, c->a.vals[1].u.sval);
}

static void bExp(Context *c) {
   double val;

   if (c->a.vals[1].type==v_real)
      val=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
      val=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_real;
   c->return_val.u.fval=exp(val);
}

static void bExpandStroke(Context *c) {
   StrokeInfo si;
   double args[8];
   int i;

   /* Arguments:
      2 => stroke width (implied butt, round)
      4 => stroke width, line cap, line join
      5 => stroke width, caligraphic angle, thickness-numerator, thickness-denom
      6 => stroke width, line cap, line join, 0, kanou's flags
      7 => stroke width, caligraphic angle, thickness-numerator, thickness-denom, 0, knaou's flags
    */

   for (i=1; i < c->a.argc; ++i) {
      if (c->a.vals[i].type==v_int)
	 args[i]=c->a.vals[i].u.ival;
      else if (c->a.vals[i].type==v_real)
	 args[i]=c->a.vals[i].u.fval;
      else
	 ScriptError(c, "Bad argument type");
   }
   memset(&si, 0, sizeof(si));
   si.radius=args[1] / 2.;
   si.minorradius=si.radius;
   si.stroke_type=si_std;
   if (c->a.argc==2) {
      si.join=lj_round;
      si.cap=lc_butt;
   } else if (c->a.argc==4) {
      si.cap=args[2];
      si.join=args[3];
   } else if (c->a.argc==6) {
      si.cap=args[2];
      si.join=args[3];
      if (c->a.vals[4].type != v_int || c->a.vals[4].u.ival != 0)
	 ScriptError(c, "If 5 arguments are given, the fourth must be zero");
      else if (c->a.vals[5].type != v_int)
	 ScriptError(c, "Bad argument type");
      if (c->a.vals[5].u.ival & 1)
	 si.removeinternal=true;
      else if (c->a.vals[5].u.ival & 2)
	 si.removeexternal=true;
      /*if ( c->a.vals[5].u.ival&4 )
         si.removeoverlapifneeded=true; *//* Obsolete */
   } else if (c->a.argc==5) {
      si.stroke_type=si_caligraphic;
      si.penangle=3.1415926535897932 * args[2] / 180;
      si.minorradius=si.radius * args[3] / (double) args[4];
   } else {
      si.stroke_type=si_caligraphic;
      si.penangle=3.1415926535897932 * args[2] / 180;
      si.minorradius=si.radius * args[3] / (double) args[4];
      if (c->a.vals[5].type != v_int || c->a.vals[5].u.ival != 0)
	 ScriptError(c, "If 6 arguments are given, the fifth must be zero");
      else if (c->a.vals[6].type != v_int)
	 ScriptError(c, "Bad argument type");
      if (c->a.vals[6].u.ival & 1)
	 si.removeinternal=true;
      else if (c->a.vals[6].u.ival & 2)
	 si.removeexternal=true;
      /* if ( c->a.vals[6].u.ival&4 )
         si.removeoverlapifneeded=true; *//* Obsolete */
   }
   FVStrokeItScript(c->curfv, &si, false);
}

static void bExport(Context *c) {
   int format, i, gid;
   BDFFont *bdf;
   char *pt, *format_spec;
   char buffer[20];
   char *t;

   if (c->a.vals[1].type != v_str
       || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad type of arguments");

   t=script2utf8_copy(c->a.vals[1].u.sval);
   pt=fastrdup(t);
   free(t);
   sprintf(buffer, "%%n_%%f.%.4s", pt);
   format_spec=buffer;
   if (strrchr(pt, '.') != NULL) {
      format_spec=pt;
      pt=strrchr(pt, '.') + 1;
   }
   if (strmatch(pt, "eps")==0)
      format=0;
   else if (strmatch(pt, "fig")==0)
      format=1;
   else if (strmatch(pt, "svg")==0)
      format=2;
   else if (strmatch(pt, "glif")==0)
      format=3;
   else if (strmatch(pt, "pdf")==0)
      format=4;
   else if (strmatch(pt, "plate")==0)
      format=5;
   else if (strmatch(pt, "xbm")==0)
      format=6;
   else if (strmatch(pt, "bmp")==0)
      format=7;
#   ifndef _NO_LIBPNG
   else if (strmatch(pt, "png")==0)
      format=8;
   else
      ScriptError(c, "Bad format (first arg must be eps/fig/xbm/bmp/png)");
#   else
   else
      ScriptError(c, "Bad format (first arg must be eps/fig/xbm/bmp)");
#   endif
   if ((format > 4 && c->a.argc != 3) || (format <= 4 && c->a.argc==3))
      ScriptError(c, "Wrong number of arguments");
   bdf=NULL;
   if (format > 4) {
      for (bdf=c->curfv->sf->bitmaps; bdf != NULL; bdf=bdf->next)
	 if ((BDFDepth(bdf)==1 && bdf->pixelsize==c->a.vals[2].u.ival) ||
	     (bdf->pixelsize != (c->a.vals[2].u.ival & 0xffff) &&
	      BDFDepth(bdf)==(c->a.vals[2].u.ival >> 16)))
	    break;
      if (bdf==NULL)
	 ScriptError(c, "No bitmap font matches the specified size");
   }
   for (i=0; i < c->curfv->map->enccount; ++i)
      if (c->curfv->selected[i] && (gid=c->curfv->map->map[i]) != -1 &&
	  SCWorthOutputting(c->curfv->sf->glyphs[gid]))
	 ScriptExport(c->curfv->sf, bdf, format, gid, format_spec,
		      c->curfv->map);
   if (format_spec != buffer)
      free(format_spec);
}

/* FontImage("Outputfilename",array of [pointsize,string][,width[,height]]) */

static void bFileAccess(Context *c) {
   if (c->a.vals[1].type != v_str
	    || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad type of argument");
   c->return_val.type=v_int;
   c->return_val.u.ival =
      access(c->a.vals[1].u.sval,
	     c->a.argc==3 ? c->a.vals[2].u.ival : R_OK);
}

static void bFindIntersections(Context *c) {
   FVOverlap(c->curfv, over_findinter);
}

static void bFindOrAddCvtIndex(Context *c) {
   int sign_matters=0;
   SplineFont *sf=c->curfv->sf;

   if (c->a.vals[1].type != v_int
       || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad argument type");
   if (c->a.argc==3)
      sign_matters=c->a.vals[2].u.ival;
   c->return_val.type=v_int;
   if (sign_matters)
      c->return_val.u.ival=TTF__getcvtval(sf, c->a.vals[1].u.ival);
   else
      c->return_val.u.ival=TTF_getcvtval(sf, c->a.vals[1].u.ival);
}

static void bFloor(Context *c) {
if (c->a.vals[1].type != v_real)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_int;
   c->return_val.u.ival=floor(c->a.vals[1].u.fval);
}

static void bFontImage(Context *c) {
   int i;
   char *pt, *t;
   int width=-1, height=-1;
   Array *arr;

   if (c->a.vals[1].type != v_str ||
       (c->a.vals[2].type != v_arr && c->a.vals[2].type != v_arrfree) ||
       (c->a.argc >= 4 && c->a.vals[3].type != v_int) ||
       (c->a.argc >= 5 && c->a.vals[4].type != v_int))
      ScriptError(c, "Bad type of arguments");

   t=script2utf8_copy(c->a.vals[1].u.sval);
   pt=strrchr(t, '.');
   if (pt==NULL || (strmatch(pt, ".bmp") != 0
#   ifndef _NO_LIBPNG
		      && strmatch(pt, ".png") != 0
#   endif
       ))
      ScriptError(c, "Unsupported image format");

   if (c->a.argc >= 4)
      width=c->a.vals[3].u.ival;
   if (c->a.argc >= 5)
      height=c->a.vals[4].u.ival;

   arr=c->a.vals[2].u.aval;
   if ((arr->argc & 1) && arr->argc >= 2)
      ScriptError(c,
		  "Second argument must be an array with an even number of entries");
   if (arr->argc==1) {
      if (arr->vals[0].type != v_int)
	 ScriptError(c,
		     "Second argument must be an array where each even numbered entry is an integer pixelsize");
   } else
      for (i=0; i < arr->argc; i += 2) {
	 if (arr->vals[i].type != v_int)
	    ScriptError(c,
			"Second argument must be an array where each even numbered entry is an integer pixelsize");
	 if (arr->vals[i + 1].type != v_str)
	    ScriptError(c,
			"Second argument must be an array where each odd numbered entry is a string");
      }

   FontImage(c->curfv->sf, t, arr, width, height);

   free(t);
}

static void bFontsInFile(Context *c) {
   char **ret;
   int cnt;
   char *t;
   char *locfilename;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "FontsInFile expects a filename");
   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   ret=GetFontNames(locfilename);
   free(t);
   free(locfilename);

   cnt=0;
   if (ret != NULL)
      for (cnt=0; ret[cnt] != NULL; ++cnt);
   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=cnt;
   c->return_val.u.aval->vals=malloc((cnt==0 ? 1 : cnt) * sizeof(Val));
   if (ret != NULL)
      for (cnt=0; ret[cnt] != NULL; ++cnt) {
	 c->return_val.u.aval->vals[cnt].type=v_str;
	 c->return_val.u.aval->vals[cnt].u.sval=ret[cnt];
      }
   free(ret);
}

static void bGenerate(Context *c) {
   SplineFont *sf=c->curfv->sf;
   char *bitmaptype="";
   int fmflags=-1;
   int res=-1;
   char *subfontdirectory=NULL;
   char *t;
   char *locfilename;
   NameList *rename_to=NULL;

   if (c->a.vals[1].type != v_str ||
       (c->a.argc >= 3 && c->a.vals[2].type != v_str) ||
       (c->a.argc >= 4 && c->a.vals[3].type != v_int) ||
       (c->a.argc >= 5 && c->a.vals[4].type != v_int) ||
       (c->a.argc >= 6 && c->a.vals[5].type != v_str) ||
       (c->a.argc >= 7 && c->a.vals[5].type != v_str))
      ScriptError(c, "Bad type of argument");
   if (c->a.argc >= 3)
      bitmaptype=c->a.vals[2].u.sval;
   if (c->a.argc >= 4)
      fmflags=c->a.vals[3].u.ival;
   if (c->a.argc >= 5)
      res=c->a.vals[4].u.ival;
   if (c->a.argc >= 6)
      subfontdirectory=c->a.vals[5].u.sval;
   if (c->a.argc >= 7) {
      rename_to=NameListByName(c->a.vals[6].u.sval);
      if (rename_to==NULL)
	 ScriptErrorString(c, "Could not find namelist: ",
			   c->a.vals[6].u.sval);
   }
   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   if (!GenerateScript
       (sf, locfilename, bitmaptype, fmflags, res, subfontdirectory, NULL,
	c->curfv->normal==NULL ? c->curfv->map : c->curfv->normal,
	rename_to, ly_fore))
      ScriptError(c, "Save failed");
   free(t);
   free(locfilename);
}

static int strtailcmp(char *needle,char *haystack) {
   int nlen=strlen(needle), hlen=strlen(haystack);

   if (nlen > hlen)
      return (-1);

   if (*needle=='/')
      return (strcmp(needle, haystack));

   return (strcmp(needle, haystack + hlen - nlen));
}

typedef SplineFont *SFArray[48];

static void bGenerateFamily(Context *c) {
   SplineFont *sf=NULL;
   char *bitmaptype="";
   int fmflags=-1;
   struct sflist *sfs, *cur, *lastsfs;
   Array *fonts;
   FontViewBase *fv;
   int i, j, fc, added;
   uint16_t psstyle;
   int fondcnt=0, fondmax=10;
   SFArray *familysfs=NULL;
   char *t;
   char *locfilename;

   familysfs=malloc((fondmax=10) * sizeof(SFArray));

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str ||
       c->a.vals[3].type != v_int || c->a.vals[4].type != v_arr)
      ScriptError(c, "Bad type of argument");
   bitmaptype=c->a.vals[2].u.sval;
   fmflags=c->a.vals[3].u.ival;

   fonts=c->a.vals[4].u.aval;
   for (i=0; i < fonts->argc; ++i) {
      if (fonts->vals[i].type != v_str)
	 ScriptError(c, "Values in the fontname array must be strings");
      for (fv=FontViewFirst(); fv != NULL; fv=fv->next)
	 if (fv->sf != sf)
	    if (strtailcmp(fonts->vals[i].u.sval, fv->sf->origname)==0 ||
		(fv->sf->filename != NULL
		 && strtailcmp(fonts->vals[i].u.sval, fv->sf->origname)==0))
	       break;
      if (fv==NULL)
	 for (fv=FontViewFirst(); fv != NULL; fv=fv->next)
	    if (fv->sf != sf)
	       if (strcmp(fonts->vals[i].u.sval, fv->sf->fontname)==0)
		  break;
      if (fv==NULL) {
	 ErrorMsg(2,"%s\n", fonts->vals[i].u.sval);
	 ScriptError(c, "The above font is not loaded");
      }
      if (sf==NULL)
	 sf=fv->sf;
      if (strcmp(fv->sf->familyname, sf->familyname) != 0)
	 ErrorMsg(2,"Warning: %s has a different family name than does %s (GenerateFamily)\n",
		  fv->sf->fontname, sf->fontname);
      MacStyleCode(fv->sf, &psstyle);
      if (psstyle >= 48) {
	 ErrorMsg(2,"%s(%s)\n", fv->sf->origname, fv->sf->fontname);
	 ScriptError(c, "A font can't be both Condensed and Expanded");
      }
      added=false;
      if (fv->sf->fondname==NULL) {
	 if (fondcnt > 0) {
	    for (j=0; j < 48; ++j)
	       if (familysfs[0][j] != NULL)
		  break;
	    if (familysfs[0][j]->fondname==NULL) {
	       if (familysfs[0][psstyle]==NULL
		   || familysfs[0][psstyle]==fv->sf) {
		  familysfs[0][psstyle]=fv->sf;
		  fv->sf->map=fv->map;
		  added=true;
	       }
	    }
	 }
      } else {
	 for (fc=0; fc < fondcnt; ++fc) {
	    for (j=0; j < 48; ++j)
	       if (familysfs[fc][j] != NULL)
		  break;
	    if (familysfs[fc][j]->fondname != NULL &&
		strcmp(familysfs[fc][j]->fondname, fv->sf->fondname)==0) {
	       if (familysfs[fc][psstyle]==NULL
		   || familysfs[fc][psstyle]==fv->sf) {
		  familysfs[fc][psstyle]=fv->sf;
		  added=true;
	       } else {
		  ErrorMsg(2,"%s(%s) and %s(%s) 0x%x in FOND %s\n",
			   familysfs[fc][psstyle]->origname,
			   familysfs[fc][psstyle]->fontname, fv->sf->origname,
			   fv->sf->fontname, psstyle, fv->sf->fondname);
		  ScriptError(c,
			      "Two array entries given with the same style (in the Mac's postscript style set)");
	       }
	    }
	 }
      }
      if (!added) {
	 if (fondcnt >= fondmax)
	    familysfs=realloc(familysfs, (fondmax += 10) * sizeof(SFArray));
	 memset(familysfs[fondcnt], 0, sizeof(SFArray));
	 familysfs[fondcnt++][psstyle]=fv->sf;
      }
   }
   if (familysfs[0][0]==NULL) {
      if (MacStyleCode(c->curfv->sf, NULL)==0
	  && strcmp(c->curfv->sf->familyname, sf->familyname) != 0)
	 familysfs[0][0]=c->curfv->sf;
      if (familysfs[0][0]==NULL)
	 ScriptError(c,
		     "At least one of the specified fonts must have a plain style");
   }
   sfs=lastsfs=NULL;
   for (fc=0; fc < fondcnt; ++fc) {
      for (j=0; j < 48; ++j)
	 if (familysfs[fc][j] != NULL) {
	    cur=chunkalloc(sizeof(struct sflist));
	    if (sfs==NULL)
	       sfs=cur;
	    else
	       lastsfs->next=cur;
	    lastsfs=cur;
	    cur->sf=familysfs[fc][j];
	    cur->map=cur->sf->map;
	 }
   }
   free(familysfs);

   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   if (!GenerateScript(sf, locfilename, bitmaptype, fmflags, -1, NULL, sfs,
		       c->curfv->normal ==
		       NULL ? c->curfv->map : c->curfv->normal, NULL,
		       ly_fore))
      ScriptError(c, "Save failed");
   free(t);
   free(locfilename);
   for (cur=sfs; cur != NULL; cur=sfs) {
      sfs=cur->next;
      /* free(cur->sizes); *//* Done inside GenerateScript */
      chunkfree(cur, sizeof(struct sflist));
   }
}

static void bGenerateFeatureFile(Context *c) {
   SplineFont *sf=c->curfv->sf;
   char *t;
   char *locfilename;
   OTLookup *otl=NULL;
   AFILE *out;
   int err;

   if (c->a.vals[1].type != v_str
       || (c->a.argc==3 && c->a.vals[2].type != v_str))
      ScriptError(c, "Bad type of argument");
   if (c->a.argc==3) {
      otl=SFFindLookup(sf, c->a.vals[2].u.sval);
      if (otl==NULL)
	 ScriptError(c, "Unknown lookup");
   }

   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   out=afopen(locfilename, "w");
   if (out==NULL)
      ScriptError(c, "Failed to open output file");
   if (otl != NULL)
      FeatDumpOneLookup(out, sf, otl);
   else
      FeatDumpFontLookups(out, sf);
   err=aferror(out);
   if (afclose(out) != 0 || err)
      ScriptError(c, "IO Error");
   free(t);
   free(locfilename);
}

static const char *FindNameOfFlag(struct flaglist *flags, int flag) {
   int i;

   for (i=0; flags[i].name != NULL; ++i) {
      if (flags[i].flag==flag)
	 return flags[i].name;
   }
   return NULL;
}

static void bGetAnchorPoints(Context *c) {
   SplineChar *sc;
   AnchorPoint *ap;
   int cnt;
   Array *ret, *temp;
   sc=GetOneSelChar(c);

   for (ap=sc->anchor, cnt=0; ap != NULL; ap=ap->next, ++cnt);
   ret=malloc(sizeof(Array));
   ret->argc=cnt;
   ret->vals=calloc(cnt, sizeof(Val));
   for (ap=sc->anchor, cnt=0; ap != NULL; ap=ap->next, ++cnt) {
      ret->vals[cnt].type=v_arr;
      ret->vals[cnt].u.aval=temp=malloc(sizeof(Array));
      if (ap->type==at_baselig) {
	 temp->argc=5;
	 temp->vals=calloc(6, sizeof(Val));
	 temp->vals[4].type=v_int;
	 temp->vals[4].u.ival=ap->lig_index;
      } else {
	 temp->argc=4;
	 temp->vals=calloc(5, sizeof(Val));
      }
      temp->vals[0].type=v_str;
      temp->vals[0].u.sval=fastrdup(ap->anchor->name);
      temp->vals[1].type=v_str;
      temp->vals[1].u.sval=fastrdup(FindNameOfFlag(ap_types, ap->type));
      temp->vals[2].type=v_real;
      temp->vals[2].u.fval=ap->me.x;
      temp->vals[3].type=v_real;
      temp->vals[3].u.fval=ap->me.y;
   }

   c->return_val.type=v_arrfree;
   c->return_val.u.aval=ret;
}

static void bGetCvtAt(Context *c) {
   SplineFont *sf=c->curfv->sf;
   struct ttf_table *tab;

   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   for (tab=sf->ttf_tables;
	tab != NULL && tab->tag != CHR('c', 'v', 't', ' '); tab=tab->next);
   if (tab==NULL || c->a.vals[1].u.ival >= tab->len / 2)
      ScriptError(c, "Cvt table is either not present or too short");
   c->return_val.type=v_int;
   c->return_val.u.ival=memushort(tab->data, tab->len,
				    sizeof(uint16_t) * c->a.vals[1].u.ival);
}

static void bGetEnv(Context *c) {
   char *env;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");
   if ((env=getenv(c->a.vals[1].u.sval))==NULL)
     env="";
   c->return_val.type=v_str;
   c->return_val.u.sval=fastrdup(env);
}

static void bGetFontBoundingBox(Context *c) {
   int i;
   SplineFont *sf=c->curfv->sf;
   DBounds b;

   SplineFontFindBounds(sf, &b);
   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=4;
   c->return_val.u.aval->vals=malloc(4 * sizeof(Val));
   for (i=0; i < 4; ++i)
      c->return_val.u.aval->vals[i].type=v_real;
   c->return_val.u.aval->vals[0].u.fval=b.minx;
   c->return_val.u.aval->vals[1].u.fval=b.miny;
   c->return_val.u.aval->vals[2].u.fval=b.maxx;
   c->return_val.u.aval->vals[3].u.fval=b.maxy;
}

static void bGetLookupInfo(Context *c) {
   OTLookup *otl;
   FeatureScriptLangList *fl;
   struct scriptlanglist *sl;
   int fcnt, scnt, l;
   Array *farray, *sarray, *larray;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");

   otl=SFFindLookup(c->curfv->sf, c->a.vals[1].u.sval);
   if (otl==NULL)
      ScriptErrorString(c, "Missing lookup", c->a.vals[1].u.sval);
   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=3;
   c->return_val.u.aval->vals=malloc(3 * sizeof(Val));
   c->return_val.u.aval->vals[0].type=v_str;
   c->return_val.u.aval->vals[0].u.sval =
      fastrdup(otl->lookup_type ==
	   gpos_single ? "GPOS_single" : otl->lookup_type ==
	   gpos_pair ? "GPOS_pair" : otl->lookup_type ==
	   gpos_cursive ? "GPOS_cursive" : otl->lookup_type ==
	   gpos_mark2base ? "GPOS_marktobase" : otl->lookup_type ==
	   gpos_mark2ligature ? "GPOS_marktoligature" : otl->lookup_type ==
	   gpos_mark2mark ? "GPOS_marktomark" : otl->lookup_type ==
	   gpos_context ? "GPOS_context" : otl->lookup_type ==
	   gpos_contextchain ? "GPOS_contextchain" : otl->lookup_type ==
	   kern_statemachine ? "kern_statemachine" : otl->lookup_type ==
	   gsub_single ? "GSUB_single" : otl->lookup_type ==
	   gsub_multiple ? "GSUB_multiple" : otl->lookup_type ==
	   gsub_alternate ? "GSUB_alternate" : otl->lookup_type ==
	   gsub_ligature ? "GSUB_ligature" : otl->lookup_type ==
	   gsub_context ? "GSUB_context" : otl->lookup_type ==
	   gsub_contextchain ? "GSUB_contextchain" : otl->lookup_type ==
	   gsub_reversecchain ? "GSUB_reversecchain" : otl->lookup_type ==
	   morx_indic ? "morx_indic" : otl->lookup_type ==
	   morx_context ? "morx_context" : "morx_insert");
   c->return_val.u.aval->vals[1].type=v_int;
   c->return_val.u.aval->vals[1].u.ival=otl->lookup_flags;
   c->return_val.u.aval->vals[2].type=v_arrfree;
   c->return_val.u.aval->vals[2].u.aval=farray=malloc(sizeof(Array));
   for (fl=otl->features, fcnt=0; fl != NULL; fl=fl->next, ++fcnt);
   farray->argc=fcnt;
   farray->vals=malloc(fcnt * sizeof(Val));
   for (fl=otl->features, fcnt=0; fl != NULL; fl=fl->next, ++fcnt) {
      farray->vals[fcnt].type=v_arrfree;
      farray->vals[fcnt].u.aval=malloc(sizeof(Array));
      farray->vals[fcnt].u.aval->argc=2;
      farray->vals[fcnt].u.aval->vals=malloc(2 * sizeof(Val));
      farray->vals[fcnt].u.aval->vals[0].type=v_str;
      farray->vals[fcnt].u.aval->vals[0].u.sval =
	 Tag2Str(fl->featuretag, fl->ismac);
      farray->vals[fcnt].u.aval->vals[1].type=v_arrfree;
      farray->vals[fcnt].u.aval->vals[1].u.aval=sarray =
	 malloc(sizeof(Array));
      for (sl=fl->scripts, scnt=0; sl != NULL; sl=sl->next, ++scnt);
      sarray->argc=scnt;
      sarray->vals=malloc(scnt * sizeof(Val));
      for (sl=fl->scripts, scnt=0; sl != NULL; sl=sl->next, ++scnt) {
	 sarray->vals[scnt].type=v_arrfree;
	 sarray->vals[scnt].u.aval=malloc(sizeof(Array));
	 sarray->vals[scnt].u.aval->argc=2;
	 sarray->vals[scnt].u.aval->vals=malloc(2 * sizeof(Val));
	 sarray->vals[scnt].u.aval->vals[0].type=v_str;
	 sarray->vals[scnt].u.aval->vals[0].u.sval =
	    Tag2Str(sl->script, false);
	 sarray->vals[scnt].u.aval->vals[1].type=v_arrfree;
	 sarray->vals[scnt].u.aval->vals[1].u.aval=larray =
	    malloc(sizeof(Array));
	 larray->argc=sl->lang_cnt;
	 larray->vals=malloc(sl->lang_cnt * sizeof(Val));
	 for (l=0; l < sl->lang_cnt; ++l) {
	    larray->vals[l].type=v_str;
	    larray->vals[l].u.sval =
	       Tag2Str(l <
		       MAX_LANG ? sl->langs[l] : sl->morelangs[l - MAX_LANG],
		       false);
	 }
      }
   }
}

static void bGetLookupOfSubtable(Context *c) {
   struct lookup_subtable *sub;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");
   sub=SFFindLookupSubtable(c->curfv->sf, c->a.vals[1].u.sval);
   if (sub==NULL)
      ScriptErrorString(c, "Unknown lookup subtable", c->a.vals[1].u.sval);
   c->return_val.type=v_str;
   c->return_val.u.sval=fastrdup(sub->lookup->lookup_name);
}

static void bGetLookupSubtables(Context *c) {
   OTLookup *otl;
   struct lookup_subtable *sub;
   int cnt;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");

   otl=SFFindLookup(c->curfv->sf, c->a.vals[1].u.sval);
   if (otl==NULL)
      ScriptErrorString(c, "Missing lookup", c->a.vals[1].u.sval);
   for (sub=otl->subtables, cnt=0; sub != NULL; sub=sub->next, ++cnt);

   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=cnt;
   c->return_val.u.aval->vals=malloc(cnt * sizeof(Val));
   for (sub=otl->subtables, cnt=0; sub != NULL; sub=sub->next, ++cnt) {
      c->return_val.u.aval->vals[cnt].type=v_str;
      c->return_val.u.aval->vals[cnt].u.sval=fastrdup(sub->subtable_name);
   }
}

static void bGetLookups(Context *c) {
   OTLookup *otl, *base;
   int cnt;
   SplineFont *sf=c->curfv->sf;

   if (sf->cidmaster)
      sf=sf->cidmaster;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");

   if (strmatch(c->a.vals[1].u.sval, "GPOS")==0)
      base=sf->gpos_lookups;
   else if (strmatch(c->a.vals[1].u.sval, "GSUB")==0)
      base=sf->gsub_lookups;
   else
      ScriptError(c,
		  "Argument to \"GetLookups\" must be either \"GPOS\" or \"GSUB\"");

   for (otl=base, cnt=0; otl != NULL; otl=otl->next, ++cnt);
   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=cnt;
   c->return_val.u.aval->vals=malloc(cnt * sizeof(Val));
   for (otl=base, cnt=0; otl != NULL; otl=otl->next, ++cnt) {
      c->return_val.u.aval->vals[cnt].type=v_str;
      c->return_val.u.aval->vals[cnt].u.sval=fastrdup(otl->lookup_name);
   }
}

static void bGetMaxpValue(Context *c) {
   SplineFont *sf=c->curfv->sf;
   struct ttf_table *tab;
   uint8_t *data, dummy[32];

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");

   memset(dummy, 0, 32);
   dummy[15]=2;
   tab=SFFindTable(sf, CHR('m', 'a', 'x', 'p'));
   if (tab==NULL)
      data=dummy;
   else if (tab->len < 32) {
      memcpy(dummy, tab->data, tab->len);
      data=dummy;
   } else
      data=tab->data;

   c->return_val.type=v_int;
   if (strmatch(c->a.vals[1].u.sval, "Zones")==0)
      c->return_val.u.ival=memushort(data, 32, 7 * sizeof(uint16_t));
   else if (strmatch(c->a.vals[1].u.sval, "TwilightPntCnt")==0)
      c->return_val.u.ival=memushort(data, 32, 8 * sizeof(uint16_t));
   else if (strmatch(c->a.vals[1].u.sval, "StorageCnt")==0)
      c->return_val.u.ival=memushort(data, 32, 9 * sizeof(uint16_t));
   else if (strmatch(c->a.vals[1].u.sval, "MaxStackDepth")==0)
      c->return_val.u.ival=memushort(data, 32, 12 * sizeof(uint16_t));
   else if (strmatch(c->a.vals[1].u.sval, "FDEFs")==0)
      c->return_val.u.ival=memushort(data, 32, 10 * sizeof(uint16_t));
   else if (strmatch(c->a.vals[1].u.sval, "IDEFs")==0)
      c->return_val.u.ival=memushort(data, 32, 11 * sizeof(uint16_t));
   else
      ScriptErrorString(c, "Unknown 'maxp' field: ", c->a.vals[1].u.sval);
}

static void bGetOS2Value(Context *c) {
   int i;
   SplineFont *sf=c->curfv->sf;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");

   if (strmatch(c->a.vals[1].u.sval, "Weight")==0) {
      os2getint(sf->pfminfo.weight, c);
   } else if (strmatch(c->a.vals[1].u.sval, "Width")==0) {
      os2getint(sf->pfminfo.width, c);
   } else if (strmatch(c->a.vals[1].u.sval, "FSType")==0) {
      os2getint(sf->pfminfo.fstype, c);
   } else if (strmatch(c->a.vals[1].u.sval, "IBMFamily")==0) {
      os2getint(sf->pfminfo.os2_family_class, c);
   } else if (strmatch(c->a.vals[1].u.sval, "VendorID")==0) {
      c->return_val.type=v_str;
      c->return_val.u.sval=copyn(sf->pfminfo.os2_vendor, 4);
   } else if (strmatch(c->a.vals[1].u.sval, "WinAscent")==0) {
      os2getint(sf->pfminfo.os2_winascent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "WinAscentIsOffset")==0) {
      os2getint(sf->pfminfo.winascent_add, c);
   } else if (strmatch(c->a.vals[1].u.sval, "WinDescent")==0) {
      os2getint(sf->pfminfo.os2_windescent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "WinDescentIsOffset")==0) {
      os2getint(sf->pfminfo.windescent_add, c);
   } else if (strmatch(c->a.vals[1].u.sval, "typoAscent")==0) {
      os2getint(sf->pfminfo.os2_typoascent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "typoAscentIsOffset")==0) {
      os2getint(sf->pfminfo.typoascent_add, c);
   } else if (strmatch(c->a.vals[1].u.sval, "typoDescent")==0) {
      os2getint(sf->pfminfo.os2_typodescent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "typoDescentIsOffset")==0) {
      os2getint(sf->pfminfo.typodescent_add, c);
   } else if (strmatch(c->a.vals[1].u.sval, "typoLineGap")==0) {
      os2getint(sf->pfminfo.os2_typolinegap, c);
   } else if (strmatch(c->a.vals[1].u.sval, "hheadAscent")==0) {
      os2getint(sf->pfminfo.hhead_ascent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "hheadAscentIsOffset")==0) {
      os2getint(sf->pfminfo.hheadascent_add, c);
   } else if (strmatch(c->a.vals[1].u.sval, "hheadDescent")==0) {
      os2getint(sf->pfminfo.hhead_descent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "hheadDescentIsOffset")==0) {
      os2getint(sf->pfminfo.hheaddescent_add, c);
   } else if (strmatch(c->a.vals[1].u.sval, "LineGap")==0
	      || strmatch(c->a.vals[1].u.sval, "HHeadLineGap")==0) {
      os2getint(sf->pfminfo.linegap, c);
   } else if (strmatch(c->a.vals[1].u.sval, "VLineGap")==0
	      || strmatch(c->a.vals[1].u.sval, "VHeadLineGap")==0) {
      os2getint(sf->pfminfo.vlinegap, c);
   } else if (strmatch(c->a.vals[1].u.sval, "Panose")==0) {
      c->return_val.type=v_arrfree;
      c->return_val.u.aval=malloc(sizeof(Array));
      c->return_val.u.aval->argc=10;
      c->return_val.u.aval->vals=malloc((10 + 1) * sizeof(Val));
      for (i=0; i < 10; ++i) {
	 c->return_val.u.aval->vals[i].type=v_int;
	 c->return_val.u.aval->vals[i].u.ival=sf->pfminfo.panose[i];
      }
   } else if (strmatch(c->a.vals[1].u.sval, "SubXSize")==0) {
      os2getint(sf->pfminfo.os2_subxsize, c);
   } else if (strmatch(c->a.vals[1].u.sval, "SubYSize")==0) {
      os2getint(sf->pfminfo.os2_subysize, c);
   } else if (strmatch(c->a.vals[1].u.sval, "SubXOffset")==0) {
      os2getint(sf->pfminfo.os2_subxoff, c);
   } else if (strmatch(c->a.vals[1].u.sval, "SubYOffset")==0) {
      os2getint(sf->pfminfo.os2_subyoff, c);
   } else if (strmatch(c->a.vals[1].u.sval, "supXSize")==0) {
      os2getint(sf->pfminfo.os2_supxsize, c);
   } else if (strmatch(c->a.vals[1].u.sval, "supYSize")==0) {
      os2getint(sf->pfminfo.os2_supysize, c);
   } else if (strmatch(c->a.vals[1].u.sval, "supXOffset")==0) {
      os2getint(sf->pfminfo.os2_supxoff, c);
   } else if (strmatch(c->a.vals[1].u.sval, "supYOffset")==0) {
      os2getint(sf->pfminfo.os2_supyoff, c);
   } else if (strmatch(c->a.vals[1].u.sval, "StrikeOutSize")==0) {
      os2getint(sf->pfminfo.os2_strikeysize, c);
   } else if (strmatch(c->a.vals[1].u.sval, "StrikeOutPos")==0) {
      os2getint(sf->pfminfo.os2_strikeypos, c);
   } else {
      ScriptErrorString(c, "Unknown OS/2 field: ", c->a.vals[1].u.sval);
   }
}

static void bGetPosSub(Context *c) {
   SplineFont *sf=c->curfv->sf;
   EncMap *map=c->curfv->map;
   SplineChar *sc;
   int i, j, k, cnt, subcnt, found, gid;
   SplineChar dummy;
   Array *ret, *temp;
   PST *pst;
   KernPair *kp;
   char *pt, *start;
   struct lookup_subtable *sub;

   found=GetOneSelCharIndex(c);
   gid=map->map[found];
   sc=gid==-1 ? NULL : sf->glyphs[gid];
   if (sc==NULL)
      sc=SCBuildDummy(&dummy, sf, map, found);

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   if (*c->a.vals[1].u.sval=='*')
      sub=NULL;
   else {
      sub=SFFindLookupSubtable(sf, c->a.vals[1].u.sval);
      if (sub==NULL)
	 ScriptErrorString(c, "Unknown lookup subtable", c->a.vals[1].u.sval);
   }

   for (i=0; i < 2; ++i) {
      cnt=0;
      for (pst=sc->possub; pst != NULL; pst=pst->next) {
	 if (pst->type==pst_lcaret)
	    continue;
	 if (pst->subtable==sub || sub==NULL) {
	    if (i) {
	       ret->vals[cnt].type=v_arr;
	       ret->vals[cnt].u.aval=temp=malloc(sizeof(Array));
	       switch (pst->type) {
		 default:
		    free(temp);
		    ret->vals[cnt].type=v_void;
/* The important things here should not be translated. We hope the user will */
/*  never see this. Let's not translate it at all */
		    ErrorMsg(2,"Unexpected PST type in GetPosSub (%d).\n",
			     pst->type);
		    break;
		 case pst_position:
		    temp->argc=6;
		    temp->vals=calloc(7, sizeof(Val));
		    temp->vals[1].u.sval=fastrdup("Position");
		    for (k=2; k < 6; ++k) {
		       temp->vals[k].type=v_int;
		       temp->vals[k].u.ival=(&pst->u.pos.xoff)[(k - 2)];
		    }
		    break;
		 case pst_pair:
		    temp->argc=11;
		    temp->vals=calloc(11, sizeof(Val));
		    temp->vals[1].u.sval=fastrdup("Pair");
		    temp->vals[2].type=v_str;
		    temp->vals[2].u.sval=fastrdup(pst->u.pair.paired);
		    for (k=3; k < 11; ++k) {
		       temp->vals[k].type=v_int;
		       temp->vals[k].u.ival =
			  (&pst->u.pair.vr[(k - 3) / 4].xoff)[(k - 3) & 3];
		    }
		    break;
		 case pst_substitution:
		    temp->argc=3;
		    temp->vals=calloc(4, sizeof(Val));
		    temp->vals[1].u.sval=fastrdup("Substitution");
		    temp->vals[2].type=v_str;
		    temp->vals[2].u.sval=fastrdup(pst->u.subs.variant);
		    break;
		 case pst_alternate:
		 case pst_multiple:
		 case pst_ligature:
		    for (pt=pst->u.mult.components, subcnt=1; *pt; ++pt) {
		       if (*pt==' ') {
			  ++subcnt;
			  while (pt[1]==' ')
			     ++pt;
		       }
		    }
		    temp->argc=2 + subcnt;
		    temp->vals=calloc(2 + subcnt, sizeof(Val));
		    temp->vals[1].u.sval =
		       fastrdup(pst->type ==
			    pst_alternate ? "AltSubs" : pst->type ==
			    pst_multiple ? "MultSubs" : "Ligature");
		    for (pt=pst->u.mult.components, subcnt=0; *pt;) {
		       while (*pt==' ')
			  ++pt;
		       if (*pt=='\0')
			  break;
		       start=pt;
		       while (*pt != ' ' && *pt != '\0')
			  ++pt;
		       temp->vals[2 + subcnt].type=v_str;
		       temp->vals[2 + subcnt].u.sval =
			  copyn(start, pt - start);
		       ++subcnt;
		    }
		    break;
	       }
	       if (ret->vals[cnt].type==v_arr) {
		  temp->vals[0].type=v_str;
		  temp->vals[0].u.sval=fastrdup(pst->subtable->subtable_name);
		  temp->vals[1].type=v_str;
	       }
	    }
	    ++cnt;
	 }
      }
      for (j=0; j < 2; ++j) {
	 if (sub==NULL || sub->lookup->lookup_type==gpos_pair) {
	    for (kp=(j==0 ? sc->kerns : sc->vkerns); kp != NULL;
		 kp=kp->next) {
	       if (sub==NULL || sub==kp->subtable) {
		  if (i) {
		     ret->vals[cnt].type=v_arr;
		     ret->vals[cnt].u.aval=temp=malloc(sizeof(Array));
		     temp->argc=11;
		     temp->vals=calloc(temp->argc, sizeof(Val));
		     temp->vals[0].type=v_str;
		     temp->vals[0].u.sval=fastrdup(kp->subtable->subtable_name);
		     temp->vals[1].type=v_str;
		     temp->vals[1].u.sval=fastrdup("Pair");
		     temp->vals[2].type=v_str;
		     temp->vals[2].u.sval=fastrdup(kp->sc->name);
		     for (k=3; k < 11; ++k) {
			temp->vals[k].type=v_int;
			temp->vals[k].u.ival=0;
		     }
		     if (j)
			temp->vals[6].u.ival=kp->off;
		     else if (SCRightToLeft(sc))
			temp->vals[9].u.ival=kp->off;
		     else
			temp->vals[5].u.ival=kp->off;
		  }
		  ++cnt;
	       }
	    }
	 }
      }
      if (i==0) {
	 ret=malloc(sizeof(Array));
	 ret->argc=cnt;
	 ret->vals=calloc(cnt, sizeof(Val));
      }
   }
   c->return_val.type=v_arrfree;
   c->return_val.u.aval=ret;
}

static void bGetPrefs(Context *c) {
   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");
   if (!GetPrefs(c->a.vals[1].u.sval, &c->return_val))
      ScriptErrorString(c, "Unknown Preference variable",
			c->a.vals[1].u.sval);
}

static void bGetPrivateEntry(Context *c) {
   int i;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   c->return_val.type=v_str;
   if (c->curfv->sf->private==NULL ||
       (i =
	PSDictFindEntry(c->curfv->sf->private, c->a.vals[1].u.sval))==-1)
      c->return_val.u.sval=fastrdup("");
   else
      c->return_val.u.sval=fastrdup(c->curfv->sf->private->values[i]);
}

static void bGetSubtableOfAnchorClass(Context *c) {
   AnchorClass *ac;
   SplineFont *sf=c->curfv->sf;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");
   if (sf->cidmaster != NULL)
      sf=sf->cidmaster;

   for (ac=sf->anchor; ac != NULL; ac=ac->next)
      if (strcmp(ac->name, c->a.vals[1].u.sval)==0)
	 break;
   if (ac==NULL)
      ScriptErrorString(c, "Unknown anchor class", c->a.vals[1].u.sval);
   c->return_val.type=v_str;
   c->return_val.u.sval =
      fastrdup(ac->subtable ? ac->subtable->subtable_name : "");
}

static void bGetTTFName(Context *c) {
   SplineFont *sf=c->curfv->sf;
   int lang, strid;
   struct ttflangname *ln;
   if (sf->cidmaster != NULL)
      sf=sf->cidmaster;
   if (c->a.vals[1].type != v_int || c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");

   lang=c->a.vals[1].u.ival;
   strid=c->a.vals[2].u.ival;
   if (lang < 0 || lang > 0xffff)
      ScriptError(c, "Bad value for language");
   else if (strid < 0 || strid >= ttf_namemax)
      ScriptError(c, "Bad value for string id");

   c->return_val.type=v_str;

   for (ln=sf->names; ln != NULL && ln->lang != lang; ln=ln->next);
   if (ln==NULL || ln->names[strid]==NULL)
      c->return_val.u.sval=fastrdup("");
   else
      c->return_val.u.sval=fastrdup(ln->names[strid]);
}

static void bGetTeXParam(Context *c) {
   SplineFont *sf=c->curfv->sf;

   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   else if (c->a.vals[1].u.ival < -1 || c->a.vals[1].u.ival >= 24)
      ScriptError(c, "Bad argument value (must be >=-1 <=24)");
   c->return_val.type=v_int;
   if (sf->texdata.type==tex_unset)
      TeXDefaultParams(sf);
   if (c->a.vals[1].u.ival==-1)
      c->return_val.u.ival=sf->texdata.type;
   else
      c->return_val.u.ival=sf->texdata.params[c->a.vals[1].u.ival];
}

static void bHFlip(Context *c) {
   real trans[6];
   int otype=1;
   BVTFunc bvts[2];

   trans[0]=-1;
   trans[3]=1;
   trans[1]=trans[2]=trans[4]=trans[5]=0;
   if (c->a.argc==1)
      /* default to center of char for origin */ ;
   else if (c->a.argc==2) {
      if (c->a.vals[1].type != v_int && c->a.vals[1].type != v_real)
	 ScriptError(c, "Bad argument type in HFlip");
      if (c->a.vals[1].type==v_int)
	 trans[4]=2 * c->a.vals[1].u.ival;
      else
	 trans[4]=2 * c->a.vals[1].u.fval;
      otype=0;
   }
   bvts[0].func=bvt_fliph;
   bvts[1].func=bvt_none;
   FVTransFunc(c->curfv, trans, otype, bvts, true);
}

static void bHasPreservedTable(Context *c) {
   SplineFont *sf=c->curfv->sf;
   uint32_t tag;
   char *tstr, *end;
   struct ttf_table *tab;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");

   tstr=c->a.vals[1].u.sval;
   end=tstr + strlen(tstr);
   if (*tstr=='\0' || end - tstr > 4)
      ScriptError(c, "Bad tag");
   tag=*tstr << 24;
   tag |= (tstr + 1 < end ? tstr[1] : ' ') << 16;
   tag |= (tstr + 2 < end ? tstr[2] : ' ') << 8;
   tag |= (tstr + 3 < end ? tstr[3] : ' ');

   for (tab=sf->ttf_tab_saved; tab != NULL && tab->tag != tag;
	tab=tab->next);
   c->return_val.type=v_int;
   c->return_val.u.ival=(tab != NULL);
}

static void bHasPrivateEntry(Context *c) {
   SplineFont *sf=c->curfv->sf;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   c->return_val.type=v_int;
   c->return_val.u.ival=0;
   if (PSDictHasEntry(sf->private, c->a.vals[1].u.sval) != NULL)	/* this works if sf->private==NULL */
      c->return_val.u.ival=1;
}

static void bImport(Context *c) {
   char *ext, *filename;
   int format, back, ok, flags;
   char *t;
   char *locfilename;

   if (c->a.vals[1].type != v_str ||
       (c->a.argc >= 3 && c->a.vals[2].type != v_int) ||
       (c->a.argc==4 && c->a.vals[3].type != v_int))
      ScriptError(c, "Bad type of argument");

   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   filename=GFileMakeAbsoluteName(locfilename);
   free(locfilename);
   free(t);

   ext=strrchr(filename, '.');
   if (ext==NULL) {
      int len=strlen(filename);

      ext=filename + len - 2;
      if (ext[0] != 'p' || ext[1] != 'k')
	 ScriptErrorString(c, "No extension in", filename);
   }
   back=0;
   flags=-1;
   if (strmatch(ext, ".bdf")==0 || strmatch(ext - 4, ".bdf.gz")==0)
      format=fv_bdf;
   else if (strmatch(ext, ".pcf")==0 || strmatch(ext - 4, ".pcf.gz")==0)
      format=fv_pcf;
   else if (strmatch(ext, ".ttf")==0 || strmatch(ext, ".otf")==0
	    || strmatch(ext, ".otb")==0)
      format=fv_ttf;
   else if (strmatch(ext, "pk")==0 || strmatch(ext, ".pk")==0) {
      format=fv_pk;
      back=true;
   } else if (strmatch(ext, ".eps")==0 || strmatch(ext, ".ps")==0 ||
	      strmatch(ext, ".art")==0) {
      if (strchr(filename, '*') != NULL)
	 format=fv_epstemplate;
      else
	 format=fv_eps;
   } else if (strmatch(ext, ".pdf")==0) {
      if (strchr(filename, '*') != NULL)
	 format=fv_pdftemplate;
      else
	 format=fv_pdf;
   } else if (strmatch(ext, ".svg")==0) {
      if (strchr(filename, '*') != NULL)
	 format=fv_svgtemplate;
      else
	 format=fv_svg;
   } else {
      if (strchr(filename, '*') != NULL)
	 format=fv_imgtemplate;
      else
	 format=fv_image;
      back=true;
   }
   if (c->a.argc >= 3)
      back=c->a.vals[2].u.ival;
   if (c->a.argc >= 4)
      flags=c->a.vals[3].u.ival;
   if (format==fv_bdf)
      ok=FVImportBDF(c->curfv, filename, false, back);
   else if (format==fv_pcf)
      ok=FVImportBDF(c->curfv, filename, 2, back);
   else if (format==fv_ttf)
      ok=FVImportMult(c->curfv, filename, back, bf_ttf);
   else if (format==fv_pk)
      ok=FVImportBDF(c->curfv, filename, true, back);
   else if (format==fv_image || format==fv_eps || format==fv_svg
	    || format==fv_pdf)
      ok=FVImportImages(c->curfv, filename, format, back, flags);
   else
      ok=FVImportImageTemplate(c->curfv, filename, format, back, flags);
   free(filename);
   if (!ok)
      ScriptError(c, "Import failed");
}

static void bInFont(Context *c) {
   SplineFont *sf=c->curfv->sf;
   EncMap *map=c->curfv->map;

   c->return_val.type=v_int;
   if (c->a.vals[1].type==v_int)
      c->return_val.u.ival=c->a.vals[1].u.ival >= 0
	 && c->a.vals[1].u.ival < map->enccount;
   else if (c->a.vals[1].type==v_unicode || c->a.vals[1].type==v_str) {
      int enc;

      if (c->a.vals[1].type==v_unicode)
	 enc=SFFindSlot(sf, map, c->a.vals[1].u.ival, NULL);
      else {
	 enc=NameToEncoding(sf, map, c->a.vals[1].u.sval);
      }
      c->return_val.u.ival=(enc != -1);
   } else
      ScriptError(c, "Bad type of argument");
}

static void bInline(Context *c) {
   if (c->a.vals[1].type != v_int || c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");
   FVInline(c->curfv, c->a.vals[1].u.ival, c->a.vals[2].u.ival);
}

static void bInt(Context *c) {
   if (c->a.vals[1].type==v_real)
      c->return_val.u.ival=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_unicode || c->a.vals[1].type==v_int)
      c->return_val.u.ival=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_int;
}

static void bInterpolateFonts(Context *c) {
   SplineFont *sf;
   int openflags=0;
   float percent;
   char *t;
   char *locfilename;

   if (c->a.vals[1].type != v_int && c->a.vals[1].type != v_real)
      ScriptError(c, "Bad argument type for first arg");
   else if (c->a.vals[2].type != v_str)
      ScriptError(c, "InterpolateFonts expects a filename");
   else if (c->a.argc==4) {
      if (c->a.vals[3].type != v_int)
	 ScriptError(c,
		     "InterpolateFonts expects an integer for third argument");
      openflags=c->a.vals[3].u.ival;
   }
   if (c->a.vals[1].type==v_int)
      percent=c->a.vals[1].u.ival;
   else
      percent=c->a.vals[1].u.fval;
   t=script2utf8_copy(c->a.vals[2].u.sval);
   locfilename=fastrdup(t);
   sf=LoadSplineFont(locfilename, openflags);
   free(t);
   free(locfilename);
   if (sf==NULL)
      ScriptErrorString(c, "Can't find font", c->a.vals[2].u.sval);
   if (sf->fv==NULL)
      EncMapFree(sf->map);
   c->curfv =
      FVAppend(_FontViewCreate
	       (InterpolateFont
		(c->curfv->sf, sf, percent / 100.0, c->curfv->map->enc)));
}

static void bIsFinite(Context *c) {
if (c->a.vals[1].type != v_real)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_int;
   c->return_val.u.ival=isfinite(c->a.vals[1].u.fval);
}

static void bIsNan(Context *c) {
if (c->a.vals[1].type != v_real)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_int;
   c->return_val.u.ival=isnan(c->a.vals[1].u.fval);
}

static void bItalic(Context *c) {
   double pct=1.0;
   int i;

   for (i=1; i < c->a.argc; ++i) {
      switch (i) {
	case 1:
	   /* Argument 1: italic angle */
	   if (c->a.vals[1].type==v_real)
	      default_ii.italic_angle=c->a.vals[1].u.fval;
	   else if (c->a.vals[1].type==v_int)
	      default_ii.italic_angle=c->a.vals[1].u.ival;
	   else
	      break;
	   continue;

	case 2:
	   /* Argument 2: xheight percentage */
	   if (c->a.vals[i].type==v_real)
	      default_ii.xheight_percent=c->a.vals[i].u.fval;
	   else if (c->a.vals[i].type==v_int)
	      default_ii.xheight_percent=c->a.vals[i].u.ival;
	   else
	      break;
	   continue;

	case 3:
	   /* Argument 3: flags */
	   if (c->a.vals[i].type==v_int) {
	      int flags=c->a.vals[i].u.ival;

	      if (flags >= 0) {
		 default_ii.transform_bottom_serifs =
		    (flags & 0x0001) ? 1 : 0;
		 default_ii.transform_top_xh_serifs =
		    (flags & 0x0002) ? 1 : 0;
		 default_ii.transform_top_as_serifs =
		    (flags & 0x0004) ? 1 : 0;
		 default_ii.transform_diagon_serifs =
		    (flags & 0x0008) ? 1 : 0;

		 default_ii.a_from_d=(flags & 0x0010) ? 1 : 0;
		 default_ii.f_long_tail=(flags & 0x0020) ? 1 : 0;
		 default_ii.f_rotate_top=(flags & 0x0040) ? 1 : 0;
		 default_ii.pq_deserif=(flags & 0x0080) ? 1 : 0;

		 default_ii.cyrl_phi=(flags & 0x0100) ? 1 : 0;
		 default_ii.cyrl_i=(flags & 0x0200) ? 1 : 0;
		 default_ii.cyrl_pi=(flags & 0x0400) ? 1 : 0;
		 default_ii.cyrl_te=(flags & 0x0800) ? 1 : 0;

		 default_ii.cyrl_sha=(flags & 0x1000) ? 1 : 0;
		 default_ii.cyrl_dje=(flags & 0x2000) ? 1 : 0;
		 default_ii.cyrl_dzhe=(flags & 0x4000) ? 1 : 0;
	      }
	      continue;
	   }
	   break;

	case 4:
	   /* Argument 4: serif type: 1=flat, 2=simpleslant, 3=complexslant,
	      others ignored */
	   if (c->a.vals[i].type==v_int) {
	      switch (c->a.vals[i].u.ival) {
		case 1:
		   default_ii.secondary_serif=srf_flat;
		   break;
		case 2:
		   default_ii.secondary_serif=srf_simpleslant;
		   break;
		case 3:
		   default_ii.secondary_serif=srf_complexslant;
		   break;
	      }
	      continue;
	   }
	   break;

	case 5:
	   /* Argument 5: left side bearings change, percentage */
	   if (c->a.vals[i].type==v_real)
	      pct=c->a.vals[i].u.fval;
	   else if (c->a.vals[i].type==v_int)
	      pct=c->a.vals[i].u.ival;
	   else
	      break;
	   default_ii.lc.lsb_percent=default_ii.lc.rsb_percent =
	      default_ii.uc.lsb_percent=default_ii.uc.rsb_percent =
	      default_ii.neither.lsb_percent =
	      default_ii.neither.rsb_percent=pct;
	   continue;

	case 6:
	   /* Argument 6: stem width change, percentage */
	   if (c->a.vals[i].type==v_real)
	      pct=c->a.vals[i].u.fval;
	   else if (c->a.vals[i].type==v_int)
	      pct=c->a.vals[i].u.ival;
	   else
	      break;
	   default_ii.lc.stem_percent =
	      default_ii.uc.stem_percent =
	      default_ii.neither.stem_percent=pct;
	   continue;

	case 7:
	   /* Argument 7: counters change, percentage */
	   if (c->a.vals[i].type==v_real)
	      pct=c->a.vals[i].u.fval;
	   else if (c->a.vals[i].type==v_int)
	      pct=c->a.vals[i].u.ival;
	   else
	      break;
	   default_ii.lc.counter_percent =
	      default_ii.uc.counter_percent =
	      default_ii.neither.counter_percent=pct;
	   continue;

	case 8:
	   /* Argument 8: lowercase stem width change, percentage
	      (defaults to argument 6) */
	   if (c->a.vals[i].type==v_real)
	      pct=c->a.vals[i].u.fval;
	   else if (c->a.vals[i].type==v_int)
	      pct=c->a.vals[i].u.ival;
	   else
	      break;
	   default_ii.lc.stem_percent=pct;
	   continue;

	case 9:
	   /* Argument 9: lowercase counters change, percentage
	      (defaults to argument 7) */
	   if (c->a.vals[i].type==v_real)
	      pct=c->a.vals[i].u.fval;
	   else if (c->a.vals[i].type==v_int)
	      pct=c->a.vals[i].u.ival;
	   else
	      break;
	   default_ii.lc.counter_percent=pct;
	   continue;
      }
      {
	 char errmsg[40];

	 snprintf(errmsg, sizeof(errmsg), "Bad argument %d type in Italic",
		  i);
	 ScriptError(c, errmsg);
      }
   }
   MakeItalic(c->curfv, NULL, &default_ii);
}

static void bJoin(Context *c) {
   FVJoin(c->curfv);
}

static void bLoadEncodingFile(Context *c) {
   char *t;
   char *locfilename;

   if ((c->a.vals[1].type != v_str) ||
	    (c->a.argc >= 3 && c->a.vals[2].type != v_str))
      ScriptError(c, "Bad argument type");

   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   ParseEncodingFile(locfilename,
		     (c->a.argc >= 3 ? c->a.vals[2].u.sval : NULL));
   free(locfilename);
   free(t);
}

static void bLoadFileToString(Context *c) {
   AFILE *f;
   int len;
   char *name, *_name;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type of argument");
   c->return_val.type=v_str;
   _name=script2utf8_copy(c->a.vals[1].u.sval);
   name=fastrdup(_name);
   free(_name);
   f=afopen(name, "rb");
   free(name);

   if (f==NULL)
      c->return_val.u.sval=fastrdup("");
   else {
      afseek(f, 0, SEEK_END);
      len=aftell(f);
      afseek(f,0,SEEK_SET);
      c->return_val.u.sval=malloc(len + 1);
      len=afread(c->return_val.u.sval, 1, len, f);
      c->return_val.u.sval[len]='\0';
      afclose(f);
   }
}

static void bLoadNamelist(Context *c) {
   char *name, *_name;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type of argument");
   _name=script2utf8_copy(c->a.vals[1].u.sval);
   name=fastrdup(_name);
   free(_name);
   LoadNamelist(name);
   free(name);
}

static void bLoadNamelistDir(Context *c) {
   char *dir=NULL, *_dir;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_str)
	 ScriptError(c, "Bad type of argument");
      _dir=script2utf8_copy(c->a.vals[1].u.sval);
      dir=fastrdup(_dir);
      free(_dir);
   }
   LoadNamelistDir(dir);
   free(dir);
}

static char **GetFontNames(char *filename) {
   AFILE *foo;
   char **ret=NULL;

   if (GFileIsDir(filename)) {
      char *temp =
	 malloc(strlen(filename) + strlen("/glyphs/contents.plist") + 1);
      strcpy(temp, filename);
      strcat(temp, "/glyphs/contents.plist");
      if (GFileExists(temp))
	 ret=NamesReadUFO(filename);
      else {
	 strcpy(temp, filename);
	 strcat(temp, "/font.props");
	 if (GFileExists(temp))
	    ret=NamesReadSFD(temp);
	 /* The fonts.prop file will look just like an sfd file as far */
	 /* as fontnames are concerned, we don't need a separate routine */
      }
      free(temp);
   } else {
      foo=afopen(filename, "rb");
      if (foo != NULL) {
	 /* Try to guess the file type from the first few characters... */
	 int ch1=agetc(foo);
	 int ch2=agetc(foo);
	 int ch3=agetc(foo);
	 int ch4=agetc(foo);
	 int ch5, ch6;

	 afseek(foo, 98, SEEK_SET);
	 ch5=agetc(foo);
	 ch6=agetc(foo);
	 afclose(foo);
	 if ((ch1==0 && ch2==1 && ch3==0 && ch4==0) ||
	     (ch1=='O' && ch2=='T' && ch3=='T' && ch4=='O') ||
	     (ch1=='t' && ch2=='r' && ch3=='u' && ch4=='e') ||
	     (ch1=='t' && ch2=='t' && ch3=='c' && ch4=='f')) {
	    ret=NamesReadTTF(filename);
	 } else if ((ch1=='%' && ch2=='!') || (ch1==0x80 && ch2=='\01')) {	/* PFB header */
	    ret=NamesReadPostScript(filename);
	 } else if (ch1=='%' && ch2=='P' && ch3=='D' && ch4=='F') {
	    ret=NamesReadPDF(filename);
	 } else if (ch1=='<' && ch2=='?' && (ch3=='x' || ch3=='X')
		    && (ch4=='m' || ch4=='M')) {
	    ret=NamesReadSVG(filename);
	 } else if (ch1=='S' && ch2=='p' && ch3=='l' && ch4=='i') {
	    ret=NamesReadSFD(filename);
	 } else if (ch1==1 && ch2==0 && ch3==4) {
	    ret=NamesReadCFF(filename);
	 } else			/* Too hard to figure out a valid mark for a mac resource file */
	    ret=NamesReadMacBinary(filename);
      }
   }
   return (ret);
}

static void bLoadTableFromFile(Context *c) {
   SplineFont *sf=c->curfv->sf;
   uint32_t tag;
   char *tstr, *end;
   struct ttf_table *tab;
   AFILE *file;
   int len;
   char *t;
   char *locfilename;

   if (c->a.vals[1].type != v_str && c->a.vals[2].type != v_str)
      ScriptError(c, "Bad argument type");

   tstr=c->a.vals[1].u.sval;
   end=tstr + strlen(tstr);
   if (*tstr=='\0' || end - tstr > 4)
      ScriptError(c, "Bad tag");
   tag=*tstr << 24;
   tag |= (tstr + 1 < end ? tstr[1] : ' ') << 16;
   tag |= (tstr + 2 < end ? tstr[2] : ' ') << 8;
   tag |= (tstr + 3 < end ? tstr[3] : ' ');

   t=script2utf8_copy(c->a.vals[2].u.sval);
   locfilename=fastrdup(t);
   file=afopen(locfilename, "rb");
   free(locfilename);
   free(t);
   if (file==NULL)
      ScriptErrorString(c, "Could not open file: ", c->a.vals[2].u.sval);
   len=afilesize(file);
   if (len<0)
     ScriptErrorString(c, "afilesize () failed on ",c->a.vals[2].u.sval);

   for (tab=sf->ttf_tab_saved; tab != NULL && tab->tag != tag;
	tab=tab->next);
   if (tab==NULL) {
      tab=chunkalloc(sizeof(struct ttf_table));
      tab->tag=tag;
      tab->next=sf->ttf_tab_saved;
      sf->ttf_tab_saved=tab;
   } else
      free(tab->data);
   tab->len=len;
   tab->data=malloc(len);
   afread(tab->data, 1, len, file);
   afclose(file);
}

static void bLog(Context *c) {
   double val;

   if (c->a.vals[1].type==v_real)
      val=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
      val=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_real;
   c->return_val.u.fval=log(val);
}

static void bLookupStoreLigatureInAfm(Context *c) {
   OTLookup *otl;

if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_int)
      ScriptError(c, "Bad type for argument");

   otl=SFFindLookup(c->curfv->sf, c->a.vals[1].u.sval);
   if (otl==NULL)
      ScriptErrorString(c, "Missing lookup", c->a.vals[1].u.sval);
   otl->store_in_afm=c->a.vals[2].u.ival;
}

static void bMMAxisBounds(Context *c) {
   int i, axis;
   MMSet *mm=c->curfv->sf->mm;

if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad type of argument");
   else if (mm==NULL)
      ScriptError(c, "Not a multiple master font");
   else if (c->a.vals[1].u.ival < 0 || c->a.vals[1].u.ival >= mm->axis_count)
      ScriptError(c, "Axis out of range");
   axis=c->a.vals[1].u.ival;

   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=mm->axis_count;
   c->return_val.u.aval->vals=malloc(3 * sizeof(Val));
   for (i=0; i < 3; ++i)
      c->return_val.u.aval->vals[i].type=v_int;
   c->return_val.u.aval->vals[0].u.ival=mm->axismaps[axis].min * 65536;
   c->return_val.u.aval->vals[1].u.ival=mm->axismaps[axis].def * 65536;
   c->return_val.u.aval->vals[2].u.ival=mm->axismaps[axis].max * 65536;
}

static void bMMAxisNames(Context *c) {
   int i;
   MMSet *mm=c->curfv->sf->mm;

if (mm==NULL)
      ScriptError(c, "Not a multiple master font");

   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=mm->axis_count;
   c->return_val.u.aval->vals=malloc(mm->axis_count * sizeof(Val));
   for (i=0; i < mm->axis_count; ++i) {
      c->return_val.u.aval->vals[i].type=v_str;
      c->return_val.u.aval->vals[i].u.sval=fastrdup(mm->axes[i]);
   }
}

static void bMMBlendToNewFont(Context *c) {
   Reblend(c, true);
}

static void bMMChangeInstance(Context *c) {
   int i;
   MMSet *mm=c->curfv->sf->mm;

if (mm==NULL)
      ScriptError(c, "Not a multiple master font");
   else if (c->a.vals[1].type==v_int) {
      if (c->a.vals[1].u.ival==-1)
	 c->curfv->sf=mm->normal;
      else if (c->a.vals[1].u.ival < mm->instance_count)
	 c->curfv->sf=mm->instances[c->a.vals[1].u.ival];
      else
	 ScriptError(c, "Mutilple Master instance index out of bounds");
   } else if (c->a.vals[1].type==v_str) {
      if (strcmp(mm->normal->fontname, c->a.vals[1].u.sval)==0)
	 c->curfv->sf=mm->normal;
      else {
	 for (i=0; i < mm->instance_count; ++i)
	    if (strcmp(mm->instances[i]->fontname, c->a.vals[1].u.sval)==0) {
	       c->curfv->sf=mm->instances[i];
	       break;
	    }
	 if (i==mm->instance_count)
	    ScriptErrorString(c, "No instance named", c->a.vals[1].u.sval);
      }
   } else
      ScriptError(c, "Bad argument");
}

static void bMMChangeWeight(Context *c) {
   Reblend(c, false);
}

static void bMMInstanceNames(Context *c) {
   int i;
   MMSet *mm=c->curfv->sf->mm;

if (mm==NULL)
      ScriptError(c, "Not a multiple master font");

   c->return_val.type=v_arrfree;
   c->return_val.u.aval=malloc(sizeof(Array));
   c->return_val.u.aval->argc=mm->instance_count;
   c->return_val.u.aval->vals=malloc(mm->instance_count * sizeof(Val));
   for (i=0; i < mm->instance_count; ++i) {
      c->return_val.u.aval->vals[i].type=v_str;
      c->return_val.u.aval->vals[i].u.sval=fastrdup(mm->instances[i]->fontname);
   }
}

static void bMMWeightedName(Context *c) {
   MMSet *mm=c->curfv->sf->mm;

if (mm==NULL)
      ScriptError(c, "Not a multiple master font");

   c->return_val.type=v_str;
   c->return_val.u.sval=fastrdup(mm->normal->fontname);
}

static void bMakeLine(Context *c) {
   int i, gid;
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;

   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i]) {
	 SplineChar *sc=sf->glyphs[gid];

	 SCMakeLine(sc);
      }
}

static void bMergeFonts(Context *c) {
   SplineFont *sf;
   int openflags=0;
   char *t;
   char *locfilename;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "MergeFonts expects a filename");
   else if (c->a.argc==3) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "MergeFonts expects an integer for second argument");
      openflags=c->a.vals[2].u.ival;
   }
   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   sf=LoadSplineFont(locfilename, openflags);
   free(t);
   free(locfilename);
   if (sf==NULL)
      ScriptErrorString(c, "Can't find font", c->a.vals[1].u.sval);
   if (sf->fv==NULL)
      EncMapFree(sf->map);
   MergeFont(c->curfv, sf, 0);
}

static void bMergeKern(Context *c) {
   char *t;
   char *locfilename;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type of arguments");

   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   if (!LoadKerningDataFromMetricsFile
       (c->curfv->sf, locfilename, c->curfv->map))
      ScriptError(c, "Failed to find kern info in file");
   free(locfilename);
   free(t);
}

static void bMergeLookupSubtables(Context *c) {
   struct lookup_subtable *sub1, *sub2;

if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str)
      ScriptError(c, "Bad type for argument");
   sub1=SFFindLookupSubtable(c->curfv->sf, c->a.vals[1].u.sval);
   if (sub1==NULL)
      ScriptErrorString(c, "Unknown subtable", c->a.vals[1].u.sval);
   sub2=SFFindLookupSubtable(c->curfv->sf, c->a.vals[2].u.sval);
   if (sub2==NULL)
      ScriptErrorString(c, "Unknown subtable", c->a.vals[2].u.sval);
   if (sub1->lookup != sub2->lookup)
      ScriptError(c,
		  "When merging two lookup subtables they must be in the same lookup.");
   SFSubTablesMerge(c->curfv->sf, sub1, sub2);
   SFRemoveLookupSubTable(c->curfv->sf, sub2, 0);
}

static void bMergeLookups(Context *c) {
   OTLookup *otl1, *otl2;
   struct lookup_subtable *sub;

if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str)
      ScriptError(c, "Bad type for argument");
   otl1=SFFindLookup(c->curfv->sf, c->a.vals[1].u.sval);
   if (otl1==NULL)
      ScriptErrorString(c, "Unknown lookup", c->a.vals[1].u.sval);
   otl2=SFFindLookup(c->curfv->sf, c->a.vals[2].u.sval);
   if (otl2==NULL)
      ScriptErrorString(c, "Unknown lookup", c->a.vals[2].u.sval);
   if (otl1->lookup_type != otl2->lookup_type)
      ScriptError(c,
		  "When merging two lookups they must be of the same type");
   FLMerge(otl1, otl2);

   for (sub=otl2->subtables; sub != NULL; sub=sub->next)
      sub->lookup=otl1;
   if (otl1->subtables==NULL)
      otl1->subtables=otl2->subtables;
   else {
      for (sub=otl1->subtables; sub->next != NULL; sub=sub->next);
      sub->next=otl2->subtables;
   }
   otl2->subtables=NULL;
   SFRemoveLookup(c->curfv->sf, otl2, 0);
}

static void bMove(Context *c) {
   real trans[6];
   int otype=1;
   BVTFunc bvts[2];

   trans[0]=trans[3]=1;
   trans[1]=trans[2]=0;
   if (c->a.vals[1].type==v_int)
      trans[4]=c->a.vals[1].u.ival;
   else if (c->a.vals[1].type==v_real)
      trans[4]=c->a.vals[1].u.fval;
   else
      ScriptError(c, "Bad argument type");
   if (c->a.vals[2].type==v_int)
      trans[5]=c->a.vals[2].u.ival;
   else if (c->a.vals[2].type==v_real)
      trans[5]=c->a.vals[2].u.fval;
   else
      ScriptError(c, "Bad argument type");

   bvts[0].func=bvt_transmove;
   bvts[0].x=trans[4];
   bvts[0].y=trans[5];
   bvts[1].func=bvt_none;
   FVTransFunc(c->curfv, trans, otype, bvts, true);
}

static void bMoveReference(Context *c) {
   _bMoveReference(c, false);
}

static void bMultipleEncodingsToReferences(Context *c) {
   int i, gid;
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;
   SplineChar *sc, *orig;
   struct altuni *alt, *next, *prev;
   int uni, enc;

   for (i=0; i < map->enccount; ++i) {
      if ((gid=map->map[i]) != -1 && fv->selected[i] &&
	  (orig=sf->glyphs[gid]) != NULL && orig->altuni != NULL) {
	 prev=NULL;
	 for (alt=orig->altuni; alt != NULL; alt=next) {
	    if (alt->vs==-1) {
	       next=alt->next;
	       uni=alt->unienc;
	       orig->altuni=next;
	       AltUniFree(alt);
	       if (prev==NULL)
		  orig->altuni=next;
	       else
		  prev->next=next;
	       enc=EncFromUni(uni, map->enc);
	       if (enc != -1) {
		  map->map[enc]=-1;
		  sc=SFMakeChar(sf, map, enc);
		  SCAddRef(sc, orig, ly_fore, 0, 0);
	       }
	    } else
	       prev=alt;
	 }
      }
   }
   /* Now suppose we had a duplicate encoding of a glyph with no unicode */
   /*  so there will be no altuni to mark it */
   for (gid=0; gid < sf->glyphcnt; ++gid) {
      for (i=0; i < map->enccount; ++i) {
	 if (map->map[i]==gid && map->backmap[gid] != i &&
	     (fv->selected[i]
	      || (map->backmap[gid] != -1
		  && fv->selected[map->backmap[gid]]))) {
	    map->map[i]=-1;
	    sc=SFMakeChar(sf, map, i);
	    SCAddRef(sc, orig, ly_fore, 0, 0);
	    sc->width=orig->width;
	    sc->vwidth=orig->vwidth;
	 }
      }
   }
}

static int bDoSelect(Context *c,int signal_error,int select,int by_ranges) {
   int top, bottom, i, j;
   int any=false;

   if (c->a.argc==2
       && (c->a.vals[1].type==v_arr || c->a.vals[1].type==v_arrfree)) {
      struct array *arr=c->a.vals[1].u.aval;

      for (i=0; i < arr->argc && i < c->curfv->map->enccount; ++i) {
	 if (arr->vals[i].type != v_int) {
	    if (signal_error)
	       ScriptError(c, "Bad type within selection array");
	    else
	       return (any ? -1 : -2);
	 } else {
	    c->curfv->selected[i]=(arr->vals[i].u.ival != 0);
	    ++any;
	 }
      }
      return (any);
   }

   for (i=1; i < c->a.argc; i += 1 + by_ranges) {
      bottom=ParseCharIdent(c, &c->a.vals[i], signal_error);
      if (i + 1==c->a.argc || !by_ranges)
	 top=bottom;
      else {
	 top=ParseCharIdent(c, &c->a.vals[i + 1], signal_error);
      }
      if (bottom==-1 || top==-1)	/* an error occurred in parsing */
	 return (any ? -1 : -2);

      if (top < bottom) {
	 j=top;
	 top=bottom;
	 bottom=j;
      }
      for (j=bottom; j <= top; ++j) {
	 c->curfv->selected[j]=select;
	 ++any;
      }
   }
   return (any);
}

static void bNameFromUnicode(Context *c) {
   char buffer[400];
   int uniinterp;
   NameList *for_new_glyphs;

   if (c->a.vals[1].type != v_int && c->a.vals[1].type != v_unicode)
     ScriptError(c, "Bad type for argument");
   else if (c->a.argc==3 && c->a.vals[2].type != v_str)
      ScriptError(c, "Bad type for argument");

   if (c->a.argc==3) {
      uniinterp=ui_none;
      for_new_glyphs=NameListByName(c->a.vals[2].u.sval);
      if (for_new_glyphs==NULL)
	 ScriptErrorString(c, "Could not find namelist", c->a.vals[2].u.sval);
   } else if (c->curfv==NULL) {
      uniinterp=ui_none;
      for_new_glyphs=NameListByName("AGL with PUA");
   } else {
      uniinterp=c->curfv->sf->uni_interp;
      for_new_glyphs=c->curfv->sf->for_new_glyphs;
   }

   c->return_val.type=v_str;
   c->return_val.u.sval =
      fastrdup(StdGlyphName
	   (buffer, c->a.vals[1].u.ival, uniinterp, for_new_glyphs));
}

static void bNearlyHvCps(Context *c) {
   FontViewBase *fv=c->curfv;
   int i, layer, last;
   SplineSet *spl;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;
   real err=.1;
   int gid;

   if (c->a.argc > 1) {
      if (c->a.vals[1].type==v_int)
	 err=c->a.vals[1].u.ival;
      else if (c->a.vals[1].type==v_real)
	 err=c->a.vals[1].u.fval;
      else
	 ScriptError(c, "Bad type for argument");
      if (c->a.argc > 2) {
	 if (c->a.vals[2].type != v_int)
	    ScriptError(c, "Bad type for argument");
	 err /= (real) c->a.vals[2].u.ival;
      }
   }
   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i]) {
	 SplineChar *sc=sf->glyphs[gid];

	 SCPreserveState(sc, false);
	 last=ly_fore;
	 if (sc->parent->multilayer)
	    last=sc->layer_cnt - 1;
	 for (layer=ly_fore; layer <= last; ++layer) {
	    for (spl=sc->layers[layer].splines; spl != NULL;
		 spl=spl->next)
	       SPLNearlyHvCps(sc, spl, err);
	 }
      }
}

static void bNearlyHvLines(Context *c) {
   FontViewBase *fv=c->curfv;
   int i, layer, last, gid;
   SplineSet *spl;
   SplineFont *sf=fv->sf;
   real err=.1;

   if (c->a.argc > 1) {
      if (c->a.vals[1].type==v_int)
	 err=c->a.vals[1].u.ival;
      else if (c->a.vals[1].type==v_real)
	 err=c->a.vals[1].u.fval;
      else
	 ScriptError(c, "Bad type for argument");
      if (c->a.argc > 2) {
	 if (c->a.vals[2].type != v_int)
	    ScriptError(c, "Bad type for argument");
	 err /= (real) c->a.vals[2].u.ival;
      }
   }
   for (i=0; i < c->curfv->map->enccount; ++i)
      if ((gid=c->curfv->map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i]) {
	 SplineChar *sc=sf->glyphs[gid];

	 SCPreserveState(sc, false);
	 last=ly_fore;
	 if (sc->parent->multilayer)
	    last=sc->layer_cnt - 1;
	 for (layer=ly_fore; layer <= last; ++layer) {
	    for (spl=sc->layers[layer].splines; spl != NULL;
		 spl=spl->next)
	       SPLNearlyHvLines(sc, spl, err);
	 }
      }
}

static void bNearlyLines(Context *c) {
   FontViewBase *fv=c->curfv;
   int i, layer, last, gid;
   SplineSet *spl;
   SplineFont *sf=fv->sf;
   real err=1;

   if (c->a.argc > 1) {
      if (c->a.vals[1].type==v_int)
	 err=c->a.vals[1].u.ival;
      else if (c->a.vals[1].type==v_real)
	 err=c->a.vals[1].u.fval;
      else
	 ScriptError(c, "Bad type for argument");
   }
   for (i=0; i < c->curfv->map->enccount; ++i)
      if ((gid=c->curfv->map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i]) {
	 SplineChar *sc=sf->glyphs[gid];
	 int changed=false;

	 SCPreserveState(sc, false);
	 last=ly_fore;
	 if (sc->parent->multilayer)
	    last=sc->layer_cnt - 1;
	 for (layer=ly_fore; layer <= last; ++layer) {
	    for (spl=sc->layers[layer].splines; spl != NULL;
		 spl=spl->next)
	       changed |= SPLNearlyLines(sc, spl, err);
	 }
	 if (changed)
	    SCCharChangedUpdate(sc, ly_fore, true);
      }
}

static void bNew(Context *c) {
   c->curfv=FVAppend(_FontViewCreate(SplineFontNew()));
}

static void bNonLinearTransform(Context *c) {
   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str)
      ScriptError(c, "Bad argument type");
   if (c->curfv->sf->layers[ly_fore].order2)
      ScriptError(c, "Can only be applied to cubic (PostScript) fonts");
   if (!SFNLTrans(c->curfv, c->a.vals[1].u.sval, c->a.vals[2].u.sval))
      ScriptError(c, "Bad expression");
}

static void bOpen(Context *c) {
   SplineFont *sf;
   int openflags=0;
   char *t;
   char *locfilename;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Open expects a filename");
   else if (c->a.argc==3) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Open expects an integer for second argument");
      openflags=c->a.vals[2].u.ival;
   }
   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   sf=LoadSplineFont(locfilename, openflags);
   free(t);
   free(locfilename);
   if (sf==NULL)
      ScriptErrorString(c, "Failed to open", c->a.vals[1].u.sval);
   else {
      if (sf->fv != NULL)
	 /* All done */ ;
      else
	 FVAppend(_FontViewCreate(sf));
      c->curfv=sf->fv;
   }
}

static void bOrd(Context *c) {
   if (c->a.vals[1].type != v_str
	    || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad type for argument");
   if (c->a.argc==3) {
      if (c->a.vals[2].u.ival < 0
	  || c->a.vals[2].u.ival > strlen(c->a.vals[1].u.sval))
	 ScriptError(c, "Bad value for argument");
      c->return_val.type=v_int;
      c->return_val.u.ival=(uint8_t) c->a.vals[1].u.sval[c->a.vals[2].u.ival];
   } else {
      int i, len=strlen(c->a.vals[1].u.sval);

      c->return_val.type=v_arrfree;
      c->return_val.u.aval=malloc(sizeof(Array));
      c->return_val.u.aval->argc=len;
      c->return_val.u.aval->vals=malloc(len * sizeof(Val));
      for (i=0; i < len; ++i) {
	 c->return_val.u.aval->vals[i].type=v_int;
	 c->return_val.u.aval->vals[i].u.ival =
	    (uint8_t) c->a.vals[1].u.sval[i];
      }
   }
}

static void bOutline(Context *c) {
   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   FVOutline(c->curfv, c->a.vals[1].u.ival);
}

static void bOverlapIntersect(Context *c) {
   FVOverlap(c->curfv, over_intersect);
}

static void bPaste(Context *c) {
   PasteIntoFV(c->curfv, false, NULL);
}

static void bPasteInto(Context *c) {
   PasteIntoFV(c->curfv, true, NULL);
}

static void bPasteWithOffset(Context *c) {
   real trans[6];

   memset(trans, 0, sizeof(trans));
   trans[0]=trans[3]=1;
   if (c->a.vals[1].type==v_int)
      trans[4]=c->a.vals[1].u.ival;
   else if (c->a.vals[1].type==v_real)
      trans[4]=c->a.vals[1].u.fval;
   else
      ScriptError(c, "Bad type for argument");
   if (c->a.vals[2].type==v_int)
      trans[5]=c->a.vals[2].u.ival;
   else if (c->a.vals[2].type==v_real)
      trans[5]=c->a.vals[2].u.fval;
   else
      ScriptError(c, "Bad type for argument");
   PasteIntoFV(c->curfv, 3, trans);
}

static void bPositionReference(Context *c) {
   _bMoveReference(c, true);
}

static void bPostNotice(Context *c) {
   char *t1;
   char *loc;

   if (c->a.vals[1].type != v_str)
     ScriptError(c, "Expected string argument");

   loc=c->a.vals[1].u.sval;
   t1=script2utf8_copy(loc);
   loc=fastrdup(t1);
   ErrorMsg(1,"%s\n", loc);
   free(loc);
   free(t1);
}

static void bPow(Context *c) {
   double val1, val2;

   if (c->a.vals[1].type==v_real)
      val1=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
      val1=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   if (c->a.vals[2].type==v_real)
      val2=c->a.vals[2].u.fval;
   else if (c->a.vals[2].type==v_int)
      val2=c->a.vals[2].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_real;
   c->return_val.u.fval=pow(val1, val2);
}

static void bPreloadCidmap(Context *c) {

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str
       || c->a.vals[3].type != v_str || c->a.vals[4].type != v_int)
      ScriptError(c, "Bad argument type");
   LoadMapFromFile(c->a.vals[1].u.sval, c->a.vals[2].u.sval,
		   c->a.vals[3].u.sval, c->a.vals[4].u.ival);
}

static void bPrint(Context *c) {
   int i;

   for (i=1; i < c->a.argc; ++i)
      PrintVal(&c->a.vals[i]);
   printf("\n");
   fflush(stdout);
}
/****************************************************************************/

static void bPrintFont(Context *c) {
   ScriptError(c,"Printing fonts is currently unimplemented.");
}

static void bPrintSetup(Context *c) {
   ScriptError(c,"Printing fonts is currently unimplemented.");
}

static void bPrivateGuess(Context *c) {
   SplineFont *sf=c->curfv->sf;
   char *key;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");

   key=forceASCIIcopy(c, c->a.vals[1].u.sval);
   if (sf->private==NULL) {
      sf->private=calloc(1, sizeof(struct psdict));
   }
   SFPrivateGuess(sf, c->curfv->active_layer, sf->private, key, true);
   free(key);
}

static void bQuit(Context *c) {
   if (verbose > 0)
      putchar('\n');
   if (c->a.argc==1)
      exit(0);
   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Expected integer argument");
   else
      exit(c->a.vals[1].u.ival);
   exit(1);
}

static void bRand(Context *c) {
   c->return_val.type=v_int;
   c->return_val.u.ival=sfmt_genrand_uint32(&fa_sfmt)&0x7FFFFFFF;
}

static void bRandReal(Context *c) {
   c->return_val.type=v_real;
   c->return_val.u.fval=sfmt_to_real2(sfmt_genrand_uint32(&fa_sfmt));
}

static void bReadOtherSubrsFile(Context *c) {
   if (c->a.vals[1].type != v_str)
     ScriptError(c, "Bad type for argument");
   if (ReadOtherSubrsFile(c->a.vals[1].u.sval) <= 0)
      ScriptErrorString(c, "Failed to read OtherSubrs from %s",
			c->a.vals[1].u.sval);
}

static void bReal(Context *c) {
   if (c->a.vals[1].type != v_int && c->a.vals[1].type != v_unicode)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_real;
   c->return_val.u.fval=(float) c->a.vals[1].u.ival;
}

static void bReencode(Context *c) {
   Encoding *new_enc;
   int force=0;

if (c->a.vals[1].type != v_str
	    || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad argument type");
   if (c->a.argc==3)
      force=c->a.vals[2].u.ival;
   if (strmatch(c->a.vals[1].u.sval, "compacted")==0) {
      c->curfv->normal=EncMapCopy(c->curfv->map);
      CompactEncMap(c->curfv->map, c->curfv->sf);
   } else {
      new_enc=FindOrMakeEncoding(c->a.vals[1].u.sval);
      if (new_enc==NULL)
	 ScriptErrorString(c, "Unknown encoding", c->a.vals[1].u.sval);
      if (force)
	 SFForceEncoding(c->curfv->sf, c->curfv->map, new_enc);
      else if (new_enc==&custom)
	 c->curfv->map->enc=&custom;
      else {
	 EncMap *map=EncMapFromEncoding(c->curfv->sf, new_enc);

	 EncMapFree(c->curfv->map);
	 c->curfv->map=map;
      }
      if (c->curfv->normal != NULL) {
	 EncMapFree(c->curfv->normal);
	 c->curfv->normal=NULL;
      }
      SFReplaceEncodingBDFProps(c->curfv->sf, c->curfv->map);
   }
   free(c->curfv->selected);
   c->curfv->selected=calloc(c->curfv->map->enccount, sizeof(char));
/*
    c->curfv->sf->changed=true;
    c->curfv->sf->changed_since_xuidchanged=true;
*/
}

static void bRemoveAnchorClass(Context *c) {
   AnchorClass *t;
   SplineFont *sf=c->curfv->sf;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");

   for (t=sf->anchor; t != NULL; t=t->next)
      if (strcmp(c->a.vals[1].u.sval, t->name)==0)
	 break;
   if (t==NULL)
      ScriptErrorString(c,
			"This font does not contain an anchor class with this name: ",
			c->a.vals[1].u.sval);
   SFRemoveAnchorClass(sf, t);
}

static void bRemoveDetachedGlyphs(Context *c) {
   FontViewBase *fv=c->curfv;
   int i, gid;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   SplineChar *sc;
   int flags=-1;
   int changed=false;

   for (gid=0; gid < sf->glyphcnt; ++gid)
      if (sf->glyphs[gid] != NULL)
	 sf->glyphs[gid]->ticked=false;

   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1)
	 sf->glyphs[gid]->ticked=true;

   for (gid=0; gid < sf->glyphcnt; ++gid)
      if ((sc=sf->glyphs[gid]) != NULL && !sc->ticked) {
	 SFRemoveGlyph(sf, sc, &flags);
	 changed=true;
      }
   if (changed && !sf->changed) {
      fv->sf->changed=true;
   }
}

static void bRemoveLookup(Context *c) {
   OTLookup *otl;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument 1");
   else if (c->a.argc==3 && c->a.vals[2].type != v_int)
      ScriptError(c, "Bad type for argument 2");
   otl=SFFindLookup(c->curfv->sf, c->a.vals[1].u.sval);
   if (otl==NULL)
      ScriptErrorString(c, "Unknown lookup", c->a.vals[1].u.sval);
   SFRemoveLookup(c->curfv->sf, otl,
		  c->a.argc==3 ? c->a.vals[2].u.ival : 1);
}

static void bRemoveLookupSubtable(Context *c) {
   struct lookup_subtable *sub;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");
   else if (c->a.argc==3 && c->a.vals[2].type != v_int)
      ScriptError(c, "Bad type for argument 2");
   sub=SFFindLookupSubtable(c->curfv->sf, c->a.vals[1].u.sval);
   if (sub==NULL)
      ScriptErrorString(c, "Unknown lookup subtable", c->a.vals[1].u.sval);
   SFRemoveLookupSubTable(c->curfv->sf, sub,
			  c->a.argc==3 ? c->a.vals[2].u.ival : 1);
}

static void bRemoveOverlap(Context *c) {
   FVOverlap(c->curfv, over_remove);
}

static void bRemovePosSub(Context *c) {
   SplineFont *sf=c->curfv->sf, *sf_sl=sf;
   EncMap *map=c->curfv->map;
   SplineChar *sc;
   int i, gid, is_v;
   PST *pst, *next, *prev;
   KernPair *kp, *kpnext, *kpprev;
   struct lookup_subtable *sub;

   if (sf_sl->cidmaster != NULL)
      sf_sl=sf_sl->cidmaster;
   else if (sf_sl->mm != NULL)
      sf_sl=sf_sl->mm->normal;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   if (*c->a.vals[1].u.sval=='*')
      sub=NULL;
   else {
      sub=SFFindLookupSubtable(sf, c->a.vals[1].u.sval);
      if (sub==NULL)
	 ScriptErrorString(c, "Unknown lookup subtable", c->a.vals[1].u.sval);
   }

   for (i=0; i < c->curfv->map->enccount; ++i) {
      if (c->curfv->selected[i] && (gid=map->map[i]) != -1 &&
	  SCWorthOutputting(sc=sf->glyphs[gid])) {
	 for (prev=NULL, pst=sc->possub; pst != NULL; pst=next) {
	    next=pst->next;
	    if (pst->type==pst_lcaret) {
	       prev=pst;
	       continue;
	    }
	    if (pst->subtable==sub || sub==NULL) {
	       if (prev==NULL)
		  sc->possub=next;
	       else
		  prev->next=next;
	       pst->next=NULL;
	       PSTFree(pst);
	    } else
	       prev=pst;
	 }
	 for (is_v=0; is_v < 2; ++is_v) {
	    for (kpprev=NULL, kp=is_v ? sc->vkerns : sc->kerns;
		 kp != NULL; kp=kpnext) {
	       kpnext=kp->next;
	       if (kp->subtable==sub || sub==NULL) {
		  if (kpprev != NULL)
		     kpprev->next=kpnext;
		  else if (is_v)
		     sc->vkerns=kpnext;
		  else
		     sc->kerns=kpnext;
		  kp->next=NULL;
		  KernPairsFree(kp);
	       } else
		  kpprev=kp;
	    }
	 }
      }
   }
}

static void bRemovePreservedTable(Context *c) {
   SplineFont *sf=c->curfv->sf;
   uint32_t tag;
   char *tstr, *end;
   struct ttf_table *tab, *prev;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");

   tstr=c->a.vals[1].u.sval;
   end=tstr + strlen(tstr);
   if (*tstr=='\0' || end - tstr > 4)
      ScriptError(c, "Bad tag");
   tag=*tstr << 24;
   tag |= (tstr + 1 < end ? tstr[1] : ' ') << 16;
   tag |= (tstr + 2 < end ? tstr[2] : ' ') << 8;
   tag |= (tstr + 3 < end ? tstr[3] : ' ');

   for (tab=sf->ttf_tab_saved, prev=NULL; tab != NULL && tab->tag != tag;
	prev=tab, tab=tab->next);
   if (tab==NULL)
      ScriptErrorString(c, "No preserved table matches tag: ", tstr);
   if (prev==NULL)
      sf->ttf_tab_saved=tab->next;
   else
      prev->next=tab->next;
   free(tab->data);
   chunkfree(tab, sizeof(*tab));
}

static void bRenameGlyphs(Context *c) {
   NameList *nl;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   nl=NameListByName(c->a.vals[1].u.sval);
   if (nl==NULL)
      ScriptErrorString(c, "Unknown namelist", c->a.vals[1].u.sval);
   SFRenameGlyphsToNamelist(c->curfv->sf, nl);
}

static void bReplaceCharCounterMasks(Context *c) {
   HintMask *cm;
   SplineChar *sc;
   int i, j, cnt;
   Array *arr;

if (c->a.vals[1].type != v_arr)
      ScriptError(c, "Bad argument type");
   arr=c->a.vals[1].u.aval;
   cnt=arr->argc;
   cm=calloc(cnt, sizeof(HintMask));
   for (i=0; i < cnt; ++i) {
      if (arr->vals[i].type != v_arr || arr->vals[i].u.aval->argc > 12)
	 ScriptError(c, "Argument must be array of array[12] of integers");
      for (j=0; j < arr->vals[i].u.aval->argc; ++j) {
	 if (arr->vals[i].u.aval->vals[j].type != v_int)
	    ScriptError(c, "Argument must be array of array[12] of integers");
	 cm[i][j]=arr->vals[i].u.aval->vals[j].u.ival & 0xff;
      }
   }

   sc=GetOneSelChar(c);
   free(sc->countermasks);
   sc->countermask_cnt=cnt;
   sc->countermasks=cm;
}

static void bReplaceCvtAt(Context *c) {
   SplineFont *sf=c->curfv->sf;
   struct ttf_table *tab;

   if (c->a.vals[1].type != v_int || c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");
   for (tab=sf->ttf_tables;
	tab != NULL && tab->tag != CHR('c', 'v', 't', ' '); tab=tab->next);
   if (tab==NULL || c->a.vals[1].u.ival >= tab->len / 2)
      ScriptError(c, "Cvt table is either not present or too short");
   memputshort(tab->data, sizeof(uint16_t) * c->a.vals[1].u.ival,
	       c->a.vals[2].u.ival);
}

/****************************************************************************/

static void bReplaceOutlineWithReference(Context *c) {
   double fudge=.01;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_real)
	 ScriptError(c, "Bad argument type");
      fudge=c->a.vals[1].u.fval;
   } else if (c->a.argc==3) {
      if (c->a.vals[1].type != v_int || c->a.vals[2].type != v_int ||
	  c->a.vals[2].u.ival==0)
	 ScriptError(c, "Bad argument type");
      fudge=c->a.vals[1].u.ival / (double) c->a.vals[2].u.ival;
   }
   FVBReplaceOutlineWithReference(c->curfv, fudge);
}

static void bRevert(Context *c) {
   FVRevert(c->curfv);
}

static void bRevertToBackup(Context *c) {
   FVRevertBackup(c->curfv);
}

static void bRotate(Context *c) {
   real trans[6];
   int otype=1;
   BVTFunc bvts[2];
   double a, ox, oy;

/* bRotatec->a.argc==1 || c->a.argc==3 || c->a.argc > 4 */
   if ((c->a.vals[1].type != v_int && c->a.vals[1].type != v_real)
       || (c->a.argc==4
	   && ((c->a.vals[2].type != v_int && c->a.vals[2].type != v_real)
	       || (c->a.vals[3].type != v_int
		   && c->a.vals[3].type != v_real))))
      ScriptError(c, "Bad argument type in Rotate");
   if (c->a.vals[1].type==v_int)
      a=c->a.vals[1].u.ival;
   else
      a=c->a.vals[1].u.fval;
   a=fmod(a, 360.0);
   if (a < 0)
      a += 360;
   a *= 3.1415926535897932 / 180.;
   trans[0]=trans[3]=cos(a);
   trans[2]=-(trans[1]=sin(a));
   trans[4]=trans[5]=0;
   if (c->a.argc==4) {
      if (c->a.vals[2].type==v_int)
	 ox=c->a.vals[2].u.ival;
      else
	 ox=c->a.vals[2].u.fval;
      if (c->a.vals[3].type==v_int)
	 oy=c->a.vals[3].u.ival;
      else
	 oy=c->a.vals[3].u.fval;

      trans[4]=ox - (trans[0] * ox + trans[2] * oy);
      trans[5]=oy - (trans[1] * ox + trans[3] * oy);
      otype=0;
   }
   bvts[0].func=bvt_none;
   if (c->a.vals[1].u.ival==90)
      bvts[0].func=bvt_rotate90ccw;
   else if (c->a.vals[1].u.ival==180)
      bvts[0].func=bvt_rotate180;
   else if (c->a.vals[1].u.ival==270)
      bvts[0].func=bvt_rotate90cw;
   bvts[1].func=bvt_none;
   FVTransFunc(c->curfv, trans, otype, bvts, true);
}

static void bRound(Context *c) {
if (c->a.vals[1].type != v_real)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_int;
   c->return_val.u.ival=rint(c->a.vals[1].u.fval);
}

static char *ToString(Val *val) {
   int j;
   char buffer[40];

   if (val->type==v_str) {
      return (fastrdup(val->u.sval));
   } else if (val->type==v_arr || val->type==v_arrfree) {
      char **results, *ret, *pt;
      int len;

      results=malloc(val->u.aval->argc * sizeof(char *));
      len=0;
      for (j=0; j < val->u.aval->argc; ++j) {
	 results[j]=ToString(&val->u.aval->vals[j]);
	 len += strlen(results[j]) + 2;
      }
      ret=pt=malloc(len + 20);
      *pt++='[';
      if (val->u.aval->argc > 0) {
	 strcpy(pt, results[0]);
	 pt += strlen(pt);
	 free(results[0]);
	 for (j=1; j < val->u.aval->argc; ++j) {
	    *pt++=',';
	    if (val->u.aval->vals[j - 1].type==v_arr
		|| val->u.aval->vals[j - 1].type==v_arrfree)
	       *pt++='\n';
	    strcpy(pt, results[j]);
	    pt += strlen(pt);
	    free(results[j]);
	 }
      }
      *pt++=']';
      *pt='\0';
      free(results);
      return (ret);
   } else if (val->type==v_int)
      sprintf(buffer, "%d", val->u.ival);
   else if (val->type==v_unicode)
      sprintf(buffer, "0u%04X", val->u.ival);
   else if (val->type==v_real)
      sprintf(buffer, "%g", (double) val->u.fval);
   else if (val->type==v_void)
      sprintf(buffer, "<void>");
   else
      sprintf(buffer, "<" "???" ">");
   return (fastrdup(buffer));
}

static void bRoundToCluster(Context *c) {
   real within=.1, max=.5;
   int i, gid;
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;

   if (c->a.argc >= 2) {
      if (c->a.vals[1].type==v_int)
	 within=c->a.vals[1].u.ival;
      else if (c->a.vals[1].type==v_real)
	 within=c->a.vals[1].u.fval;
      else
	 ScriptError(c, "Bad type for argument");
      max=4 * within;
      if (c->a.argc >= 3) {
	 if (c->a.vals[2].type==v_int)
	    max=c->a.vals[2].u.ival;
	 else if (c->a.vals[2].type==v_real)
	    max=c->a.vals[2].u.fval;
	 else
	    ScriptError(c, "Bad type for argument");
	 max *= within;
      }
   }
   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i]) {
	 SplineChar *sc=sf->glyphs[gid];

	 SCRoundToCluster(sc, ly_all, false, within, max);
      }
}

static void bRoundToInt(Context *c) {
   real factor=1.0;
   int i, gid;
   FontViewBase *fv=c->curfv;
   SplineFont *sf=fv->sf;
   EncMap *map=fv->map;

   if (c->a.argc==2) {
      if (c->a.vals[1].type==v_int)
	 factor=c->a.vals[1].u.ival;
      else if (c->a.vals[1].type==v_real)
	 factor=c->a.vals[1].u.fval;
      else
	 ScriptError(c, "Bad type for argument");
   }
   for (i=0; i < map->enccount; ++i)
      if ((gid=map->map[i]) != -1 && sf->glyphs[gid] != NULL
	  && fv->selected[i]) {
	 SplineChar *sc=sf->glyphs[gid];

	 SCRound2Int(sc, ly_fore, factor);
      }
}

static void bSameGlyphAs(Context *c) {
   FVSameGlyphAs(c->curfv);
}

static void bSave(Context *c) {
   SplineFont *sf=c->curfv->sf;
   char *t, *pt;
   char *locfilename;
   int localRevisionsToRetain=-1;

   // Grab the optional number of backups that are desired argument
   if (c->a.argc==3) {
	/**
	 * A call Save( wheretosave, revisioncount )
	 */
      if (c->a.vals[2].type != v_int)
	 ScriptError(c,
		     "The second argument to Save() must be a number of revisions to keep (integer)");
      localRevisionsToRetain=c->a.vals[2].u.ival;
   }

   if (c->a.argc >= 2) {
      if (c->a.vals[1].type != v_str)
	 ScriptError(c,
		     "If an argument is given to Save it must be a filename");

      t=script2utf8_copy(c->a.vals[1].u.sval);
      locfilename=fastrdup(t);
      pt=strrchr(locfilename, '.');
      int rc=SFDWriteBakExtended(locfilename,
				   sf, c->curfv->map, c->curfv->normal,
				   localRevisionsToRetain);

      if (!rc)
	 ScriptError(c, "Save failed");

      /* Hmmm.  We don't set the filename. */
      free(t);
      free(locfilename);
   } else {
      if (sf->filename==NULL)
	 ScriptError(c,
		     "This font has no associated sfd file yet, you must specify a filename");

	/**
	 * If there are no existing backup files, don't start creating them here.
	 * Otherwise, save as many as the user wants.
	 */
      int rc=SFDWriteBakExtended(sf->filename,
				   sf, c->curfv->map, c->curfv->normal,
				   localRevisionsToRetain);

      if (!rc)
	 ScriptError(c, "Save failed");
   }
}

static void bSaveTableToFile(Context *c) {
   SplineFont *sf=c->curfv->sf;
   uint32_t tag;
   char *tstr, *end;
   struct ttf_table *tab;
   AFILE *file;
   char *t;
   char *locfilename;

   if (c->a.vals[1].type != v_str && c->a.vals[2].type != v_str)
      ScriptError(c, "Bad argument type");

   tstr=c->a.vals[1].u.sval;
   end=tstr + strlen(tstr);
   if (*tstr=='\0' || end - tstr > 4)
      ScriptError(c, "Bad tag");
   tag=*tstr << 24;
   tag |= (tstr + 1 < end ? tstr[1] : ' ') << 16;
   tag |= (tstr + 2 < end ? tstr[2] : ' ') << 8;
   tag |= (tstr + 3 < end ? tstr[3] : ' ');

   t=script2utf8_copy(c->a.vals[2].u.sval);
   locfilename=fastrdup(t);
   file=afopen(locfilename, "wb");
   free(locfilename);
   free(t);
   if (file==NULL)
      ScriptErrorString(c, "Could not open file: ", c->a.vals[2].u.sval);

   for (tab=sf->ttf_tab_saved; tab != NULL && tab->tag != tag;
	tab=tab->next);
   if (tab==NULL)
      ScriptErrorString(c, "No preserved table matches tag: ", tstr);
   afwrite(tab->data, 1, tab->len, file);
   afclose(file);
}

static void bScale(Context *c) {
   real trans[6];
   int otype=1;
   BVTFunc bvts[2];
   double xfact, yfact;
   int i;
   double args[6];

   /* Arguments:
      1 => same scale factor both directions, origin at center
      2 => different scale factors for each direction, origin at center
      (scale factors are per cents)
      3 => same scale factor both directions, origin last two args
      4 => different scale factors for each direction, origin last two args
    */

   for (i=1; i < c->a.argc; ++i) {
      if (c->a.vals[i].type==v_int)
	 args[i]=c->a.vals[i].u.ival;
      else if (c->a.vals[i].type==v_real)
	 args[i]=c->a.vals[i].u.fval;
      else
	 ScriptError(c, "Bad argument type");
   }
   i=1;
   if (c->a.argc & 1) {
      xfact=args[1] / 100.;
      yfact=args[2] / 100.;
      i=3;
   } else {
      xfact=yfact=args[1] / 100.;
      i=2;
   }
   trans[0]=xfact;
   trans[3]=yfact;
   trans[2]=trans[1]=0;
   trans[4]=trans[5]=0;
   if (c->a.argc > i) {
      trans[4]=args[i] - (trans[0] * args[i]);
      trans[5]=args[i + 1] - (trans[3] * args[i + 1]);
      otype=0;
   }
   bvts[0].func=bvt_none;
   FVTransFunc(c->curfv, trans, otype, bvts, true);
}

static void bScaleToEm(Context *c) {
   int i;
   int ascent, descent;

   for (i=1; i < c->a.argc; ++i)
      if (c->a.vals[i].type != v_int || c->a.vals[i].u.ival < 0
	  || c->a.vals[i].u.ival > 16384)
	 ScriptError(c, "Bad argument type");
   if (c->a.argc==3) {
      ascent=c->a.vals[1].u.ival;
      descent=c->a.vals[2].u.ival;
   } else {
      ascent=rint(c->a.vals[1].u.ival * ((double) c->curfv->sf->ascent) /
		    (c->curfv->sf->ascent + c->curfv->sf->descent));
      descent=c->a.vals[1].u.ival - ascent;
   }
   SFScaleToEm(c->curfv->sf, ascent, descent);
}

static void bSelect(Context *c) {
   memset(c->curfv->selected, 0, c->curfv->map->enccount);
   bDoSelect(c, true, true, true);
}

static void bSelectAll(Context *c) {
   memset(c->curfv->selected, 1, c->curfv->map->enccount);
}

static void bSelectAllInstancesOf(Context *c) {
   int i, j, gid;
   SplineChar *sc;
   FontViewBase *fv=c->curfv;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   struct altuni *alt;

   memset(fv->selected, 0, map->enccount);
   for (i=1; i < c->a.argc; ++i) {
      if (c->a.vals[i].type==v_unicode) {
	 int uni=c->a.vals[i].u.ival;

	 for (j=0; j < map->enccount; ++j)
	    if ((gid=map->map[j]) != -1 && (sc=sf->glyphs[gid]) != NULL) {
	       for (alt=sc->altuni; alt != NULL && alt->unienc != uni;
		    alt=alt->next);
	       if (sc->unicodeenc==uni || alt != NULL)
		  fv->selected[j]=true;
	    }
      } else if (c->a.vals[i].type==v_str) {
	 char *name=c->a.vals[i].u.sval;

	 for (j=0; j < map->enccount; ++j)
	    if ((gid=map->map[j]) != -1 && (sc=sf->glyphs[gid]) != NULL) {
	       if (strcmp(sc->name, name)==0)
		  fv->selected[j]=true;
	    }
      } else
	 ScriptError(c, "Bad type for argument");
   }
}

static void bSelectBitmap(Context *c) {
   BDFFont *bdf;
   int depth, size;

   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad type for argument");
   size=c->a.vals[2].u.ival;
   if (size==-1)
      c->curfv->active_bitmap=NULL;
   else {
      depth=size >> 16;
      if (depth==0)
	 depth=1;
      size=size & 0xffff;
      for (bdf=c->curfv->sf->bitmaps; bdf != NULL; bdf=bdf->next)
	 if (size==bdf->pixelsize && depth==BDFDepth(bdf))
	    break;
      ScriptError(c, "No matching bitmap");
      c->curfv->active_bitmap=bdf;
   }
}

static void bSelectByColor(Context *c) {
   int col, sccol;
   int i, any=0;
   EncMap *map=c->curfv->map;
   SplineFont *sf=c->curfv->sf;

if (c->a.vals[1].type != v_str && c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   if (c->a.vals[1].type==v_int)
      col=c->a.vals[1].u.ival;
   else {
      if (strmatch(c->a.vals[1].u.sval, "Red")==0)
	 col=0xff0000;
      else if (strmatch(c->a.vals[1].u.sval, "Green")==0)
	 col=0x00ff00;
      else if (strmatch(c->a.vals[1].u.sval, "Blue")==0)
	 col=0x0000ff;
      else if (strmatch(c->a.vals[1].u.sval, "Magenta")==0)
	 col=0xff00ff;
      else if (strmatch(c->a.vals[1].u.sval, "Cyan")==0)
	 col=0x00ffff;
      else if (strmatch(c->a.vals[1].u.sval, "Yellow")==0)
	 col=0xffff00;
      else if (strmatch(c->a.vals[1].u.sval, "White")==0)
	 col=0xffffff;
      else if (strmatch(c->a.vals[1].u.sval, "none")==0 ||
	       strmatch(c->a.vals[1].u.sval, "Default")==0)
	 col=COLOR_DEFAULT;
      else
	 ScriptErrorString(c, "Unknown color", c->a.vals[1].u.sval);
   }

   for (i=0; i < map->enccount; ++i) {
      int gid=map->map[i];

      if (gid != -1) {
	 sccol =
	    (sf->glyphs[gid] ==
	     NULL) ? COLOR_DEFAULT : sf->glyphs[gid]->color;
	 if (c->curfv->selected[i] != (sccol==col)) {
	    c->curfv->selected[i]=!c->curfv->selected[i];
	    if (c->curfv->selected[i])
	       any=true;
	 }
      }
   }
   /*c->curfv->sel_index=any; */
}

static void bSelectByPosSub(Context *c) {
   struct lookup_subtable *sub;

if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");
   else if (c->a.vals[2].u.ival < 1 || c->a.vals[2].u.ival > 3)
      ScriptError(c, "Bad argument value");

   sub=SFFindLookupSubtable(c->curfv->sf, c->a.vals[1].u.sval);
   if (sub==NULL)
      ScriptErrorString(c, "Unknown lookup subtable", c->a.vals[1].u.sval);

   c->return_val.type=v_int;
   c->return_val.u.ival =
      FVBParseSelectByPST(c->curfv, sub, c->a.vals[2].u.ival);
}

static void bSelectChanged(Context *c) {
   int i, gid;
   FontViewBase *fv=c->curfv;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   int add=0;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_int)
	 ScriptError(c, "Bad type for argument");
      add=c->a.vals[1].u.ival;
   }

   if (add) {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i] |= ((gid=map->map[i]) != -1
			     && sf->glyphs[gid] != NULL
			     && sf->glyphs[gid]->changed);
   } else {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i]=((gid=map->map[i]) != -1
			    && sf->glyphs[gid] != NULL
			    && sf->glyphs[gid]->changed);
   }
}

static void bSelectFewer(Context *c) {
   bDoSelect(c, true, false, true);
}

static void bSelectFewerSingletons(Context *c) {
   if (c->a.argc==1)
      ScriptError(c, "SelectFewer needs at least one argument");
   bDoSelect(c, true, false, false);
}

static void bSelectGlyphsBoth(Context *c) {
   FontViewBase *fv=c->curfv;
   int i, gid;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   int layer=fv->active_layer;
   int add=0;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_int)
	 ScriptError(c, "Bad type for argument");
      add=c->a.vals[1].u.ival;
   }

   if (add) {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i] |= ((gid=map->map[i]) != -1
			     && sf->glyphs[gid] != NULL
			     && sf->glyphs[gid]->layers[layer].refs != NULL
			     && sf->glyphs[gid]->layers[layer].splines !=
			     NULL);
   } else {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i]=((gid=map->map[i]) != -1
			    && sf->glyphs[gid] != NULL
			    && sf->glyphs[gid]->layers[layer].refs != NULL
			    && sf->glyphs[gid]->layers[layer].splines !=
			    NULL);
   }
}

static void bSelectGlyphsReferences(Context *c) {
   FontViewBase *fv=c->curfv;
   int i, gid;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   int layer=fv->active_layer;
   int add=0;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_int)
	 ScriptError(c, "Bad type for argument");
      add=c->a.vals[1].u.ival;
   }

   if (add) {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i] |= ((gid=map->map[i]) != -1
			     && sf->glyphs[gid] != NULL
			     && sf->glyphs[gid]->layers[layer].refs != NULL);
   } else {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i]=((gid=map->map[i]) != -1
			    && sf->glyphs[gid] != NULL
			    && sf->glyphs[gid]->layers[layer].refs != NULL);
   }
}

static void bSelectGlyphsSplines(Context *c) {
   FontViewBase *fv=c->curfv;
   int i, gid;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   int layer=fv->active_layer;
   int add=0;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_int)
	 ScriptError(c, "Bad type for argument");
      add=c->a.vals[1].u.ival;
   }

   if (add) {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i] |= ((gid=map->map[i]) != -1
			     && sf->glyphs[gid] != NULL
			     && sf->glyphs[gid]->layers[layer].splines !=
			     NULL);
   } else {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i]=((gid=map->map[i]) != -1
			    && sf->glyphs[gid] != NULL
			    && sf->glyphs[gid]->layers[layer].splines !=
			    NULL);
   }
}

static void bSelectHintingNeeded(Context *c) {
   int i, gid;
   FontViewBase *fv=c->curfv;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   int add=0;
   int order2=sf->layers[ly_fore].order2;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_int)
	 ScriptError(c, "Bad type for argument");
      add=c->a.vals[1].u.ival;
   }

   if (add) {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i] |= ((gid=map->map[i]) != -1
			     && sf->glyphs[gid] != NULL
			     &&
			     ((!order2
			       && sf->glyphs[gid]->changedsincelasthinted)
			      || (order2
				  && sf->glyphs[gid]->layers[ly_fore].
				  splines != NULL
				  && sf->glyphs[gid]->ttf_instrs_len <= 0)));
   } else {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i]=((gid=map->map[i]) != -1
			    && sf->glyphs[gid] != NULL
			    &&
			    ((!order2
			      && sf->glyphs[gid]->changedsincelasthinted)
			     || (order2
				 && sf->glyphs[gid]->layers[ly_fore].
				 splines != NULL
				 && sf->glyphs[gid]->ttf_instrs_len <= 0)));
   }
}

static void bSelectIf(Context *c) {
   memset(c->curfv->selected, 0, c->curfv->map->enccount);
   c->return_val.type=v_int;
   c->return_val.u.ival=bDoSelect(c, false, true, true);
}

static void bSelectInvert(Context *c) {
   int i;

   for (i=0; i < c->curfv->map->enccount; ++i)
      c->curfv->selected[i]=!c->curfv->selected[i];
}

static void bSelectMore(Context *c) {
   bDoSelect(c, true, true, true);
}

static void bSelectMoreIf(Context *c) {
   c->return_val.type=v_int;
   c->return_val.u.ival=bDoSelect(c, false, true, true);
}

static void bSelectMoreSingletons(Context *c) {
   bDoSelect(c, true, true, false);
}

static void bSelectMoreSingletonsIf(Context *c) {
   c->return_val.type=v_int;
   c->return_val.u.ival=bDoSelect(c, false, true, false);
}

static void bSelectNone(Context *c) {
   memset(c->curfv->selected, 0, c->curfv->map->enccount);
}

static void bSelectSingletons(Context *c) {
   memset(c->curfv->selected, 0, c->curfv->map->enccount);
   bDoSelect(c, true, true, false);
}

static void bSelectSingletonsIf(Context *c) {
   memset(c->curfv->selected, 0, c->curfv->map->enccount);
   c->return_val.type=v_int;
   c->return_val.u.ival=bDoSelect(c, false, true, false);
}

static void bSelectWorthOutputting(Context *c) {
   int i, gid;
   FontViewBase *fv=c->curfv;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;
   int add=0;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_int)
	 ScriptError(c, "Bad type for argument");
      add=c->a.vals[1].u.ival;
   }

   if (add) {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i] |= ((gid=map->map[i]) != -1
			     && sf->glyphs[gid] != NULL
			     && SCWorthOutputting(sf->glyphs[gid]));
   } else {
      for (i=0; i < map->enccount; ++i)
	 fv->selected[i]=((gid=map->map[i]) != -1
			    && sf->glyphs[gid] != NULL
			    && SCWorthOutputting(sf->glyphs[gid]));
   }
}

static void bSetCharCnt(Context *c) {
   EncMap *map=c->curfv->map;
   int newcnt;

if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   else if (c->a.vals[1].u.ival <= 0 && c->a.vals[1].u.ival > 10 * 65536)
      ScriptError(c, "Argument out of bounds");

   newcnt=c->a.vals[1].u.ival;
   if (map->enccount==newcnt)
      return;
   if (newcnt < map->enc->char_cnt) {
      map->enc=&custom;
   } else {
      c->curfv->selected=realloc(c->curfv->selected, newcnt);
      if (newcnt > map->encmax) {
	 memset(c->curfv->selected + map->enccount, 0,
		newcnt - map->enccount);
	 map->map =
	    realloc(map->map, (map->encmax=newcnt + 10) * sizeof(int32_t));
	 memset(map->map + map->enccount, -1,
		(newcnt - map->enccount) * sizeof(int32_t));
      }
   }
   map->enccount=newcnt;
   c->curfv->sf->changed=true;
   c->curfv->sf->changed_since_xuidchanged=true;
}

static void bSetCharColor(Context *c) {
   SplineFont *sf=c->curfv->sf;
   EncMap *map=c->curfv->map;
   SplineChar *sc;
   int i;

if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   for (i=0; i < map->enccount; ++i)
      if (c->curfv->selected[i]) {
	 sc=SFMakeChar(sf, map, i);
	 sc->color=c->a.vals[1].u.ival;
      }
   c->curfv->sf->changed=true;
}

static void bSetCharComment(Context *c) {
   SplineChar *sc;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   sc=GetOneSelChar(c);
   sc->comment =
      *c->a.vals[1].u.sval ==
      '\0' ? NULL : script2utf8_copy(c->a.vals[1].u.sval);
   c->curfv->sf->changed=true;
}

static void bSetCharCounterMask(Context *c) {
   SplineChar *sc;
   int i;
   HintMask *cm;

   for (i=1; i < c->a.argc; ++i)
      if (c->a.vals[i].type != v_int)
	 ScriptError(c, "Bad argument type");
      else if (c->a.vals[i].u.ival < 0 || c->a.vals[i].u.ival >= HntMax)
	 ScriptError(c, "Bad argument value (must be between [0,96) )");
   sc=GetOneSelChar(c);
   if (c->a.vals[1].u.ival >= sc->countermask_cnt) {
      if (sc->countermask_cnt==0) {
	 sc->countermasks =
	    calloc(c->a.vals[1].u.ival + 10, sizeof(HintMask));
	 sc->countermask_cnt=c->a.vals[1].u.ival + 1;
      } else {
	 sc->countermasks=realloc(sc->countermasks,
				    (c->a.vals[1].u.ival +
				     1) * sizeof(HintMask));
	 memset(sc->countermasks + sc->countermask_cnt, 0,
		(c->a.vals[1].u.ival + 1 -
		 sc->countermask_cnt) * sizeof(HintMask));
	 sc->countermask_cnt=c->a.vals[1].u.ival + 1;
      }
   }
   cm=&sc->countermasks[c->a.vals[1].u.ival];
   memset(cm, 0, sizeof(HintMask));
   for (i=2; i < c->a.argc; ++i)
      (*cm)[c->a.vals[i].u.ival >> 3] |= (0x80 >> (c->a.vals[i].u.ival & 7));
}

static void bSetCharName(Context *c) {
   SplineChar *sc;
   char *name;
   int uni;
   char *comment;

   if (c->a.vals[1].type != v_str
	    || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad argument type");
   sc=GetOneSelChar(c);
   uni=sc->unicodeenc;
   name=c->a.vals[1].u.sval;
   comment=fastrdup(sc->comment);

   if (c->a.argc != 3 || c->a.vals[2].u.ival) {
      uni=UniFromName(name, c->curfv->sf->uni_interp, c->curfv->map->enc);
   }
   SCSetMetaData(sc, name, uni, comment);
   /* SCLigDefault(sc); *//* Not appropriate for indic scripts. May not be appropriate anywhere. Seems to confuse people even when it is appropriate */
}

static void bSetFeatureList(Context *c) {
   OTLookup *otl;

if (c->a.vals[1].type != v_str ||
	    (c->a.vals[2].type != v_arr && c->a.vals[2].type != v_arrfree))
      ScriptError(c, "Bad type for argument");

   otl=SFFindLookup(c->curfv->sf, c->a.vals[1].u.sval);
   if (otl==NULL)
      ScriptErrorString(c, "Missing lookup", c->a.vals[1].u.sval);
   FeatureScriptLangListFree(otl->features);
   otl->features=NULL;
   otl->features=ParseFeatureList(c, c->a.vals[2].u.aval);
}

static void bSetFondName(Context *c) {
   SplineFont *sf=c->curfv->sf;

   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   if (*c->a.vals[1].u.sval != '\0') {
      free(sf->fondname);
      sf->fondname=forceASCIIcopy(c, c->a.vals[1].u.sval);
   }
}

static void bSetFontHasVerticalMetrics(Context *c) {

if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");

   c->return_val.type=v_int;
   c->return_val.u.ival=c->curfv->sf->hasvmetrics;

   c->curfv->sf->hasvmetrics=(c->a.vals[1].u.ival != 0);
}

static void bSetFontNames(Context *c) {
   SplineFont *sf=c->curfv->sf;

   _SetFontNames(c, sf);
}

static void bSetFontOrder(Context *c) {
   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   else if (c->a.vals[1].u.ival != 2 && c->a.vals[1].u.ival != 3)
      ScriptError(c, "Order must be 2 or 3");

   c->return_val.type=v_int;
   c->return_val.u.ival=c->curfv->sf->layers[ly_fore].order2 ? 2 : 3;

   if (c->a.vals[1].u.ival==(c->curfv->sf->layers[ly_fore].order2 ? 2 : 3))
      /* No Op */ ;
   else {
      if (c->a.vals[1].u.ival==2) {
	 SFConvertToOrder2(c->curfv->sf);
      } else
	 SFConvertToOrder3(c->curfv->sf);
   }
}

static void bSetGasp(Context *c) {
   int i, base;
   struct array *arr;
   SplineFont *sf=c->curfv->sf;

   if (c->a.argc==2
       && (c->a.vals[1].type==v_arr || c->a.vals[1].type==v_arrfree)) {
      arr=c->a.vals[1].u.aval;
      if (arr->argc & 1)
	 ScriptError(c, "Bad array size");
      base=0;
   } else if ((c->a.argc & 1)==0)
      ScriptError(c, "Wrong number of arguments");
   else {
      arr=&c->a;
      base=1;
   }
   for (i=base; i < arr->argc; i += 2) {
      if (arr->vals[i].type != v_int || arr->vals[i + 1].type != v_int)
	 ScriptError(c, "Bad argument type");
      if (arr->vals[i].u.ival <= 0 || arr->vals[i].u.ival > 65535)
	 ScriptError(c, "'gasp' Pixel size out of range");
      if (i != base && arr->vals[i].u.ival <= arr->vals[i - 2].u.ival)
	 ScriptError(c, "'gasp' Pixel size out of order");
      if (arr->vals[i + 1].u.ival < 0 || arr->vals[i + 1].u.ival > 15)
	 ScriptError(c, "'gasp' flag out of range");
      if (arr->vals[i+1].u.ival>3)
	 sf->gasp_version=1;
   }
   if (arr->argc >= 2 && arr->vals[arr->argc - 2].u.ival != 65535)
      ScriptError(c, "'gasp' Final pixel size must be 65535");

   free(sf->gasp);
   sf->gasp_cnt=(arr->argc - base) / 2;
   if (sf->gasp_cnt != 0) {
      sf->gasp=calloc(sf->gasp_cnt, sizeof(struct gasp));
      for (i=base; i < arr->argc; i += 2) {
	 int g=(i - base) / 2;

	 sf->gasp[g].ppem=arr->vals[i].u.ival;
	 sf->gasp[g].flags=arr->vals[i + 1].u.ival;
      }
   } else
      sf->gasp=NULL;
}

static void bSetGlyphChanged(Context *c) {
   int i, gid;
   int changed_or_not, changed_any=false;
   FontViewBase *fv=c->curfv;
   EncMap *map=fv->map;
   SplineFont *sf=fv->sf;

   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   changed_or_not=c->a.vals[1].u.ival ? true : false;

   for (i=0; i < map->enccount; ++i) {
      gid=map->map[i];
      if (gid != -1 && sf->glyphs[gid] != NULL) {
	 if (fv->selected[i]) {
	    sf->glyphs[gid]->changed=changed_or_not;
	    sf->glyphs[gid]->changedsincelasthinted=changed_or_not;
	    sf->glyphs[gid]->changed_since_search=changed_or_not;
	    sf->glyphs[gid]->namechanged=changed_or_not;
	 }
	 if (sf->glyphs[gid]->changed)
	    changed_any=true;
      }
   }
   sf->changed=changed_any;
   sf->changed_since_xuidchanged=changed_any;
}

static void bSetGlyphClass(Context *c) {
   SplineChar *sc;
   int class, gid, i;

if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   if (strmatch(c->a.vals[1].u.sval, "automatic")==0)
      class=0;
   else if (strmatch(c->a.vals[1].u.sval, "none")==0)
      class=1;
   else if (strmatch(c->a.vals[1].u.sval, "base")==0)
      class=2;
   else if (strmatch(c->a.vals[1].u.sval, "ligature")==0)
      class=3;
   else if (strmatch(c->a.vals[1].u.sval, "mark")==0)
      class=4;
   else if (strmatch(c->a.vals[1].u.sval, "component")==0)
      class=5;
   else
      ScriptErrorString(c, "Unknown glyph class: ", c->a.vals[1].u.sval);

   for (i=0; i < c->curfv->map->enccount; ++i) {
      if (c->curfv->selected[i] && (gid=c->curfv->map->map[i]) != -1 &&
	  (sc=c->curfv->sf->glyphs[gid]) != NULL)
	 sc->glyph_class=class;
   }
}

static void bSetGlyphTeX(Context *c) {
   SplineFont *sf=c->curfv->sf;
   EncMap *map=c->curfv->map;
   SplineChar *sc;
   int found;

/* bSetGlyphTeX c->a.argc != 3 && c->a.argc != 5 */
   if (c->a.vals[1].type != v_int || c->a.vals[2].type != v_int)
      ScriptError(c, "Bad type for argument");

   found=GetOneSelCharIndex(c);
   sc=SFMakeChar(sf, map, found);
   sc->tex_height=c->a.vals[1].u.ival;
   sc->tex_depth=c->a.vals[2].u.ival;

   if (c->a.argc==5) {
      if (c->a.vals[3].type != v_int || c->a.vals[4].type != v_int)
	 ScriptError(c, "Bad type for argument");
      sc->tex_height=c->a.vals[3].u.ival;
      sc->tex_depth=c->a.vals[4].u.ival;
   }
}

static void bSetItalicAngle(Context *c) {
   double denom=1;
   double num;

   if (c->a.argc==3) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad argument type");
      denom=c->a.vals[2].u.ival;
   }
   if (c->a.vals[1].type==v_real)
      num=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
      num=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad argument type");
   c->curfv->sf->italicangle=num / denom;
}

static void bSetKern(Context *c) {
   _SetKern(c, false);
}

static void bSetLBearing(Context *c) {
   int incr=0;

   if (c->a.vals[1].type != v_int
       || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad argument type in SetLBearing");
   if (c->a.argc==3)
      incr=c->a.vals[2].u.ival;
   FVSetWidthScript(c->curfv, wt_lbearing, c->a.vals[1].u.ival, incr);
}

static void bSetMacStyle(Context *c) {

   if (c->a.vals[1].type==v_int)
      c->curfv->sf->macstyle=c->a.vals[1].u.ival;
   else if (c->a.vals[1].type==v_str)
      c->curfv->sf->macstyle=_MacStyleCode(c->a.vals[1].u.sval, NULL, NULL);
   else
      ScriptError(c, "Bad argument type");
}

static void bSetMaxpValue(Context *c) {
   SplineFont *sf=c->curfv->sf;
   struct ttf_table *tab;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");

   tab=SFFindTable(sf, CHR('m', 'a', 'x', 'p'));
   if (tab==NULL) {
      tab=chunkalloc(sizeof(struct ttf_table));
      tab->next=sf->ttf_tables;
      sf->ttf_tables=tab;
      tab->tag=CHR('m', 'a', 'x', 'p');
   }
   if (tab->len < 32) {
      tab->data=realloc(tab->data, 32);
      memset(tab->data + tab->len, 0, 32 - tab->len);
      tab->data[15]=2;	/* Default zones to 2 */
      tab->len=tab->maxlen=32;
   }
   if (strmatch(c->a.vals[1].u.sval, "Zones")==0)
      memputshort(tab->data, 7 * sizeof(uint16_t), c->a.vals[2].u.ival);
   else if (strmatch(c->a.vals[1].u.sval, "TwilightPntCnt")==0)
      memputshort(tab->data, 8 * sizeof(uint16_t), c->a.vals[2].u.ival);
   else if (strmatch(c->a.vals[1].u.sval, "StorageCnt")==0)
      memputshort(tab->data, 9 * sizeof(uint16_t), c->a.vals[2].u.ival);
   else if (strmatch(c->a.vals[1].u.sval, "MaxStackDepth")==0)
      memputshort(tab->data, 12 * sizeof(uint16_t), c->a.vals[2].u.ival);
   else if (strmatch(c->a.vals[1].u.sval, "FDEFs")==0)
      memputshort(tab->data, 10 * sizeof(uint16_t), c->a.vals[2].u.ival);
   else if (strmatch(c->a.vals[1].u.sval, "IDEFs")==0)
      memputshort(tab->data, 11 * sizeof(uint16_t), c->a.vals[2].u.ival);
   else
      ScriptErrorString(c, "Unknown 'maxp' field: ", c->a.vals[1].u.sval);
}

static void bSetOS2Value(Context *c) {
   int i;
   SplineFont *sf=c->curfv->sf;
   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad argument type");
   SFDefaultOS2Info(&sf->pfminfo, sf, sf->fontname);

   if (strmatch(c->a.vals[1].u.sval, "Weight")==0) {
      setint16(&sf->pfminfo.weight, c);
   } else if (strmatch(c->a.vals[1].u.sval, "Width")==0) {
      setint16(&sf->pfminfo.width, c);
   } else if (strmatch(c->a.vals[1].u.sval, "FSType")==0) {
      setint16(&sf->pfminfo.fstype, c);
   } else if (strmatch(c->a.vals[1].u.sval, "IBMFamily")==0) {
      setint16(&sf->pfminfo.os2_family_class, c);
   } else if (strmatch(c->a.vals[1].u.sval, "VendorID")==0) {
      char *pt1, *pt2;

      if (c->a.vals[2].type != v_str)
	 ScriptError(c, "Bad argument type");
      else if (strlen(c->a.vals[2].u.sval) > 4)
	 ScriptError(c, "VendorID string limited to 4 (ASCII) characters");
      pt1=c->a.vals[2].u.sval;
      pt2=sf->pfminfo.os2_vendor;
      memset(pt2, ' ', 4);
      while (*pt1 != '\0')
	 *pt2++=*pt1++;
   } else if (strmatch(c->a.vals[1].u.sval, "WinAscent")==0) {
      setint16(&sf->pfminfo.os2_winascent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "WinAscentIsOffset")==0) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad argument type");
      sf->pfminfo.winascent_add=c->a.vals[2].u.ival;
   } else if (strmatch(c->a.vals[1].u.sval, "WinDescent")==0) {
      setint16(&sf->pfminfo.os2_windescent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "WinDescentIsOffset")==0) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad argument type");
      sf->pfminfo.windescent_add=c->a.vals[2].u.ival;
   } else if (strmatch(c->a.vals[1].u.sval, "typoAscent")==0) {
      setint16(&sf->pfminfo.os2_typoascent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "typoAscentIsOffset")==0) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad argument type");
      sf->pfminfo.typoascent_add=c->a.vals[2].u.ival;
   } else if (strmatch(c->a.vals[1].u.sval, "typoDescent")==0) {
      setint16(&sf->pfminfo.os2_typodescent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "typoDescentIsOffset")==0) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad argument type");
      sf->pfminfo.typodescent_add=c->a.vals[2].u.ival;
   } else if (strmatch(c->a.vals[1].u.sval, "typoLineGap")==0) {
      setint16(&sf->pfminfo.os2_typolinegap, c);
   } else if (strmatch(c->a.vals[1].u.sval, "hheadAscent")==0) {
      setint16(&sf->pfminfo.hhead_ascent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "hheadAscentIsOffset")==0) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad argument type");
      sf->pfminfo.hheadascent_add=c->a.vals[2].u.ival;
   } else if (strmatch(c->a.vals[1].u.sval, "hheadDescent")==0) {
      setint16(&sf->pfminfo.hhead_descent, c);
   } else if (strmatch(c->a.vals[1].u.sval, "hheadDescentIsOffset")==0) {
      if (c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad argument type");
      sf->pfminfo.hheaddescent_add=c->a.vals[2].u.ival;
   } else if (strmatch(c->a.vals[1].u.sval, "LineGap")==0
	      || strmatch(c->a.vals[1].u.sval, "HHeadLineGap")==0) {
      setint16(&sf->pfminfo.linegap, c);
   } else if (strmatch(c->a.vals[1].u.sval, "VLineGap")==0
	      || strmatch(c->a.vals[1].u.sval, "VHeadLineGap")==0) {
      setint16(&sf->pfminfo.vlinegap, c);
   } else if (strmatch(c->a.vals[1].u.sval, "Panose")==0) {
      if (c->a.vals[2].type != v_arr && c->a.vals[2].type != v_arrfree)
	 ScriptError(c, "Bad argument type");
      if (c->a.vals[2].u.aval->argc != 10)
	 ScriptError(c, "Wrong size of array");
      if (c->a.vals[2].u.aval->vals[0].type != v_int)
	 ScriptError(c, "Bad argument sub-type");
      for (i=0; i < 10; ++i) {
	 if (c->a.vals[2].u.aval->vals[i].type != v_int)
	    ScriptError(c, "Bad argument sub-type");
	 sf->pfminfo.panose[i]=c->a.vals[2].u.aval->vals[i].u.ival;
      }
      sf->pfminfo.panose_set=true;
   } else if (strmatch(c->a.vals[1].u.sval, "SubXSize")==0) {
      setss16(&sf->pfminfo.os2_subxsize, sf, c);
   } else if (strmatch(c->a.vals[1].u.sval, "SubYSize")==0) {
      setss16(&sf->pfminfo.os2_subysize, sf, c);
   } else if (strmatch(c->a.vals[1].u.sval, "SubXOffset")==0) {
      setss16(&sf->pfminfo.os2_subxoff, sf, c);
   } else if (strmatch(c->a.vals[1].u.sval, "SubYOffset")==0) {
      setss16(&sf->pfminfo.os2_subyoff, sf, c);
   } else if (strmatch(c->a.vals[1].u.sval, "supXSize")==0) {
      setss16(&sf->pfminfo.os2_supxsize, sf, c);
   } else if (strmatch(c->a.vals[1].u.sval, "supYSize")==0) {
      setss16(&sf->pfminfo.os2_supysize, sf, c);
   } else if (strmatch(c->a.vals[1].u.sval, "supXOffset")==0) {
      setss16(&sf->pfminfo.os2_supxoff, sf, c);
   } else if (strmatch(c->a.vals[1].u.sval, "supYOffset")==0) {
      setss16(&sf->pfminfo.os2_supyoff, sf, c);
   } else if (strmatch(c->a.vals[1].u.sval, "StrikeOutSize")==0) {
      setss16(&sf->pfminfo.os2_strikeysize, sf, c);
   } else if (strmatch(c->a.vals[1].u.sval, "StrikeOutPos")==0) {
      setss16(&sf->pfminfo.os2_strikeypos, sf, c);
   } else {
      ScriptErrorString(c, "Unknown OS/2 field: ", c->a.vals[1].u.sval);
   }
   sf->pfminfo.pfmset=true;
   sf->changed=true;
}

static void bSetPanose(Context *c) {
   int i;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_arr && c->a.vals[1].type != v_arrfree)
	 ScriptError(c, "Bad argument type");
      if (c->a.vals[1].u.aval->argc != 10)
	 ScriptError(c, "Wrong size of array");
      if (c->a.vals[1].u.aval->vals[0].type != v_int)
	 ScriptError(c, "Bad argument sub-type");
      SFDefaultOS2Info(&c->curfv->sf->pfminfo, c->curfv->sf,
		       c->curfv->sf->fontname);
      for (i=0; i < 10; ++i) {
	 if (c->a.vals[1].u.aval->vals[i].type != v_int)
	    ScriptError(c, "Bad argument sub-type");
	 c->curfv->sf->pfminfo.panose[i] =
	    c->a.vals[1].u.aval->vals[i].u.ival;
      }
   } else if (c->a.argc==3) {
      if (c->a.vals[1].type != v_int || c->a.vals[2].type != v_int)
	 ScriptError(c, "Bad argument type");
      if (c->a.vals[1].u.ival < 0 || c->a.vals[1].u.ival >= 10)
	 ScriptError(c, "Bad argument value must be between [0,9]");
      SFDefaultOS2Info(&c->curfv->sf->pfminfo, c->curfv->sf,
		       c->curfv->sf->fontname);
      c->curfv->sf->pfminfo.panose[c->a.vals[1].u.ival]=c->a.vals[2].u.ival;
   }
   c->curfv->sf->pfminfo.pfmset=true;
   c->curfv->sf->pfminfo.panose_set=true;
   c->curfv->sf->changed=true;
}

static void bSetPrefs(Context *c) {
   int ret;

   if (c->a.vals[1].type != v_str
       && (c->a.argc==4 && c->a.vals[3].type != v_int))
     ScriptError(c, "Bad type for argument");
   if ((ret =
	SetPrefs(c->a.vals[1].u.sval, &c->a.vals[2],
		 c->a.argc==4 ? &c->a.vals[3] : NULL))==0)
      ScriptErrorString(c, "Unknown Preference variable",
			c->a.vals[1].u.sval);
   else if (ret==-1)
      ScriptErrorString(c, "Bad type for preference variable",
			c->a.vals[1].u.sval);
}

static void bSetRBearing(Context *c) {
   int incr=0;

   if (c->a.vals[1].type != v_int
       || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad argument type in SetRBearing");
   if (c->a.argc==3)
      incr=c->a.vals[2].u.ival;
   FVSetWidthScript(c->curfv, wt_rbearing, c->a.vals[1].u.ival, incr);
}

static void bSetTTFName(Context *c) {
   SplineFont *sf=c->curfv->sf;
   char *u;
   int lang, strid;
   struct ttflangname *prev, *ln;

   if (sf->cidmaster != NULL)
      sf=sf->cidmaster;
if (c->a.vals[1].type != v_int || c->a.vals[2].type != v_int ||
	    c->a.vals[3].type != v_str)
      ScriptError(c, "Bad argument type");

   lang=c->a.vals[1].u.ival;
   strid=c->a.vals[2].u.ival;
   if (lang < 0 || lang > 0xffff)
      ScriptError(c, "Bad value for language");
   else if (strid < 0 || strid >= ttf_namemax)
      ScriptError(c, "Bad value for string id");

   u=fastrdup(c->a.vals[3].u.sval);
   if (*u=='\0') {
      free(u);
      u=NULL;
   }

   for (ln=sf->names; ln != NULL && ln->lang != lang; ln=ln->next);
   if (ln==NULL) {
      if (u==NULL)
	 return;
      for (prev=NULL, ln=sf->names; ln != NULL && ln->lang < lang;
	   prev=ln, ln=ln->next);
      ln=chunkalloc(sizeof(struct ttflangname));
      ln->lang=lang;
      if (prev==NULL) {
	 ln->next=sf->names;
	 sf->names=ln;
      } else {
	 ln->next=prev->next;
	 prev->next=ln;
      }
   }
   free(ln->names[strid]);
   ln->names[strid]=u;
}

static void bSetTeXParams(Context *c) {
   int i;

   for (i=1; i < c->a.argc; ++i)
      if (c->a.vals[i].type != v_int)
	 ScriptError(c, "Bad argument type");
   switch (c->a.vals[1].u.ival) {
     case 1:
	if (c->a.argc != 10)
	   ScriptError(c, "Wrong number of arguments");
	break;
     case 2:
	if (c->a.argc != 25)
	   ScriptError(c, "Wrong number of arguments");
	break;
     case 3:
	if (c->a.argc != 16)
	   ScriptError(c, "Wrong number of arguments");
	break;
     default:
	ScriptError(c, "Bad value for first argument, must be 1,2 or 3");
	break;
   }
   c->curfv->sf->texdata.type=c->a.vals[1].u.ival;
   c->curfv->sf->design_size=c->a.vals[2].u.ival * 10;
   /* slant is a percentage */
   c->curfv->sf->texdata.params[0] =
      ((double) c->a.vals[3].u.ival) * (1 << 20) / 100.0;
   for (i=1; i < c->a.argc - 3; ++i)
      c->curfv->sf->texdata.params[i] =
	 ((double) c->a.vals[3 + i].u.ival) * (1 << 20) /
	 (c->curfv->sf->ascent + c->curfv->sf->descent);
}

static void bSetUnicodeValue(Context *c) {
   SplineChar *sc;
   char *name;
   int uni;
   char *comment;

   if ((c->a.vals[1].type != v_int && c->a.vals[1].type != v_unicode) ||
	    (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad argument type");
   sc=GetOneSelChar(c);
   uni=c->a.vals[1].u.ival;
   name=fastrdup(sc->name);
   comment=fastrdup(sc->comment);

   if (c->a.argc != 3 || c->a.vals[2].u.ival) {
      char buffer[400];

      free(name);
      name =
	 fastrdup(StdGlyphName
	      (buffer, uni, c->curfv->sf->uni_interp,
	       c->curfv->sf->for_new_glyphs));
   }
   SCSetMetaData(sc, name, uni, comment);
   /*SCLigDefault(sc); */
}

static void bSetUniqueID(Context *c) {
   if (c->a.vals[1].type != v_int)
      ScriptError(c, "Bad argument type");
   c->curfv->sf->uniqueid=c->a.vals[1].u.ival;
}

static void bSetVKern(Context *c) {
   _SetKern(c, true);
}

static void bSetVWidth(Context *c) {
   int incr=0;

   if (c->a.vals[1].type != v_int
       || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad argument type in SetVWidth");
   if (c->a.argc==3)
      incr=c->a.vals[2].u.ival;
   FVSetWidthScript(c->curfv, wt_vwidth, c->a.vals[1].u.ival, incr);
}

static void bSetWidth(Context *c) {
   int incr=0;

   if (c->a.vals[1].type != v_int
       || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad argument type in SetWidth");
   if (c->a.argc==3)
      incr=c->a.vals[2].u.ival;
   FVSetWidthScript(c->curfv, wt_width, c->a.vals[1].u.ival, incr);
}

static void bShadow(Context *c) {
   /* Angle, outline width, shadow_len */
   double a;

   if ((c->a.vals[1].type != v_int && c->a.vals[1].type != v_real)
       || c->a.vals[2].type != v_int || c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");
   if (c->a.vals[1].type==v_int)
      a=c->a.vals[1].u.ival;
   else
      a=c->a.vals[1].u.fval;
   FVShadow(c->curfv, a * 3.1415926535897932 / 180.0,
	    c->a.vals[2].u.ival, c->a.vals[3].u.ival, false);
}

static void bShell(Context *c) {
   if (c->a.vals[1].type!=v_str)
     ScriptError(c, "Bad argument type");
   c->return_val.type=v_int;
   c->return_val.u.ival=system(c->a.vals[1].u.sval);
}

static void bSimplify(Context *c) {
   static struct simplifyinfo smpl={sf_normal,0.75,0.2,10,0,0,0};
   smpl.err=(c->curfv->sf->ascent+c->curfv->sf->descent)/1000.0;
   smpl.linefixup=(c->curfv->sf->ascent+c->curfv->sf->descent)/500.0;
   smpl.linelenmax=(c->curfv->sf->ascent+c->curfv->sf->descent)/100.0;

   if (c->a.argc >= 3 && c->a.argc <= 7) {
      if (c->a.vals[1].type != v_int
	  || (c->a.vals[2].type != v_int && c->a.vals[2].type != v_real))
	 ScriptError(c, "Bad type for argument");
      smpl.flags=c->a.vals[1].u.ival;
      if (c->a.vals[2].type==v_int)
	 smpl.err=c->a.vals[2].u.ival;
      else
	 smpl.err=c->a.vals[2].u.fval;
      if (c->a.argc >= 4) {
	 if (c->a.vals[3].type==v_int)
	    smpl.tan_bounds=c->a.vals[3].u.ival / 100.0;
	 else if (c->a.vals[3].type==v_real)
	    smpl.tan_bounds=c->a.vals[3].u.fval / 100.0;
	 else
	    ScriptError(c, "Bad type for argument");
	 if (c->a.argc >= 5) {
	    if (c->a.vals[4].type==v_int)
	       smpl.linefixup=c->a.vals[4].u.ival / 100.0;
	    else if (c->a.vals[4].type==v_real)
	       smpl.linefixup=c->a.vals[4].u.fval / 100.0;
	    else
	       ScriptError(c, "Bad type for argument");
	    if (c->a.argc >= 6) {
	       if (c->a.vals[5].type != v_int || c->a.vals[5].u.ival==0)
		  ScriptError(c, "Bad type for argument");
	       smpl.err /= (double) c->a.vals[5].u.ival;
	       if (c->a.argc >= 7) {
		  if (c->a.vals[6].type==v_int)
		     smpl.linelenmax=c->a.vals[6].u.ival;
		  else if (c->a.vals[6].type==v_real)
		     smpl.linelenmax=c->a.vals[6].u.fval;
		  else
		     smpl.linelenmax=c->a.vals[6].u.ival;
	       }
	    }
	 }
      }
   } else if (c->a.argc != 1)
      ScriptError(c, "Wrong number of arguments");
   _FVSimplify(c->curfv, &smpl);
}

static void bSin(Context *c) {
   double val;

   if (c->a.vals[1].type==v_real)
      val=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
      val=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_real;
   c->return_val.u.fval=sin(val);
}

static void bSizeOf(Context *c) {
   if (c->a.vals[1].type != v_arr && c->a.vals[1].type != v_arrfree)
      ScriptError(c, "Expected array argument");

   c->return_val.type=v_int;
   c->return_val.u.ival=c->a.vals[1].u.aval->argc;
}

static void bSkew(Context *c) {
   double args[6];
   int i;

   /* Arguments:
      2 => angle
      3 => angle-numerator, angle-denom
      4 => angle, origin-x, origin-y
      5 => angle-numerator, angle-denom, origin-x, origin-y
    */
   real trans[6];
   int otype=1;
   BVTFunc bvts[2];
   double a;

   for (i=1; i < c->a.argc; ++i) {
      if (c->a.vals[i].type==v_int)
	 args[i]=c->a.vals[i].u.ival;
      else if (c->a.vals[i].type==v_real)
	 args[i]=c->a.vals[i].u.fval;
      else
	 ScriptError(c, "Bad argument type");
   }
   if (c->a.argc==3 || c->a.argc==5)
      a=args[1] / args[2];
   else
      a=args[1];
   a=fmod(a, 360.0);
   if (a < 0)
      a += 360;
   a=a * 3.1415926535897932 / 180.;
   trans[0]=trans[3]=1;
   trans[1]=0;
   trans[2]=tan(a);
   trans[4]=trans[5]=0;
   if (c->a.argc==4) {
      trans[4]=args[2] - (trans[0] * args[2] + trans[2] * args[3]);
      trans[5]=args[3] - (trans[1] * args[2] + trans[3] * args[3]);
      otype=0;
   }
   if (c->a.argc==5) {
      trans[4]=args[3] - (trans[0] * args[3] + trans[2] * args[4]);
      trans[5]=args[4] - (trans[1] * args[3] + trans[3] * args[4]);
      otype=0;
   }
   skewselect(&bvts[0], trans[2]);
   bvts[1].func=bvt_none;
   FVTransFunc(c->curfv, trans, otype, bvts, true);
}

static void bSmallCaps(Context *c) {
   struct smallcaps small={ };
   struct position_maps maps[2]={ {.cur_width=-1}, {.cur_width=1} };
   struct genericchange genchange={
      .hcounter_scale=0.66,.lsb_scale=0.66,.rsb_scale=0.66,
      .v_scale=0.675,
      .gc=gc_smallcaps,
      .extension_for_letters="sc",
      .extension_for_symbols="taboldstyle",
      .use_vert_mapping=1,
      .dstem_control=1,
      .m={.cnt=2,.maps=maps},
      .small=&small
   };
   double h_scale, v_scale=0.66;
   double stem_h, stem_w=0.93;

   /* Arguments:
      1 => vertical scale: make smallcap height this fraction of full caps.
      2 => horizontal scale: make smallcap width this fraction of full caps,
      defaults to vertical scale if omitted or zero.
      3 => stem width scale: scale vstems by this much.
      4 => stem height scale: scale hstems by this much,
      defaults to stem width scale if omitted or zero.
    */

   SmallCapsFindConstants(&small, c->curfv->sf, c->curfv->active_layer);

   if (c->a.argc > 1) {
      if (c->a.vals[1].type==v_real)
	 v_scale=c->a.vals[1].u.fval;
      else if (c->a.vals[1].type==v_int)
	 v_scale=c->a.vals[1].u.ival;
      else
	 ScriptError(c, "Bad argument 1 type in SmallCaps");
   }
   small.vscale=small.hscale=genchange.v_scale=h_scale=v_scale;
   if (c->a.argc > 2 && c->a.vals[2].u.ival != 0) {
      if (c->a.vals[2].type==v_real)
	 h_scale=c->a.vals[2].u.fval;
      else if (c->a.vals[2].type==v_int)
	 h_scale=c->a.vals[2].u.ival;
      else
	 ScriptError(c, "Bad argument 2 type in SmallCaps");
   }
   genchange.hcounter_scale=genchange.lsb_scale=genchange.rsb_scale =
      h_scale;

   if (c->a.argc > 3) {
      if (c->a.vals[3].type==v_real)
	 stem_w=c->a.vals[3].u.fval;
      else if (c->a.vals[3].type==v_int)
	 stem_w=c->a.vals[3].u.ival;
      else
	 ScriptError(c, "Bad argument 3 type in SmallCaps");
   }
   stem_h=stem_w;

   if (c->a.argc > 4 && c->a.vals[4].u.ival != 0) {
      if (c->a.vals[4].type==v_real)
	 stem_h=c->a.vals[4].u.fval;
      else if (c->a.vals[4].type==v_int)
	 stem_h=c->a.vals[4].u.ival;
      else
	 ScriptError(c, "Bad argument 4 type in SmallCaps");
   }
   genchange.stem_height_scale=stem_h;
   genchange.stem_width_scale=stem_w;

   maps[1].current=small.capheight;
   maps[1].desired=small.scheight=small.capheight * small.vscale;

   FVAddSmallCaps(c->curfv, &genchange);
}

static void bSqrt(Context *c) {
   double val;

   if (c->a.vals[1].type==v_real)
      val=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
      val=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_real;
   c->return_val.u.fval=sqrt(val);
}

static void bStrJoin(Context *c) {
   char *str2;
   int len, len2, k, i;
   Array *arr;

   if ((c->a.vals[1].type != v_arr && c->a.vals[1].type != v_arrfree) ||
       c->a.vals[2].type != v_str)
     ScriptError(c, "Bad type for argument");

   arr=c->a.vals[1].u.aval;
   str2=c->a.vals[2].u.sval;
   len2=strlen(str2);

   for (k=0; k < 2; ++k) {
      len=0;
      for (i=0; i < arr->argc; ++i) {
	 if (arr->vals[i].type != v_str)
	    ScriptError(c, "Bad type for array element");
	 if (k) {
	    strcpy(c->return_val.u.sval + len, arr->vals[i].u.sval);
	    strcat(c->return_val.u.sval + len, str2);
	 }
	 len += strlen(arr->vals[i].u.sval) + len2;
      }
      if (!k) {
	 c->return_val.type=v_str;
	 c->return_val.u.sval=malloc(len + 1);
      }
   }
}

static void bStrSplit(Context *c) {
   char *pt, *pt2, *str1, *str2;
   int max=-1, len2, cnt, k;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str)
      ScriptError(c, "Bad type for argument");
   else if (c->a.argc==4 && c->a.vals[3].type != v_int)
      ScriptError(c, "Bad type for argument");
   else if (c->a.argc==4)
      max=c->a.vals[3].u.ival;

   str1=c->a.vals[1].u.sval;
   str2=c->a.vals[2].u.sval;
   len2=strlen(str2);

   for (k=0; k < 2; ++k) {
      cnt=0;
      pt=str1;
      while ((pt2=strstr(pt, str2)) != NULL) {
	 if (k) {
	    if (max != -1 && cnt >= max)
	       break;
	    c->return_val.u.aval->vals[cnt].type=v_str;
	    c->return_val.u.aval->vals[cnt].u.sval=copyn(pt, pt2 - pt);
	 }
	 ++cnt;
	 pt=pt2 + len2;
      }
      if (!k) {
	 if (*pt != '\0')
	    ++cnt;
	 if (max != -1 && cnt > max)
	    cnt=max;
	 c->return_val.type=v_arrfree;
	 c->return_val.u.aval=malloc(sizeof(Array));
	 c->return_val.u.aval->argc=cnt;
	 c->return_val.u.aval->vals=malloc(cnt * sizeof(Val));
      } else {
	 if (*pt != '\0') {
	    c->return_val.u.aval->vals[cnt].type=v_str;
	    c->return_val.u.aval->vals[cnt].u.sval=fastrdup(pt);
	 }
      }
   }
}

static void bStrcasecmp(Context *c) {
   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str)
      ScriptError(c, "Bad type for argument");

   c->return_val.type=v_int;
   c->return_val.u.ival=strmatch(c->a.vals[1].u.sval, c->a.vals[2].u.sval);
}

static void bStrcasestr(Context *c) {
   char *pt;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str)
      ScriptError(c, "Bad type for argument");

   c->return_val.type=v_int;
   pt=strstrmatch(c->a.vals[1].u.sval, c->a.vals[2].u.sval);
   c->return_val.u.ival=pt==NULL ? -1 : pt - c->a.vals[1].u.sval;
}

static void bStrftime(Context *c) {
   int isgmt=1;
   char *oldloc=NULL;
   char buffer[300];
   struct tm *tm;
   time_t now;

   if (c->a.vals[1].type != v_str ||
       (c->a.argc >= 3 && c->a.vals[2].type != v_int) ||
       (c->a.argc >= 4 && c->a.vals[3].type != v_str))
     ScriptError(c, "Bad type for argument");
   if (c->a.argc >= 3)
      isgmt=c->a.vals[2].u.ival;
   if (c->a.argc >= 4)
      oldloc=setlocale(LC_TIME, c->a.vals[3].u.sval);

   time(&now);
   if (isgmt)
      tm=gmtime(&now);
   else
      tm=localtime(&now);
   strftime(buffer, sizeof(buffer), c->a.vals[1].u.sval, tm);

   if (oldloc != NULL)
      (void) setlocale(LC_TIME, oldloc);

   c->return_val.type=v_str;
   c->return_val.u.sval=fastrdup(buffer);
}

static void bStrlen(Context *c) {
   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");

   c->return_val.type=v_int;
   c->return_val.u.ival=strlen(c->a.vals[1].u.sval);
}

static void bStrrstr(Context *c) {
   char *pt;
   char *haystack, *needle;
   int nlen;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str)
     ScriptError(c, "Bad type for argument");

   c->return_val.type=v_int;
   haystack=c->a.vals[1].u.sval;
   needle=c->a.vals[2].u.sval;
   nlen=strlen(needle);
   for (pt=haystack + strlen(haystack) - nlen; pt >= haystack; --pt)
      if (strncmp(pt, needle, nlen)==0)
	 break;
   c->return_val.u.ival=pt - haystack;
}

static void bStrskipint(Context *c) {
   int base=10;
   char *end;

   if (c->a.vals[1].type != v_str
	    || (c->a.argc==3 && c->a.vals[2].type != v_int))
      ScriptError(c, "Bad type for argument");
   else if (c->a.argc==3) {
      base=c->a.vals[2].u.ival;
      if (base < 0 || base==1 || base > 36)
	 ScriptError(c, "Argument out of bounds");
   }

   c->return_val.type=v_int;
   strtol(c->a.vals[1].u.sval, &end, base);
   c->return_val.u.ival=end - c->a.vals[1].u.sval;
}

static void bStrstr(Context *c) {
   char *pt;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_str)
     ScriptError(c, "Bad type for argument");

   c->return_val.type=v_int;
   pt=strstr(c->a.vals[1].u.sval, c->a.vals[2].u.sval);
   c->return_val.u.ival=pt==NULL ? -1 : pt - c->a.vals[1].u.sval;
}

static void bStrsub(Context *c) {
   int start, end;
   char *str;

   if (c->a.vals[1].type != v_str || c->a.vals[2].type != v_int ||
	    (c->a.argc==4 && c->a.vals[3].type != v_int))
      ScriptError(c, "Bad type for argument");

   str=c->a.vals[1].u.sval;
   start=c->a.vals[2].u.ival;
   end=c->a.argc==4 ? c->a.vals[3].u.ival : strlen(str);
   if (start < 0 || start > strlen(str) || end < start || end > strlen(str))
      ScriptError(c, "Arguments out of bounds");
   c->return_val.type=v_str;
   c->return_val.u.sval=copyn(str + start, end - start);
}

static void bStrtod(Context *c) {
   if (c->a.vals[1].type != v_str)
     ScriptError(c, "Bad type for argument");

   c->return_val.type=v_real;
   c->return_val.u.fval=(float) strtod(c->a.vals[1].u.sval, NULL);
}

static void bStrtol(Context *c) {
   int base=10;

   if (c->a.vals[1].type != v_str
       || (c->a.argc==3 && c->a.vals[2].type != v_int))
     ScriptError(c, "Bad type for argument");
   else if (c->a.argc==3) {
      base=c->a.vals[2].u.ival;
      if (base < 0 || base==1 || base > 36)
	 ScriptError(c, "Argument out of bounds");
   }

   c->return_val.type=v_int;
   c->return_val.u.ival=strtol(c->a.vals[1].u.sval, NULL, base);
}

static void bSubstitutionPoints(Context *c) {
   FVAutoHintSubs(c->curfv);
}

static void bTan(Context *c) {
   double val;

   if (c->a.vals[1].type==v_real)
      val=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_int)
      val=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_real;
   c->return_val.u.fval=tan(val);
}

static void bToString(Context *c) {
   c->return_val.type=v_str;
   c->return_val.u.sval=ToString(&c->a.vals[1]);
}

static void bTransform(Context *c) {
   real trans[6];
   BVTFunc bvts[1];
   int i;

   for (i=1; i < 7; ++i) {
      if (c->a.vals[i].type==v_real)
	 trans[i - 1]=c->a.vals[i].u.fval / 100.;
      else if (c->a.vals[i].type==v_int)
	 trans[i - 1]=c->a.vals[i].u.ival / 100.;
      else
	 ScriptError(c, "Bad argument type in Transform");
   }
   bvts[0].func=bvt_none;
   FVTransFunc(c->curfv, trans, 0, bvts, true);
}

static void bTypeOf(Context *c) {
   static char *typenames[] =
      { "Integer", "Real", "String", "Unicode", "LValue",
      "Array", "Array", "LValue", "LValue", "LValue", "Void"
   };

   c->return_val.type=v_str;
   c->return_val.u.sval=fastrdup(typenames[c->a.vals[1].type]);
}

static void bUCS4(Context *c) {
   if (c->a.vals[1].type==v_str) {
      const char *pt=c->a.vals[1].u.sval;
      int i, len=utf8_strlen(pt);

      c->return_val.type=v_arrfree;
      c->return_val.u.aval=malloc(sizeof(Array));
      c->return_val.u.aval->argc=len;
      c->return_val.u.aval->vals=malloc(len * sizeof(Val));
      for (i=0; i < len; ++i) {
	 c->return_val.u.aval->vals[i].type=v_int;
	 c->return_val.u.aval->vals[i].u.ival=utf8_ildb(&pt);
      }
   } else
      ScriptError(c, "Bad type for argument");
}

static void bUCodePoint(Context *c) {
   if (c->a.vals[1].type==v_real)
      c->return_val.u.ival=c->a.vals[1].u.fval;
   else if (c->a.vals[1].type==v_unicode || c->a.vals[1].type==v_int)
      c->return_val.u.ival=c->a.vals[1].u.ival;
   else
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_unicode;
}

static void bUnicodeAnnotationFromLib(Context *c) {
   char *temp;

if (c->a.vals[1].type != v_int && c->a.vals[1].type != v_unicode)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_str;

   if ((temp=unicode_annot(c->a.vals[1].u.ival))==NULL) {
      temp=malloc(1 * sizeof(char));
      *temp='\0';
   }
   c->return_val.u.sval=temp;
}

static void bUnicodeBlockEndFromLib(Context *c) {
/* If the library is available, then get the official Nth block end */
   if (c->a.vals[1].type != v_int && c->a.vals[1].type != v_unicode)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_int;

   c->return_val.u.ival=unicode_block_end(c->a.vals[1].u.ival);
}

static void bUnicodeBlockNameFromLib(Context *c) {
/* If the library is available, then get the official Nth block name */
   char *temp;

   if (c->a.vals[1].type != v_int && c->a.vals[1].type != v_unicode)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_str;

   if ((temp=unicode_block_name(c->a.vals[1].u.ival))==NULL) {
      temp=malloc(1 * sizeof(char));
      *temp='\0';
   }
   c->return_val.u.sval=temp;
}

static void bUnicodeBlockStartFromLib(Context *c) {
/* If the library is available, then get the official Nth block start */
if (c->a.vals[1].type != v_int && c->a.vals[1].type != v_unicode)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_int;

   c->return_val.u.ival=unicode_block_start(c->a.vals[1].u.ival);
}

static void bUnicodeFromName(Context *c) {
   if (c->a.vals[1].type != v_str)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_int;
   c->return_val.u.ival=UniFromName(c->a.vals[1].u.sval, ui_none, &custom);
}

static void bUnicodeNameFromLib(Context *c) {
/* If the library is available, then get the official name for this unicode value */
   char *temp;

if (c->a.vals[1].type != v_int && c->a.vals[1].type != v_unicode)
      ScriptError(c, "Bad type for argument");
   c->return_val.type=v_str;

   if ((temp=unicode_name(c->a.vals[1].u.ival))==NULL) {
      temp=malloc(1 * sizeof(char));
      *temp='\0';
   }
   c->return_val.u.sval=temp;
}

static void bUnicodeNamesListVersion(Context *c) {
   c->return_val.type=v_str;
   c->return_val.u.sval=fastrdup(PACKAGE_STRING " (" FONTANVIL_VERSIONDATE ")");
}

static void bUnlinkReference(Context *c) {
   FVUnlinkRef(c->curfv);
}

static void bUtf8(Context *c) {
   uint32_t buf[2];
   int i;
   uint32_t *temp;

   if (c->a.vals[1].type==v_int) {
      if (c->a.vals[1].u.ival < 0 || c->a.vals[1].u.ival > 0x10ffff)
	 ScriptError(c, "Bad value for argument");
      buf[0]=c->a.vals[1].u.ival;
      buf[1]=0;
      c->return_val.type=v_str;
      c->return_val.u.sval=u2utf8_copy(buf);
   } else if (c->a.vals[1].type==v_arr || c->a.vals[1].type==v_arrfree) {
      Array *arr=c->a.vals[1].u.aval;

      temp=malloc((arr->argc + 1) * sizeof(int32_t));
      for (i=0; i < arr->argc; ++i) {
	 if (arr->vals[i].type != v_int)
	    ScriptError(c, "Bad type for argument");
	 else if (arr->vals[i].u.ival < 0 || arr->vals[i].u.ival > 0x10ffff)
	    ScriptError(c, "Bad value for argument");
	 temp[i]=arr->vals[i].u.ival;
      }
      temp[i]=0;
      c->return_val.type=v_str;
      c->return_val.u.sval=u2utf8_copy(temp);
      free(temp);
   } else
      ScriptError(c, "Bad type for argument");
}

static void bVFlip(Context *c) {
   real trans[6];
   int otype=1;
   BVTFunc bvts[2];

   trans[0]=1;
   trans[3]=-1;
   trans[1]=trans[2]=trans[4]=trans[5]=0;
   if (c->a.argc==1)
      /* default to center of char for origin */ ;
   else if (c->a.argc==2) {
      if ((c->a.vals[1].type != v_int && c->a.vals[1].type != v_real) ||
	  (c->a.argc==3 && c->a.vals[2].type != v_int))
	 ScriptError(c, "Bad argument type in VFlip");
      if (c->a.vals[1].type==v_int)
	 trans[5]=2 * c->a.vals[1].u.ival;
      else
	 trans[5]=2 * c->a.vals[1].u.fval;
      otype=0;
   }
   bvts[0].func=bvt_flipv;
   bvts[1].func=bvt_none;
   FVTransFunc(c->curfv, trans, otype, bvts, true);
}

static void bVKernFromHKern(Context *c) {

   FVVKernFromHKern(c->curfv);
}

/* **** MM menu **** */

static void bValidate(Context *c) {
   int force=false;

   if (c->a.argc==2) {
      if (c->a.vals[1].type != v_int)
	 ScriptError(c, "Bad type for argument");
      force=c->a.vals[1].u.ival;
   }

   c->return_val.type=v_int;
   c->return_val.u.ival=SFValidate(c->curfv->sf, ly_fore, force);
}

static void bWireframe(Context *c) {
   /* Angle, outline width, shadow_len */
   double a;

   if ((c->a.vals[1].type != v_int && c->a.vals[1].type != v_real)
       || c->a.vals[2].type != v_int || c->a.vals[2].type != v_int)
      ScriptError(c, "Bad argument type");
   if (c->a.vals[1].type==v_int)
      a=c->a.vals[1].u.ival;
   else
      a=c->a.vals[1].u.fval;
   FVShadow(c->curfv, a * 3.1415926535897932 / 360.,
	    c->a.vals[2].u.ival, c->a.vals[3].u.ival, true);
}

static void bWorthOutputting(Context *c) {
   SplineFont *sf=c->curfv->sf;
   EncMap *map=c->curfv->map;
   int gid;

   c->return_val.type=v_int;
   if (c->a.argc==1) {
      gid=map->map[GetOneSelCharIndex(c)];
   } else if (c->a.vals[1].type==v_int) {
      gid=c->a.vals[1].u.ival >= 0 && c->a.vals[1].u.ival < map->enccount ?
	 map->map[c->a.vals[1].u.ival] : -1;
   } else if (c->a.vals[1].type==v_unicode || c->a.vals[1].type==v_str) {
      int enc;

      if (c->a.vals[1].type==v_unicode)
	 enc=SFFindSlot(sf, map, c->a.vals[1].u.ival, NULL);
      else {
	 enc=NameToEncoding(sf, map, c->a.vals[1].u.sval);
      }
      gid=enc==-1 ? -1 : map->map[enc];
   } else
      ScriptError(c, "Bad type of argument");
   c->return_val.u.ival=gid != -1 && SCWorthOutputting(sf->glyphs[gid]);
}

static void bWritePfm(Context *c) {
   SplineFont *sf=c->curfv->sf;
   char *t;
   char *locfilename;
   if (c->a.vals[1].type != v_str)
     ScriptError(c, "Bad type of argument");

   t=script2utf8_copy(c->a.vals[1].u.sval);
   locfilename=fastrdup(t);
   if (!WritePfmFile(c->a.vals[1].u.sval, sf, 0, c->curfv->map))
     ScriptError(c, "Save failed");
   free(locfilename);
   free(t);
}

static void bWriteStringToFile(Context *c) {
   AFILE *f;
   int append=0;
   char *name, *_name;

   if (c->a.vals[1].type != v_str && c->a.vals[2].type != v_str)
      ScriptError(c, "Bad type of argument");
   else if (c->a.argc==4) {
      if (c->a.vals[3].type != v_int)
	 ScriptError(c, "Bad type of argument");
      append=c->a.vals[3].u.ival;
   }
   _name=script2utf8_copy(c->a.vals[2].u.sval);
   name=fastrdup(_name);
   free(_name);
   f=afopen(name, append ? "ab" : "wb");
   free(name);
   c->return_val.type=v_int;
   if (f==NULL)
      c->return_val.u.ival=-1;
   else {
      c->return_val.u.ival =
	 afwrite(c->a.vals[1].u.sval, 1, strlen(c->a.vals[1].u.sval), f);
      afclose(f);
   }
}

static void bisalnum(Context *c) {
   const char *pt;
   long ch;

   c->return_val.type=v_int;
   if (c->a.vals[1].type==v_str) {
      pt=c->a.vals[1].u.sval;
      ch=utf8_ildb(&pt);
      c->return_val.u.ival=ch >= 0 && ch < 0x10000 ? isalnum(ch) : 0;
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode)
      c->return_val.u.ival=isalnum(c->a.vals[1].u.ival);
   else
      ScriptError(c, "Bad type for argument");
}

static void bisalpha(Context *c) {
   const char *pt;
   long ch;

   c->return_val.type=v_int;
   if (c->a.vals[1].type==v_str) {
      pt=c->a.vals[1].u.sval;
      ch=utf8_ildb(&pt);
      c->return_val.u.ival=ch >= 0 && ch < 0x10000 ? isalpha(ch) : 0;
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode)
      c->return_val.u.ival=isalpha(c->a.vals[1].u.ival);
   else
      ScriptError(c, "Bad type for argument");
}

static void bisdigit(Context *c) {
   const char *pt;
   long ch;

   c->return_val.type=v_int;
   if (c->a.vals[1].type==v_str) {
      pt=c->a.vals[1].u.sval;
      ch=utf8_ildb(&pt);
      c->return_val.u.ival=ch >= 0 && ch < 0x10000 ? isdigit(ch) : 0;
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode)
      c->return_val.u.ival=isdigit(c->a.vals[1].u.ival);
   else
      ScriptError(c, "Bad type for argument");
}

static void bishexdigit(Context *c) {
   const char *pt;
   long ch;

   c->return_val.type=v_int;
   if (c->a.vals[1].type==v_str) {
      pt=c->a.vals[1].u.sval;
      ch=utf8_ildb(&pt);
      c->return_val.u.ival=ch >= 0 && ch < 0x10000 ? ishexdigit(ch) : 0;
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode)
      c->return_val.u.ival=ishexdigit(c->a.vals[1].u.ival);
   else
      ScriptError(c, "Bad type for argument");
}

static void bislower(Context *c) {
   const char *pt;
   long ch;

   c->return_val.type=v_int;
   if (c->a.vals[1].type==v_str) {
      pt=c->a.vals[1].u.sval;
      ch=utf8_ildb(&pt);
      c->return_val.u.ival=ch >= 0 && ch < 0x10000 ? islower(ch) : 0;
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode)
      c->return_val.u.ival=islower(c->a.vals[1].u.ival);
   else
      ScriptError(c, "Bad type for argument");
}

static void bisspace(Context *c) {
   const char *pt;
   long ch;

   c->return_val.type=v_int;
   if (c->a.vals[1].type==v_str) {
      pt=c->a.vals[1].u.sval;
      ch=utf8_ildb(&pt);
      c->return_val.u.ival=ch >= 0 && ch < 0x10000 ? isspace(ch) : 0;
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode)
      c->return_val.u.ival=isspace(c->a.vals[1].u.ival);
   else
      ScriptError(c, "Bad type for argument");
}

static void bisupper(Context *c) {
   const char *pt;
   long ch;

   c->return_val.type=v_int;
   if (c->a.vals[1].type==v_str) {
      pt=c->a.vals[1].u.sval;
      ch=utf8_ildb(&pt);
      c->return_val.u.ival=ch >= 0 && ch < 0x10000 ? isupper(ch) : 0;
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode)
      c->return_val.u.ival=isupper(c->a.vals[1].u.ival);
   else
      ScriptError(c, "Bad type for argument");
}

static void btolower(Context *c) {
   char *pt;
   const char *ipt;
   long ch;

   if (c->a.vals[1].type==v_str) {
      c->return_val.type=v_str;
      c->return_val.u.sval=pt=fastrdup(ipt=c->a.vals[1].u.sval);
      while (*ipt) {
	 ch=utf8_ildb(&ipt);
	 if (ch==-1)
	    break;
	 if (ch < 0x10000)
	    ch=tolower(ch);
	 pt=utf8_idpb(pt, ch, UTF8IDPB_NOZERO);
      }
      *pt='\0';
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode) {
      c->return_val.type=v_int;
      c->return_val.u.ival =
	 c->a.vals[1].u.ival <
	 0x10000 ? tolower(c->a.vals[1].u.ival) : c->a.vals[1].u.ival;
   } else
      ScriptError(c, "Bad type for argument");
}

static void btomirror(Context *c) {
   char *pt;
   const char *ipt;
   long ch;

   if (c->a.vals[1].type==v_str) {
      c->return_val.type=v_str;
      c->return_val.u.sval=pt=fastrdup(ipt=c->a.vals[1].u.sval);
      while (*ipt) {
	 ch=utf8_ildb(&ipt);
	 if (ch==-1)
	    break;
	 if (ch < 0x10000)
	    ch=tomirror(ch);
	 pt=utf8_idpb(pt, ch, UTF8IDPB_NOZERO);
      }
      *pt='\0';
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode) {
      c->return_val.type=v_int;
      c->return_val.u.ival =
	 c->a.vals[1].u.ival <
	 0x10000 ? tomirror(c->a.vals[1].u.ival) : c->a.vals[1].u.ival;
   } else
      ScriptError(c, "Bad type for argument");
}

static void btoupper(Context *c) {
   char *pt;
   const char *ipt;
   long ch;

   if (c->a.vals[1].type==v_str) {
      c->return_val.type=v_str;
      c->return_val.u.sval=pt=fastrdup(ipt=c->a.vals[1].u.sval);
      while (*ipt) {
	 ch=utf8_ildb(&ipt);
	 if (ch==-1)
	    break;
	 if (ch < 0x10000)
	    ch=toupper(ch);
	 pt=utf8_idpb(pt, ch, UTF8IDPB_NOZERO);
      }
      *pt='\0';
   } else if (c->a.vals[1].type==v_int || c->a.vals[1].type==v_unicode) {
      c->return_val.type=v_int;
      c->return_val.u.ival =
	 c->a.vals[1].u.ival <
	 0x10000 ? toupper(c->a.vals[1].u.ival) : c->a.vals[1].u.ival;
   } else
      ScriptError(c, "Bad type for argument");
}

static void bGetCoverageCounts(Context *);  /* forward... */

static struct builtins {
   char *name;
   void (*func)(Context *);
   int nofontok;
   int minargs,maxargs;
   unsigned long count;
} builtins[]={
   {"ATan2", bATan2, 1,3,3,0},
   {"AddAccent", bAppendAccent, 0,2,3,0},
   {"AddAnchorClass", bAddAnchorClass, 0,4,4,0},
   {"AddAnchorPoint", bAddAnchorPoint, 0,5,6,0},
   {"AddDHint", bAddDHint, 0,7,7,0},
   {"AddExtrema", bAddExtrema, 0,1,2,0},
   {"AddHHint", bAddHHint, 0,0,-1,0},
   {"AddInstrs", bAddInstrs, 0,4,4,0},
   {"AddLookup", bAddLookup, 0,5,6,0},
   {"AddLookupSubtable", bAddLookupSubtable, 0,3,4,0},
   {"AddPosSub", bAddPosSub, 0,3,11,0},
   {"AddSizeFeature", bAddSizeFeature, 0,2,6,0},
   {"AddVHint", bAddVHint, 0,0,-1,0},
   {"ApplySubstitution", bApplySubstitution, 0,4,4,0},
   {"Array", bArray, 1,2,2,0},
   {"AskUser", bAskUser, 1,2,3,0},
   {"AutoCounter", bAutoCounter, 0,1,1,0},
   {"AutoHint", bAutoHint, 0,1,1,0},
   {"AutoInstr", bAutoInstr, 0,1,1,0},
   {"AutoKern", bAutoKern, 0,4,5,0},
   {"AutoTrace", bAutoTrace, 0,0,-1,0},
   {"AutoWidth", bAutoWidth, 0,2,4,0},
   {"BitmapsAvail", bBitmapsAvail, 0,0,-1,0},
   {"BitmapsRegen", bBitmapsRegen, 0,0,-1,0},
   {"BuildAccented", bBuildAccented, 0,1,1,0},
   {"BuildComposite", bBuildComposite, 0,1,1,0},
   {"BuildDuplicate", bBuildDuplicate, 0,1,1,0},
   {"CIDChangeSubFont", bCIDChangeSubFont, 0,2,2,0},
   {"CIDFlatten", bCIDFlatten, 0,1,1,0},
   {"CIDFlattenByCMap", bCIDFlattenByCMap, 0,2,2,0},
   {"CIDSetFontNames", bCIDSetFontNames, 0,2,7,0},
   {"CanonicalContours", bCanonicalContours, 0,1,1,0},
   {"CanonicalStart", bCanonicalStart, 0,1,1,0},
   {"Ceil", bCeil, 1,2,2,0},
   {"CenterInWidth", bCenterInWidth, 0,1,1,0},
   {"ChangePrivateEntry", bChangePrivateEntry, 0,3,3,0},
   {"ChangeWeight", bChangeWeight, 0,0,2,0},
   {"CharCnt", bCharCnt, 0,1,1,0},
   {"CharInfo", bCharInfo, 0,2,3,0},
   {"CheckForAnchorClass", bCheckForAnchorClass, 0,2,2,0},
   {"Chr", bChr, 1,2,2,0},
   {"Clear", bClear, 0,1,1,0},
   {"ClearBackground", bClearBackground, 0,1,1,0},
   {"ClearCharCounterMasks", bClearCharCounterMasks, 0,1,1,0},
   {"ClearGlyphCounterMasks", bClearCharCounterMasks, 0,1,1,0},
   {"ClearHints", bClearHints, 0,0,2,0},
   {"ClearInstrs", bClearInstrs, 0,1,1,0},
   {"ClearPrivateEntry", bClearPrivateEntry, 0,2,2,0},
   {"ClearTable", bClearTable, 0,2,2,0},
   {"Close", bClose, 0,1,1,0},
   {"CompareFonts", bCompareFonts, 0,4,4,0},
   {"CompareGlyphs", bCompareGlyphs, 0,0,7,0},
   {"ControlAfmLigatureOutput", bControlAfmLigatureOutput, 0,2,3,0},
   {"ConvertByCMap", bConvertByCMap, 0,2,2,0},
   {"ConvertToCID", bConvertToCID, 0,4,4,0},
   {"Copy", bCopy, 0,1,1,0},
   {"CopyAnchors", bCopyAnchors, 0,1,1,0},
   {"CopyFgToBg", bCopyFgToBg, 0,1,1,0},
   {"CopyGlyphFeatures", bCopyGlyphFeatures, 0,0,-1,0},
   {"CopyLBearing", bCopyLBearing, 0,1,1,0},
   {"CopyRBearing", bCopyRBearing, 0,1,1,0},
   {"CopyReference", bCopyReference, 0,1,1,0},
   {"CopyUnlinked", bCopyUnlinked, 0,1,1,0},
   {"CopyVWidth", bCopyVWidth, 0,1,1,0},
   {"CopyWidth", bCopyWidth, 0,1,1,0},
   {"CorrectDirection", bCorrectDirection, 0,1,2,0},
   {"Cos", bCos, 1,2,2,0},
   {"Cut", bCut, 0,1,1,0},
   {"DebugCrashFontForge", bDebugCrashFontForge, 1,0,-1,0},
   {"DefaultOtherSubrs", bDefaultOtherSubrs, 1,1,1,0},
   {"DefaultRoundToGrid", bDefaultRoundToGrid, 0,1,1,0},
   {"DefaultUseMyMetrics", bDefaultUseMyMetrics, 0,1,1,0},
   {"DetachAndRemoveGlyphs", bDetachAndRemoveGlyphs, 0,0,-1,0},
   {"DetachGlyphs", bDetachGlyphs, 0,0,-1,0},
   {"DontAutoHint", bDontAutoHint, 0,1,1,0},
   {"DrawsSomething", bDrawsSomething, 0,2,2,0},
   {"Error", bError, 1,2,2,0},
   {"Exp", bExp, 1,2,2,0},
   {"ExpandStroke", bExpandStroke, 0,2,7,0},
   {"Export", bExport, 0,2,3,0},
   {"FileAccess", bFileAccess, 1,2,3,0},
   {"FindIntersections", bFindIntersections, 0,1,1,0},
   {"FindOrAddCvtIndex", bFindOrAddCvtIndex, 0,2,3,0},
   {"Floor", bFloor, 1,2,2,0},
   {"FontImage", bFontImage, 0,3,5,0},
   {"FontsInFile", bFontsInFile, 1,2,2,0},
   {"Generate", bGenerate, 0,2,7,0},
   {"GenerateFamily", bGenerateFamily, 0,5,5,0},
   {"GenerateFeatureFile", bGenerateFeatureFile, 0,2,3,0},
   {"GetAnchorPoints", bGetAnchorPoints, 0,0,-1,0},
   {"GetCoverageCounts", bGetCoverageCounts, 1,1,1,0},
   {"GetCvtAt", bGetCvtAt, 0,2,2,0},
   {"GetEnv", bGetEnv, 1,2,2,0},
   {"GetFontBoundingBox", bGetFontBoundingBox, 0,1,1,0},
   {"GetLookupInfo", bGetLookupInfo, 0,2,2,0},
   {"GetLookupOfSubtable", bGetLookupOfSubtable, 0,2,2,0},
   {"GetLookupSubtables", bGetLookupSubtables, 0,2,2,0},
   {"GetLookups", bGetLookups, 0,2,2,0},
   {"GetMaxpValue", bGetMaxpValue, 0,2,2,0},
   {"GetOS2Value", bGetOS2Value, 0,0,-1,0},
   {"GetPosSub", bGetPosSub, 0,2,2,0},
   {"GetPref", bGetPrefs, 1,2,2,0},
   {"GetPrivateEntry", bGetPrivateEntry, 0,2,2,0},
   {"GetSubtableOfAnchorClass", bGetSubtableOfAnchorClass, 0,2,2,0},
   {"GetTTFName", bGetTTFName, 0,3,3,0},
   {"GetTeXParam", bGetTeXParam, 0,2,2,0},
   {"GlyphInfo", bCharInfo, 0,0,-1,0},
   {"HFlip", bHFlip, 0,1,2,0},
   {"HasPreservedTable", bHasPreservedTable, 0,2,2,0},
   {"HasPrivateEntry", bHasPrivateEntry, 0,2,2,0},
   {"Import", bImport, 0,2,4,0},
   {"InFont", bInFont, 0,0,2,0},
   {"Inline", bInline, 0,3,3,0},
   {"Int", bInt, 1,2,2,0},
   {"InterpolateFonts", bInterpolateFonts, 0,3,4,0},
   {"IsAlNum", bisalnum, 1,2,2,0},
   {"IsAlpha", bisalpha, 1,2,2,0},
   {"IsDigit", bisdigit, 1,2,2,0},
   {"IsFinite", bIsFinite, 1,2,2,0},
   {"IsHexDigit", bishexdigit, 1,2,2,0},
   {"IsLower", bislower, 1,2,2,0},
   {"IsNan", bIsNan, 1,2,2,0},
   {"IsSpace", bisspace, 1,2,2,0},
   {"IsUpper", bisupper, 1,2,2,0},
   {"Italic", bItalic, 0,0,10,0},
   {"Join", bJoin, 0,1,1,0},
   {"LoadEncodingFile", bLoadEncodingFile, 1,2,3,0},
   {"LoadNamelist", bLoadNamelist, 1,2,2,0},
   {"LoadNamelistDir", bLoadNamelistDir, 1,0,2,0},
   {"LoadStringFromFile", bLoadFileToString, 1,2,2,0},
   {"LoadTableFromFile", bLoadTableFromFile, 0,3,3,0},
   {"Log", bLog, 1,2,2,0},
   {"LookupStoreLigatureInAfm", bLookupStoreLigatureInAfm, 0,3,3,0},
   {"MMAxisBounds", bMMAxisBounds, 0,2,2,0},
   {"MMAxisNames", bMMAxisNames, 0,1,1,0},
   {"MMBlendToNewFont", bMMBlendToNewFont, 0,0,-1,0},
   {"MMChangeInstance", bMMChangeInstance, 0,2,2,0},
   {"MMChangeWeight", bMMChangeWeight, 0,0,-1,0},
   {"MMInstanceNames", bMMInstanceNames, 0,1,1,0},
   {"MMWeightedName", bMMWeightedName, 0,1,1,0},
   {"MakeLine", bMakeLine, 0,0,-1,0},
   {"MergeFeature", bMergeKern, 0,2,2,0},
   {"MergeFonts", bMergeFonts, 0,2,3,0},
   {"MergeKern", bMergeKern, 0,2,2,0},
   {"MergeLookupSubtables", bMergeLookupSubtables, 0,3,3,0},
   {"MergeLookups", bMergeLookups, 0,3,3,0},
   {"Move", bMove, 0,3,3,0},
   {"MoveReference", bMoveReference, 0,4,-1,0},
   {"MultipleEncodingsToReferences", bMultipleEncodingsToReferences, 0,1,1,0},
   {"NameFromUnicode", bNameFromUnicode, 1,2,3,0},
   {"NearlyHvCps", bNearlyHvCps, 0,0,3,0},
   {"NearlyHvLines", bNearlyHvLines, 0,0,2,0},
   {"NearlyLines", bNearlyLines, 0,0,2,0},
   {"New", bNew, 1,1,1,0},
   {"NonLinearTransform", bNonLinearTransform, 0,3,3,0},
   {"Open", bOpen, 1,2,3,0},
   {"Ord", bOrd, 1,2,3,0},
   {"Outline", bOutline, 0,2,2,0},
   {"OverlapIntersect", bOverlapIntersect, 0,1,1,0},
   {"Paste", bPaste, 0,1,1,0},
   {"PasteInto", bPasteInto, 0,1,1,0},
   {"PasteWithOffset", bPasteWithOffset, 0,3,3,0},
   {"PositionReference", bPositionReference, 0,4,-1,0},
   {"PostNotice", bPostNotice, 1,2,2,0},
   {"Pow", bPow, 1,3,3,0},
   {"PreloadCidmap", bPreloadCidmap, 1,5,5,0},
   {"Print", bPrint, 1,0,-1,0},
   {"PrintFont", bPrintFont, 0,2,5,0},
   {"PrintSetup", bPrintSetup, 1,2,5,0},
   {"PrivateGuess", bPrivateGuess, 0,2,2,0},
   {"Quit", bQuit, 1,0,2,0},
   {"Rand", bRand, 1,1,1,0},
   {"RandReal", bRandReal, 1,1,1,0},
   {"ReadOtherSubrsFile", bReadOtherSubrsFile, 1,2,2,0},
   {"Real", bReal, 1,2,2,0},
   {"Reencode", bReencode, 0,2,3,0},
   {"RemoveAllKerns", bClearAllKerns, 0,1,1,0},
   {"RemoveAllVKerns", bClearAllVKerns, 0,1,1,0},
   {"RemoveAnchorClass", bRemoveAnchorClass, 0,2,2,0},
   {"RemoveDetachedGlyphs", bRemoveDetachedGlyphs, 0,0,-1,0},
   {"RemoveLookup", bRemoveLookup, 0,2,3,0},
   {"RemoveLookupSubtable", bRemoveLookupSubtable, 0,2,3,0},
   {"RemoveOverlap", bRemoveOverlap, 0,1,1,0},
   {"RemovePosSub", bRemovePosSub, 0,2,2,0},
   {"RemovePreservedTable", bRemovePreservedTable, 0,2,2,0},
   {"RenameGlyphs", bRenameGlyphs, 0,2,2,0},
   {"ReplaceCharCounterMasks", bReplaceCharCounterMasks, 0,2,2,0},
   {"ReplaceCvtAt", bReplaceCvtAt, 0,3,3,0},
   {"ReplaceGlyphCounterMasks", bReplaceCharCounterMasks, 0,2,2,0},
   {"ReplaceWithReference", bReplaceOutlineWithReference, 0,0,3,0},
   {"Revert", bRevert, 0,1,1,0},
   {"RevertToBackup", bRevertToBackup, 0,1,1,0},
   {"Rotate", bRotate, 0,1,4,0},
   {"Round", bRound, 1,2,2,0},
   {"RoundToCluster", bRoundToCluster, 0,0,3,0},
   {"RoundToInt", bRoundToInt, 0,1,2,0},
   {"SameGlyphAs", bSameGlyphAs, 0,1,1,0},
   {"Save", bSave, 0,0,3,0},
   {"SaveTableToFile", bSaveTableToFile, 0,3,3,0},
   {"Scale", bScale, 0,2,5,0},
   {"ScaleToEm", bScaleToEm, 0,2,3,0},
   {"Select", bSelect, 0,0,-1,0},
   {"SelectAll", bSelectAll, 0,1,1,0},
   {"SelectAllInstancesOf", bSelectAllInstancesOf, 0,0,-1,0},
   {"SelectBitmap", bSelectBitmap, 0,2,2,0},
   {"SelectByColor", bSelectByColor, 0,2,2,0},
   {"SelectByColour", bSelectByColor, 0,2,2,0},
   {"SelectByPosSub", bSelectByPosSub, 0,3,3,0},
   {"SelectChanged", bSelectChanged, 0,1,2,0},
   {"SelectFewer", bSelectFewer, 0,2,-1,0},
   {"SelectFewerSingletons", bSelectFewerSingletons, 0,2,-1,0},
   {"SelectGlyphsBoth", bSelectGlyphsBoth, 0,1,2,0},
   {"SelectGlyphsReferences", bSelectGlyphsReferences,0,1,2,0},
   {"SelectGlyphsSplines", bSelectGlyphsSplines, 0,1,2,0},
   {"SelectHintingNeeded", bSelectHintingNeeded, 0,1,2,0},
   {"SelectIf", bSelectIf, 0,0,-1,0},
   {"SelectInvert", bSelectInvert, 0,1,1,0},
   {"SelectMore", bSelectMore, 0,2,-1,0},
   {"SelectMoreIf", bSelectMoreIf, 0,2,-1,0},
   {"SelectMoreSingletons", bSelectMoreSingletons, 0,2,-1,0},
   {"SelectMoreSingletonsIf", bSelectMoreSingletonsIf, 0,2,-1,0},
   {"SelectNone", bSelectNone, 0,1,1,0},
   {"SelectSingletons", bSelectSingletons, 0,0,-1,0},
   {"SelectSingletonsIf", bSelectSingletonsIf, 0,0,-1,0},
   {"SelectWorthOutputting", bSelectWorthOutputting, 0,1,2,0},
   {"SetCharCnt", bSetCharCnt, 0,2,2,0},
   {"SetCharColor", bSetCharColor, 0,2,2,0},
   {"SetCharComment", bSetCharComment, 0,2,2,0},
   {"SetCharCounterMask", bSetCharCounterMask, 0,3,-1,0},
   {"SetCharName", bSetCharName, 0,2,3,0},
   {"SetFeatureList", bSetFeatureList, 0,3,3,0},
   {"SetFondName", bSetFondName, 0,2,2,0},
   {"SetFontHasVerticalMetrics", bSetFontHasVerticalMetrics, 0,2,2,0},
   {"SetFontNames", bSetFontNames, 0,2,7,0},
   {"SetFontOrder", bSetFontOrder, 0,2,2,0},
   {"SetGasp", bSetGasp, 0,0,-1,0},
   {"SetGlyphChanged", bSetGlyphChanged, 0,2,2,0},
   {"SetGlyphClass", bSetGlyphClass, 0,2,2,0},
   {"SetGlyphColor", bSetCharColor, 0,2,2,0},
   {"SetGlyphComment", bSetCharComment, 0,2,2,0},
   {"SetGlyphCounterMask", bSetCharCounterMask, 0,0,-1,0},
   {"SetGlyphName", bSetCharName, 0,0,-1,0},
   {"SetGlyphTeX", bSetGlyphTeX, 0,3,5,0},
   {"SetItalicAngle", bSetItalicAngle, 0,2,3,0},
   {"SetKern", bSetKern, 0,3,4,0},
   {"SetLBearing", bSetLBearing, 0,2,3,0},
   {"SetMacStyle", bSetMacStyle, 0,2,2,0},
   {"SetMaxpValue", bSetMaxpValue, 0,3,3,0},
   {"SetOS2Value", bSetOS2Value, 0,0,-1,0},
   {"SetPanose", bSetPanose, 0,2,3,0},
   {"SetPref", bSetPrefs, 1,3,4,0},
   {"SetRBearing", bSetRBearing, 0,2,3,0},
   {"SetTTFName", bSetTTFName, 0,4,4,0},
   {"SetTeXParams", bSetTeXParams, 0,0,-1,0},
   {"SetUnicodeValue", bSetUnicodeValue, 0,2,3,0},
   {"SetUniqueID", bSetUniqueID, 0,2,2,0},
   {"SetVKern", bSetVKern, 0,3,4,0},
   {"SetVWidth", bSetVWidth, 0,2,3,0},
   {"SetWidth", bSetWidth, 0,2,3,0},
   {"Shadow", bShadow, 0,4,4,0},
   {"Shell",bShell,1,2,2,0},
   {"Simplify", bSimplify, 0,0,-1,0},
   {"Sin", bSin, 1,2,2,0},
   {"SizeOf", bSizeOf, 1,2,2,0},
   {"Skew", bSkew, 0,2,5,0},
   {"SmallCaps", bSmallCaps, 0,0,5,0},
   {"Sqrt", bSqrt, 1,2,2,0},
   {"StrJoin", bStrJoin, 1,3,3,0},
   {"StrSplit", bStrSplit, 1,3,4,0},
   {"Strcasecmp", bStrcasecmp, 1,3,3,0},
   {"Strcasestr", bStrcasestr, 1,3,3,0},
   {"Strftime", bStrftime, 1,2,4,0},
   {"Strlen", bStrlen, 1,2,2,0},
   {"Strrstr", bStrrstr, 1,3,3,0},
   {"Strskipint", bStrskipint, 1,2,3,0},
   {"Strstr", bStrstr, 1,3,3,0},
   {"Strsub", bStrsub, 1,3,4,0},
   {"Strtod", bStrtod, 1,2,2,0},
   {"Strtol", bStrtol, 1,2,3,0},
   {"SubstitutionPoints", bSubstitutionPoints, 0,1,1,0},
   {"Tan", bTan, 1,2,2,0},
   {"ToLower", btolower, 1,2,2,0},
   {"ToMirror", btomirror, 1,2,2,0},
   {"ToString", bToString, 1,2,2,0},
   {"ToUpper", btoupper, 1,2,2,0},
   {"Transform", bTransform, 0,7,7,0},
   {"TypeOf", bTypeOf, 1,2,2,0},
   {"UCodePoint", bUCodePoint, 1,2,2,0},
   {"Ucs4", bUCS4, 1,2,2,0},
   {"UnicodeAnnotationFromLib", bUnicodeAnnotationFromLib, 1,2,2,0},
   {"UnicodeBlockEndFromLib", bUnicodeBlockEndFromLib, 1,2,2,0},
   {"UnicodeBlockNameFromLib", bUnicodeBlockNameFromLib, 1,2,2,0},
   {"UnicodeBlockStartFromLib", bUnicodeBlockStartFromLib, 1,2,2,0},
   {"UnicodeFromName", bUnicodeFromName, 1,2,2,0},
   {"UnicodeNameFromLib", bUnicodeNameFromLib, 1,2,2,0},
   {"UnicodeNamesListVersion", bUnicodeNamesListVersion, 1,1,1,0},
   {"UnlinkReference", bUnlinkReference, 0,1,1,0},
   {"Utf8", bUtf8, 1,2,2,0},
   {"VFlip", bVFlip, 0,1,2,0},
   {"VKernFromHKern", bVKernFromHKern, 0,1,1,0},
   {"Validate", bValidate, 0,0,2,0},
   {"Wireframe", bWireframe, 0,4,4,0},
   {"WorthOutputting", bWorthOutputting, 0,1,2,0},
   {"WriteStringToFile", bWriteStringToFile, 1,3,4,0},
   {"WritePfm", bWritePfm, 0,2,2,0},
   {NULL, 0, 0,0,-1,0},
};

static void bGetCoverageCounts(Context *c) {
   int i;

   afputs("-----BEGIN FONTANVIL COVERAGE COUNTS-----\n",astdout);
   for (i=0;builtins[i].name;i++) {
      afprintf(astdout,"%-30s %20lu\n",builtins[i].name,builtins[i].count);
   }
   afputs("-----END FONTANVIL COVERAGE COUNTS-----\n",astdout);
}

/* ******************************* Interpreter ****************************** */

static void expr(Context *,Val *val);

void ff_FlushScanner(void);

static void cseek(Context *c,long pos) {
   afseek(c->script, pos, SEEK_SET);
   c->backedup=false;
   ff_FlushScanner();
}

enum token_type ff_NextToken(Context *c);

static void ff_backuptok(Context *c) {
   if (c->backedup)
      ErrorMsg(2,"%s:%d Internal Error: Attempt to back token twice\n",
	     c->filename, c->lineno);
   c->backedup=true;
}

#   define PE_ARG_MAX	25

static void docall(Context *c,char *name,Val *val) {
   /* Be prepared for c->donteval */
   Val args[PE_ARG_MAX];
   Array *dontfree[PE_ARG_MAX];
   int i;
   enum token_type tok;
   Context sub;
   struct builtins *found;

   tok=ff_NextToken(c);
   dontfree[0]=NULL;
   if (tok==tt_rparen)
      i=1;
   else {
      ff_backuptok(c);
      for (i=1; tok != tt_rparen; ++i) {
	 if (i >= PE_ARG_MAX)
	    ScriptError(c, "Too many arguments");
	 expr(c, &args[i]);
	 tok=ff_NextToken(c);
	 if (tok != tt_comma)
	    expect(c, tt_rparen, tok);
	 dontfree[i]=NULL;
      }
   }

   memset(&sub, 0, sizeof(sub));
   if (!c->donteval) {
      args[0].type=v_str;
      args[0].u.sval=name;
      sub.caller=c;
      sub.a.vals=args;
      sub.a.argc=i;
      sub.return_val.type=v_void;
      sub.filename=name;
      sub.curfv=c->curfv;
      sub.trace=c->trace;
      sub.dontfree=dontfree;
      for (i=0; i < sub.a.argc; ++i) {
	 dereflvalif(&args[i]);
	 if (args[i].type==v_arrfree)
	    args[i].type=v_arr;
	 else if (args[i].type==v_arr)
	    dontfree[i]=args[i].u.aval;
      }

      if (c->trace.u.ival) {
	 printf("%s:%d Calling %s(", GFileNameTail(c->filename), c->lineno,
		name);
	 for (i=1; i < sub.a.argc; ++i) {
	    if (i != 1)
	       putchar(',');
	    if (args[i].type==v_int)
	       printf("%d", args[i].u.ival);
	    else if (args[i].type==v_unicode)
	       printf("0u%x", args[i].u.ival);
	    else if (args[i].type==v_real)
	       printf("%g", (double) args[i].u.fval);
	    else if (args[i].type==v_str)
	       printf("\"%s\"", args[i].u.sval);
	    else if (args[i].type==v_void)
	       printf("<void>");
	    else
	       printf("<" "???" ">");
	 }
	 printf(")\n");
      }

      found=NULL;
      for (i=0; builtins[i].name != NULL; ++i)
	 if (strcmp(builtins[i].name, name)==0) {
	    found=&builtins[i];
	    break;
	 }
      if (found != NULL) {
	 if (verbose > 0)
	    fflush(stdout);
	 if (sub.curfv==NULL && !found->nofontok)
	    ScriptError(&sub, "This command requires an active font");
         if ((sub.a.argc<found->minargs) ||
	     ((found->maxargs>=0) && (sub.a.argc>found->maxargs)))
	   ScriptError(c, "Wrong number of arguments");
	 found->count++;
	 (found->func) (&sub);

      } else {
	 if (strchr(name, '/')==NULL && strchr(c->filename, '/') != NULL) {
	    char *pt;

	    sub.filename=malloc(strlen(c->filename) + strlen(name) + 4);
	    strcpy(sub.filename, c->filename);
	    pt=strrchr(sub.filename, '/');
	    strcpy(pt + 1, name);
	 }
	 sub.script=afopen(sub.filename, "r");
	 if (sub.script==NULL) {
	    char *pt;

	    if (sub.filename==name) {
	       sub.filename=malloc(strlen(name) + 4);
	       strcpy(sub.filename, name);
	    }
	    pt=sub.filename + strlen(sub.filename);
	    strcpy(pt, ".ff");
	    sub.script=afopen(sub.filename, "r");
	    if (sub.script==NULL) {
	       strcpy(pt, ".pe");
	       sub.script=afopen(sub.filename, "r");
	    }
	    if (sub.script==NULL) {
	       *pt='\0';
	    }
	 }
	 sub.script=afopen(sub.filename, "r");
	 if (sub.script==NULL) {
	    ScriptErrorString(c, "No built-in function or script file", name);
	 } else {
	    sub.lineno=1;
	    while (!sub.returned && !sub.broken
		   && (tok=ff_NextToken(&sub)) != tt_eof) {
	       ff_backuptok(&sub);
	       ff_statement(&sub);
	    }
	    afclose(sub.script);
	    sub.script=NULL;
	 }
	 if (sub.filename != name)
	    free(sub.filename);
      }
      c->curfv=sub.curfv;
      calldatafree(&sub);
   } else
      sub.return_val.type=v_void;
   if (val->type==v_str)
      free(val->u.sval);
   *val=sub.return_val;
}

static void buildarray(Context *c,Val *val) {
   /* Be prepared for c->donteval */
   Val *body=NULL;
   int cnt, max=0;
   int tok;

   tok=ff_NextToken(c);
   if (tok==tt_rbracket)
      /* ScriptError(c,"This array doesn't have any elements"); */
      cnt=0;
   else {
      ff_backuptok(c);
      max=0;
      body=NULL;
      for (cnt=0; tok != tt_rbracket; ++cnt) {
	 if (cnt >= max)
	    body=realloc(body, (max += 20) * sizeof(Val));
	 expr(c, &body[cnt]);
	 dereflvalif(&body[cnt]);
	 tok=ff_NextToken(c);
	 if (tok != tt_comma)
	    expect(c, tt_rbracket, tok);
      }
   }
   if (c->donteval) {
      free(body);
      val->type=v_void;
   } else {
      val->type=v_arrfree;
      val->u.aval=malloc(sizeof(Array));
      val->u.aval->argc=cnt;
      val->u.aval->vals=realloc(body, cnt * sizeof(Val));
   }
}

static void handlename(Context *c,Val *val) {
   char name[TOK_MAX + 1];
   enum token_type tok;
   int temp;
   char *pt;
   SplineFont *sf;

   strcpy(name, c->tok_text);
   val->type=v_void;
   tok=ff_NextToken(c);
   if (tok==tt_lparen) {
      docall(c, name, val);
   } else if (c->donteval) {
      ff_backuptok(c);
   } else {
      if (*name=='$') {
	 if (isdigit(name[1])) {
	    temp=0;
	    for (pt=name + 1; isdigit(*pt); ++pt)
	       temp=10 * temp + *pt - '0';
	    if (*pt=='\0' && temp < c->a.argc) {
	       val->type=v_lval;
	       val->u.lval=&c->a.vals[temp];
	    }
	 } else if (strcmp(name, "$argc")==0 || strcmp(name, "$#")==0) {
	    val->type=v_int;
	    val->u.ival=c->a.argc;
	 } else if (strcmp(name, "$argv")==0) {
	    val->type=v_arr;
	    val->u.aval=&c->a;
	 } else if (strcmp(name, "$curfont")==0
		    || strcmp(name, "$nextfont")==0
		    || strcmp(name, "$firstfont")==0) {
	    char *t;

	    if (strcmp(name, "$firstfont")==0) {
	       if (FontViewFirst()==NULL)
		  sf=NULL;
	       else
		  sf=FontViewFirst()->sf;
	    } else {
	       if (c->curfv==NULL)
		  ScriptError(c, "No current font");
	       if (strcmp(name, "$curfont")==0)
		  sf=c->curfv->sf;
	       else {
		  if (c->curfv->next==NULL)
		     sf=NULL;
		  else
		     sf=c->curfv->next->sf;
	       }
	    }
	    val->type=v_str;
	    t=fastrdup(sf==NULL ? "" :
			      sf->filename !=
			      NULL ? sf->filename : sf->origname);
	    val->u.sval=utf82script_copy(t);
	    free(t);
	 } else if (strcmp(name, "$mmcount")==0) {
	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    if (c->curfv->sf->mm==NULL)
	       val->u.ival=0;
	    else
	       val->u.ival=c->curfv->sf->mm->instance_count;
	    val->type=v_int;
	 } else if (strcmp(name, "$macstyle")==0
		    || strcmp(name, "$order")==0 || strcmp(name, "$em")==0
		    || strcmp(name, "$ascent")==0
		    || strcmp(name, "$descent")==0) {
	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    if (strcmp(name, "$macstyle")==0)
	       val->u.ival=c->curfv->sf->macstyle;
	    else if (strcmp(name, "$order")==0)
	       val->u.ival=c->curfv->sf->layers[ly_fore].order2 ? 2 : 3;
	    else if (strcmp(name, "$em")==0)
	       val->u.ival=c->curfv->sf->ascent + c->curfv->sf->descent;
	    else if (strcmp(name, "$ascent")==0)
	       val->u.ival=c->curfv->sf->ascent;
	    else		/* if ( strcmp(name,"$descent")==0 ) */
	       val->u.ival=c->curfv->sf->descent;
	    val->type=v_int;
	 } else if (strcmp(name, "$curcid")==0
		    || strcmp(name, "$nextcid")==0
		    || strcmp(name, "$firstcid")==0) {
	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    if (c->curfv->sf->cidmaster==NULL)
	       ScriptError(c, "Not a cid keyed font");
	    if (strcmp(name, "$firstcid")==0) {
	       sf=c->curfv->sf->cidmaster->subfonts[0];
	    } else {
	       sf=c->curfv->sf;
	       if (strcmp(name, "$nextcid")==0) {
		  int i;

		  for (i=0; i < sf->cidmaster->subfontcnt &&
		       sf->cidmaster->subfonts[i] != sf; ++i);
		  if (i >= sf->cidmaster->subfontcnt - 1)
		     sf=NULL;	/* No next */
		  else
		     sf=sf->cidmaster->subfonts[i + 1];
	       }
	    }
	    val->type=v_str;
	    val->u.sval=fastrdup(sf==NULL ? "" : sf->fontname);
	 } else if (strcmp(name, "$fontname")==0
		    || strcmp(name, "$familyname")==0
		    || strcmp(name, "$fullname")==0
		    || strcmp(name, "$weight")==0
		    || strcmp(name, "$copyright")==0
		    || strcmp(name, "$filename")==0
		    || strcmp(name, "$fontversion")==0
		    || strcmp(name, "$fondname")==0) {
	    char *t;

	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    val->type=v_str;
	    t=fastrdup(strcmp(name, "$fontname")==0 ? c->curfv->sf->fontname :
		     name[2]=='a' ? c->curfv->sf->familyname :
		     name[2]=='u' ? c->curfv->sf->fullname :
		     name[2]=='e' ? c->curfv->sf->weight :
		     name[2]=='i' ? c->curfv->sf->origname :
		     strcmp(name, "$fondname")==0 ? c->curfv->sf->fondname :
		     name[3]=='p' ? c->curfv->sf->copyright :
		     c->curfv->sf->version);
	    val->u.sval=utf82script_copy(t);
	    if (val->u.sval==NULL)
	       val->u.sval=fastrdup("");
	    free(t);
	 } else if (strcmp(name, "$iscid")==0) {
	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    val->type=v_int;
	    val->u.ival=(c->curfv->sf->cidmaster != NULL);
	 } else if (strcmp(name, "$cidfontname")==0
		    || strcmp(name, "$cidfamilyname")==0
		    || strcmp(name, "$cidfullname")==0
		    || strcmp(name, "$cidweight")==0
		    || strcmp(name, "$cidcopyright")==0) {
	    char *t;

	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    val->type=v_str;
	    if (c->curfv->sf->cidmaster==NULL)
	       val->u.sval=fastrdup("");
	    else {
	       SplineFont *sf=c->curfv->sf->cidmaster;

	       t=fastrdup(strcmp(name, "$cidfontname")==0 ? sf->fontname :
			name[5]=='a' ? sf->familyname :
			name[5]=='u' ? sf->fullname :
			name[5]=='e' ? sf->weight : sf->copyright);
	       val->u.sval=utf82script_copy(t);
	       free(t);
	    }
	 } else if (strcmp(name, "$italicangle")==0) {
	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    val->type=v_real;
	    val->u.fval=c->curfv->sf->italicangle;
	 } else if (strcmp(name, "$fontchanged")==0) {
	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    val->type=v_int;
	    val->u.ival=c->curfv->sf->changed;
	 } else if (strcmp(name, "$loadState")==0) {
	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    val->type=v_int;
	    val->u.ival=c->curfv->sf->loadvalidation_state;
	 } else if (strcmp(name, "$privateState")==0) {
	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    val->type=v_int;
	    val->u.ival=ValidatePrivate(c->curfv->sf);
	 } else if (strcmp(name, "$bitmaps")==0) {
	    SplineFont *sf;
	    BDFFont *bdf;
	    int cnt;

	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    sf=c->curfv->sf;
	    if (sf->cidmaster != NULL)
	       sf=sf->cidmaster;
	    for (cnt=0, bdf=sf->bitmaps; bdf != NULL; bdf=bdf->next)
	       ++cnt;
	    val->type=v_arrfree;
	    val->u.aval=malloc(sizeof(Array));
	    val->u.aval->argc=cnt;
	    val->u.aval->vals=malloc((cnt + 1) * sizeof(Val));
	    for (cnt=0, bdf=sf->bitmaps; bdf != NULL;
		 bdf=bdf->next, ++cnt) {
	       val->u.aval->vals[cnt].type=v_int;
	       val->u.aval->vals[cnt].u.ival=bdf->pixelsize;
	       if (bdf->clut != NULL)
		  val->u.aval->vals[cnt].u.ival |= BDFDepth(bdf) << 16;
	    }
	 } else if (strcmp(name, "$panose")==0) {
	    SplineFont *sf;
	    struct pfminfo pfminfo;
	    int cnt;

	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    sf=c->curfv->sf;
	    if (sf->cidmaster != NULL)
	       sf=sf->cidmaster;
	    val->type=v_arrfree;
	    val->u.aval=malloc(sizeof(Array));
	    val->u.aval->argc=10;
	    val->u.aval->vals=malloc((10 + 1) * sizeof(Val));
	    SFDefaultOS2Info(&pfminfo, sf, sf->fontname);
	    for (cnt=0; cnt < 10; ++cnt) {
	       val->u.aval->vals[cnt].type=v_int;
	       val->u.aval->vals[cnt].u.ival=pfminfo.panose[cnt];
	    }
	 } else if (strcmp(name, "$selection")==0) {
	    EncMap *map;
	    int i;

	    if (c->curfv==NULL)
	       ScriptError(c, "No current font");
	    map=c->curfv->map;
	    val->type=v_arrfree;
	    val->u.aval=malloc(sizeof(Array));
	    val->u.aval->argc=map->enccount;
	    val->u.aval->vals=malloc((map->enccount + 1) * sizeof(Val));
	    for (i=0; i < map->enccount; ++i) {
	       val->u.aval->vals[i].type=v_int;
	       val->u.aval->vals[i].u.ival=c->curfv->selected[i];
	    }
	 } else if (strcmp(name, "$trace")==0) {
	    val->type=v_lval;
	    val->u.lval=&c->trace;
	 } else if (strcmp(name, "$version")==0) {
	    val->type=v_str;
	    sprintf(name, "%d", FONTANVIL_VERSIONDATE_RAW);
	    val->u.sval=fastrdup(name);
	 } else if (strcmp(name, "$haspython")==0) {
	    val->type=v_int;
	    val->u.ival=0;
	 } else if (GetPrefs(name + 1, val)) {
	    /* Done */
	 }
      } else if (*name=='@') {
	 if (c->curfv==NULL)
	    ScriptError(c, "No current font");
	 DicaLookup(c->curfv->fontvars, name, val);
      } else if (*name=='_') {
	 DicaLookup(&globals, name, val);
      } else {
	 DicaLookup(&c->locals, name, val);
      }
      if (tok==tt_assign && val->type==v_void && *name != '$') {
	 /* It's ok to create this as a new variable, we're going to assign to it */
	 if (*name=='@') {
	    if (c->curfv->fontvars==NULL)
	       c->curfv->fontvars=calloc(1, sizeof(struct dictionary));
	    DicaNewEntry(c->curfv->fontvars, name, val);
	 } else if (*name=='_') {
	    DicaNewEntry(&globals, name, val);
	 } else {
	    DicaNewEntry(&c->locals, name, val);
	 }
      }
      if (val->type==v_void)
	 ScriptErrorString(c, "Undefined variable", name);
      ff_backuptok(c);
   }
}

static void term(Context *c,Val *val) {
   enum token_type tok=ff_NextToken(c);
   Val temp;

   if (tok==tt_lparen) {
      expr(c, val);
      tok=ff_NextToken(c);
      expect(c, tt_rparen, tok);
   } else if (tok==tt_lbracket) {
      buildarray(c, val);
   } else if (tok==tt_number || tok==tt_unicode || tok==tt_real) {
      *val=c->tok_val;
   } else if (tok==tt_string) {
      val->type=v_str;
      val->u.sval=fastrdup(c->tok_text);
   } else if (tok==tt_name) {
      handlename(c, val);
   } else if (tok==tt_minus || tok==tt_plus || tok==tt_not
	      || tok==tt_bitnot) {
      term(c, val);
      if (!c->donteval) {
	 dereflvalif(val);
	 if (val->type==v_real) {
	    if (tok==tt_minus)
	       val->u.fval=-val->u.fval;
	    else
	       ScriptError(c, "Invalid type in integer expression");
	 } else if (val->type==v_int) {
	    if (tok==tt_minus)
	       val->u.ival=-val->u.ival;
	    else if (tok==tt_not)
	       val->u.ival=!val->u.ival;
	    else if (tok==tt_bitnot)
	       val->u.ival=~val->u.ival;
	 } else
	    ScriptError(c, "Invalid type in integer expression");
      }
   } else if (tok==tt_incr || tok==tt_decr) {
      term(c, val);
      if (!c->donteval) {
	 if (val->type != v_lval)
	    ScriptError(c, "Expected lvalue");
	 if (val->u.lval->type==v_real) {
	    if (tok==tt_incr)
	       ++val->u.lval->u.fval;
	    else
	       --val->u.lval->u.fval;
	 } else if (val->u.lval->type==v_int
		    || val->u.lval->type==v_unicode) {
	    if (tok==tt_incr)
	       ++val->u.lval->u.ival;
	    else
	       --val->u.lval->u.ival;
	 } else
	    ScriptError(c, "Invalid type in integer expression");
	 dereflvalif(val);
      }
   } else
      expect(c, tt_name, tok);

   tok=ff_NextToken(c);
   while (tok==tt_incr || tok==tt_decr || tok==tt_colon
	  || tok==tt_lparen || tok==tt_lbracket) {
      if (tok==tt_colon) {
	 if (c->donteval) {
	    tok=ff_NextToken(c);
	    expect(c, tt_name, tok);
	 } else {
	    dereflvalif(val);
	    if (val->type != v_str)
	       ScriptError(c, "Invalid type in string expression");
	    else {
	       char *pt, *ept;

	       tok=ff_NextToken(c);
	       expect(c, tt_name, tok);
	       if (strcmp(c->tok_text, "h")==0) {
		  pt=strrchr(val->u.sval, '/');
		  if (pt != NULL)
		     *pt='\0';
	       } else if (strcmp(c->tok_text, "t")==0) {
		  pt=strrchr(val->u.sval, '/');
		  if (pt != NULL) {
		     char *ret=fastrdup(pt + 1);

		     free(val->u.sval);
		     val->u.sval=ret;
		  }
	       } else if (strcmp(c->tok_text, "r")==0) {
		  pt=strrchr(val->u.sval, '/');
		  if (pt==NULL)
		     pt=val->u.sval;
		  ept=strrchr(pt, '.');
		  if (ept != NULL)
		     *ept='\0';
	       } else if (strcmp(c->tok_text, "e")==0) {
		  pt=strrchr(val->u.sval, '/');
		  if (pt==NULL)
		     pt=val->u.sval;
		  ept=strrchr(pt, '.');
		  if (ept != NULL) {
		     char *ret=fastrdup(ept + 1);

		     free(val->u.sval);
		     val->u.sval=ret;
		  }
	       } else
		  ScriptErrorString(c, "Unknown colon substitution",
				    c->tok_text);
	    }
	 }
      } else if (tok==tt_lparen) {
	 if (c->donteval) {
	    docall(c, NULL, val);
	 } else {
	    dereflvalif(val);
	    if (val->type != v_str) {
	       ScriptError(c,
			   "Expected string to hold filename in procedure call");
	    } else
	       docall(c, val->u.sval, val);
	 }
      } else if (tok==tt_lbracket) {
	 expr(c, &temp);
	 tok=ff_NextToken(c);
	 expect(c, tt_rbracket, tok);
	 if (!c->donteval) {
	    dereflvalif(&temp);
	    if (val->type==v_lval
		&& (val->u.lval->type==v_arr
		    || val->u.lval->type==v_arrfree))
	       *val=*val->u.lval;
	    if (val->type != v_arr && val->type != v_arrfree)
	       ScriptError(c, "Array required");
	    if (temp.type != v_int)
	       ScriptError(c, "Integer expression required in array");
	    else if (temp.u.ival < 0 || temp.u.ival >= val->u.aval->argc)
	       ScriptError(c, "Integer expression out of bounds in array");
	    else if (val->type==v_arrfree) {
	       temp=val->u.aval->vals[temp.u.ival];
	       arrayfree(val->u.aval);
	       *val=temp;
	    } else {
	       val->type=v_lval;
	       val->u.lval=&val->u.aval->vals[temp.u.ival];
	    }
	 }
      } else if (!c->donteval) {
	 Val temp;

	 if (val->type != v_lval)
	    ScriptError(c, "Expected lvalue");
	 temp=*val->u.lval;
	 if (val->u.lval->type==v_real) {
	    if (tok==tt_incr)
	       ++val->u.lval->u.fval;
	    else
	       --val->u.lval->u.fval;
	 } else if (val->u.lval->type==v_int
		    || val->u.lval->type==v_unicode) {
	    if (tok==tt_incr)
	       ++val->u.lval->u.ival;
	    else
	       --val->u.lval->u.ival;
	 } else
	    ScriptError(c, "Invalid type in integer expression");
	 *val=temp;
      }
      tok=ff_NextToken(c);
   }
   ff_backuptok(c);
}

static void mul(Context *c,Val *val) {
   Val other;
   enum token_type tok;

   term(c, val);
   tok=ff_NextToken(c);
   while (tok==tt_mul || tok==tt_div || tok==tt_mod) {
      other.type=v_void;
      term(c, &other);
      if (!c->donteval) {
	 dereflvalif(val);
	 dereflvalif(&other);
	 if (val->type==v_int && other.type==v_int) {
	    if ((tok==tt_div || tok==tt_mod) && other.u.ival==0)
	       ScriptError(c, "Division by zero");
	    else if (tok==tt_mul)
	       val->u.ival *= other.u.ival;
	    else if (tok==tt_mod)
	       val->u.ival %= other.u.ival;
	    else
	       val->u.ival /= other.u.ival;
	 } else if ((val->type==v_real || val->type==v_int) &&
		    (other.type==v_real || other.type==v_int)) {
	    if (val->type==v_int) {
	       val->type=v_real;
	       val->u.fval=val->u.ival;
	    }
	    if (other.type==v_int)
	       other.u.fval=other.u.ival;
	    if ((tok==tt_div || tok==tt_mod) && other.u.fval==0)
	       ScriptError(c, "Division by zero");
	    else if (tok==tt_mul)
	       val->u.fval *= other.u.fval;
	    else if (tok==tt_mod)
	       val->u.fval=fmod(val->u.fval, other.u.fval);
	    else
	       val->u.fval /= other.u.fval;
	 } else
	    ScriptError(c, "Invalid type in integer expression");
      }
      tok=ff_NextToken(c);
   }
   ff_backuptok(c);
}

static void add(Context *c,Val *val) {
   Val other;
   enum token_type tok;

   mul(c, val);
   tok=ff_NextToken(c);
   while (tok==tt_plus || tok==tt_minus) {
      other.type=v_void;
      mul(c, &other);
      if (!c->donteval) {
	 dereflvalif(val);
	 dereflvalif(&other);
	 if (val->type==v_str
	     && (other.type==v_str || other.type==v_int || other.type==v_real)
	     && tok==tt_plus) {
	    char *ret, *temp;
	    char buffer[24];

	    if (other.type==v_int) {
	       sprintf(buffer, "%d", other.u.ival);
	       temp=buffer;
	    } else if (other.type==v_real) {
	       sprintf(buffer, "%g", other.u.fval);
	       temp=buffer;
	    } else
	       temp=other.u.sval;
	    ret=malloc(strlen(val->u.sval) + strlen(temp) + 1);
	    strcpy(ret, val->u.sval);
	    strcat(ret, temp);
	    if (other.type==v_str)
	       free(other.u.sval);
	    free(val->u.sval);
	    val->u.sval=ret;
	 } else if (val->type==v_arr || val->type==v_arrfree) {
	    Array *arr;

	    arr=malloc(sizeof(Array));
	    arr->argc=val->u.aval->argc +
	       ((other.type==v_arr || other.type==v_arrfree) ?
		other.u.aval->argc : 1);
	    arr->vals=malloc(arr->argc * sizeof(Val));
	    array_copy_into(arr, 0, val->u.aval);
	    if (other.type==v_arr || other.type==v_arrfree) {
	       array_copy_into(arr, val->u.aval->argc, other.u.aval);
	       if (other.type==v_arrfree)
		  arrayfree(other.u.aval);
	    } else {
	       arr->vals[val->u.aval->argc]=other;
	       /* can't be an array */
	       /* don't need to copy a string, we'd just free it */
	    }
	    if (val->type==v_arrfree)
	       arrayfree(val->u.aval);
	    val->u.aval=arr;
	    val->type=v_arrfree;
	 } else if ((val->type==v_int || val->type==v_unicode) &&
		    (other.type==v_int || other.type==v_unicode)) {
	    if (tok==tt_plus)
	       val->u.ival += other.u.ival;
	    else
	       val->u.ival -= other.u.ival;
	 } else if ((val->type==v_real || val->type==v_int) &&
		    (other.type==v_real || other.type==v_int)) {
	    if (val->type==v_int) {
	       val->type=v_real;
	       val->u.fval=val->u.ival;
	    }
	    if (other.type==v_int)
	       other.u.fval=other.u.ival;
	    if (tok==tt_plus)
	       val->u.fval += other.u.fval;
	    else
	       val->u.fval -= other.u.fval;
	 } else
	    ScriptError(c, "Invalid type in integer expression");
      }
      tok=ff_NextToken(c);
   }
   ff_backuptok(c);
}

static void comp(Context *c,Val *val) {
   Val other;
   int cmp;
   enum token_type tok;

   add(c, val);
   tok=ff_NextToken(c);
   while (tok==tt_eq || tok==tt_ne || tok==tt_gt || tok==tt_lt
	  || tok==tt_ge || tok==tt_le) {
      other.type=v_void;
      add(c, &other);
      if (!c->donteval) {
	 dereflvalif(val);
	 dereflvalif(&other);
	 if (val->type==v_str && other.type==v_str) {
	    cmp=strcmp(val->u.sval, other.u.sval);
	    free(val->u.sval);
	    free(other.u.sval);
	 } else if ((val->type==v_int || val->type==v_unicode) &&
		    (other.type==v_int || other.type==v_unicode)) {
	    cmp=val->u.ival - other.u.ival;
	 } else if ((val->type==v_real || val->type==v_int) &&
		    (other.type==v_real || other.type==v_int)) {
	    if (val->type==v_int)
	       val->u.fval=val->u.ival;
	    if (other.type==v_int)
	       other.u.fval=other.u.ival;
	    if (val->u.fval > other.u.fval)
	       cmp=1;
	    else if (val->u.fval < other.u.fval)
	       cmp=-1;
	    else
	       cmp=0;
	 } else
	    ScriptError(c, "Invalid type in integer expression");
	 val->type=v_int;
	 if (tok==tt_eq)
	    val->u.ival=(cmp==0);
	 else if (tok==tt_ne)
	    val->u.ival=(cmp != 0);
	 else if (tok==tt_gt)
	    val->u.ival=(cmp > 0);
	 else if (tok==tt_lt)
	    val->u.ival=(cmp < 0);
	 else if (tok==tt_ge)
	    val->u.ival=(cmp >= 0);
	 else if (tok==tt_le)
	    val->u.ival=(cmp <= 0);
      }
      tok=ff_NextToken(c);
   }
   ff_backuptok(c);
}

static void _and(Context *c,Val *val) {
   Val other;
   int old=c->donteval;
   enum token_type tok;

   comp(c, val);
   tok=ff_NextToken(c);
   while (tok==tt_and || tok==tt_bitand) {
      other.type=v_void;
      if (!c->donteval)
	 dereflvalif(val);
      if (tok==tt_and && val->u.ival==0)
	 c->donteval=true;
      comp(c, &other);
      c->donteval=old;
      if (!old) {
	 dereflvalif(&other);
	 if (tok==tt_and && val->type==v_int && val->u.ival==0)
	    val->u.ival=0;
	 else if (val->type != v_int || other.type != v_int)
	    ScriptError(c, "Invalid type in integer expression");
	 else if (tok==tt_and)
	    val->u.ival=val->u.ival && other.u.ival;
	 else
	    val->u.ival &= other.u.ival;
      }
      tok=ff_NextToken(c);
   }
   ff_backuptok(c);
}

static void _or(Context *c,Val *val) {
   Val other;
   int old=c->donteval;
   enum token_type tok;

   _and(c, val);
   tok=ff_NextToken(c);
   while (tok==tt_or || tok==tt_bitor || tok==tt_xor) {
      other.type=v_void;
      if (!c->donteval)
	 dereflvalif(val);
      if (tok==tt_or && val->u.ival != 0)
	 c->donteval=true;
      _and(c, &other);
      c->donteval=old;
      if (!c->donteval) {
	 dereflvalif(&other);
	 if (tok==tt_or && val->type==v_int && val->u.ival != 0)
	    val->u.ival=1;
	 else if (val->type != v_int || other.type != v_int)
	    ScriptError(c, "Invalid type in integer expression");
	 else if (tok==tt_or)
	    val->u.ival=val->u.ival || other.u.ival;
	 else if (tok==tt_bitor)
	    val->u.ival |= other.u.ival;
	 else
	    val->u.ival ^= other.u.ival;
      }
      tok=ff_NextToken(c);
   }
   ff_backuptok(c);
}

static void assign(Context *c,Val *val) {
   Val other;
   enum token_type tok;

   _or(c, val);
   tok=ff_NextToken(c);
   if (tok==tt_assign || tok==tt_pluseq || tok==tt_minuseq
       || tok==tt_muleq || tok==tt_diveq || tok==tt_modeq) {
      other.type=v_void;
      assign(c, &other);	/* that's the evaluation order here */
      if (!c->donteval) {
	 dereflvalif(&other);
	 if (val->type != v_lval)
	    ScriptError(c, "Expected lvalue");
	 else if (other.type==v_void)
	    ScriptError(c, "Void found on right side of assignment");
	 else if (tok==tt_assign) {
	    Val temp;
	    int argi;

	    temp=*val->u.lval;
	    *val->u.lval=other;
	    if (other.type==v_arr)
	       val->u.lval->u.aval=arraycopy(other.u.aval);
	    else if (other.type==v_arrfree)
	       val->u.lval->type=v_arr;
	    argi=val->u.lval - c->a.vals;
	    /* Have to free things after we copy them */
	    if (argi >= 0 && argi < c->a.argc && temp.type==v_arr &&
		temp.u.aval==c->dontfree[argi])
	       c->dontfree[argi]=NULL;	/* Don't free it */
	    else if (temp.type==v_arr)
	       arrayfree(temp.u.aval);
	    else if (temp.type==v_str)
	       free(temp.u.sval);
	 } else
	    if ((val->u.lval->type==v_int || val->u.lval->type==v_unicode)
		&& (other.type==v_int || other.type==v_unicode
		    || other.type==v_real)) {
	    if (other.type==v_real)
	       other.u.ival=rint(other.u.fval);
	    if (tok==tt_pluseq)
	       val->u.lval->u.ival += other.u.ival;
	    else if (tok==tt_minuseq)
	       val->u.lval->u.ival -= other.u.ival;
	    else if (tok==tt_muleq)
	       val->u.lval->u.ival *= other.u.ival;
	    else if (other.u.ival==0)
	       ScriptError(c, "Divide by zero");
	    else if (tok==tt_modeq)
	       val->u.lval->u.ival %= other.u.ival;
	    else
	       val->u.lval->u.ival /= other.u.ival;
	 } else if (val->u.lval->type==v_real &&
		    (other.type==v_int || other.type==v_real)) {
	    if (other.type==v_int)
	       other.u.fval=other.u.ival;
	    if (tok==tt_pluseq)
	       val->u.lval->u.fval += other.u.fval;
	    else if (tok==tt_minuseq)
	       val->u.lval->u.fval -= other.u.fval;
	    else if (tok==tt_muleq)
	       val->u.lval->u.fval *= other.u.fval;
	    else if (other.u.fval==0)
	       ScriptError(c, "Divide by zero");
	    else if (tok==tt_modeq)
	       val->u.lval->u.fval=fmod(val->u.lval->u.fval, other.u.fval);
	    else
	       val->u.lval->u.fval /= other.u.fval;
	 } else if (tok==tt_pluseq && val->u.lval->type==v_str &&
		    (other.type==v_str || other.type==v_int
		     || other.type==v_real)) {
	    char *ret, *temp;
	    char buffer[20];

	    if (other.type==v_int) {
	       sprintf(buffer, "%d", other.u.ival);
	       temp=buffer;
	    } else if (other.type==v_real) {
	       sprintf(buffer, "%g", (double) other.u.fval);
	       temp=buffer;
	    } else
	       temp=other.u.sval;
	    ret=malloc(strlen(val->u.lval->u.sval) + strlen(temp) + 1);
	    strcpy(ret, val->u.lval->u.sval);
	    strcat(ret, temp);
	    if (other.type==v_str)
	       free(other.u.sval);
	    free(val->u.lval->u.sval);
	    val->u.lval->u.sval=ret;
	 } else
	    ScriptError(c, "Invalid types in assignment");
      }
   } else
      ff_backuptok(c);
}

static void expr(Context *c,Val *val) {
   val->type=v_void;
   assign(c, val);
}

static void doforeach(Context *c) {
   long here=aftell(c->script);
   int lineno=c->lineno;
   enum token_type tok;
   int i, selsize;
   char *sel;
   int nest;

   if (c->curfv==NULL)
      ScriptError(c, "foreach requires an active font");
   selsize=c->curfv->map->enccount;
   sel=malloc(selsize);
   memcpy(sel, c->curfv->selected, selsize);
   memset(c->curfv->selected, 0, selsize);
   i=0;

   c->broken=false;
   while (1) {
      if (c->returned || c->broken)
	 break;
      if (!c->donteval)		/* On a dry run go through loop once */
	 while (i < selsize && i < c->curfv->map->enccount && !sel[i])
	    ++i;
      if (i >= selsize || i >= c->curfv->map->enccount)
	 break;
      c->curfv->selected[i]=true;
      while ((tok=ff_NextToken(c)) != tt_endloop && tok != tt_eof
	     && !c->returned && !c->broken) {
	 ff_backuptok(c);
	 ff_statement(c);
      }
      c->curfv->selected[i]=false;
      if (tok==tt_eof)
	 ScriptError(c, "End of file found in foreach loop");
      cseek(c, here);
      c->lineno=lineno;
      ++i;
      if (c->donteval)
	 break;
   }
   c->broken=false;

   nest=0;
   while ((tok=ff_NextToken(c)) != tt_endloop || nest > 0) {
      if (tok==tt_eof)
	 ScriptError(c, "End of file found in foreach loop");
      else if (tok==tt_while)
	 ++nest;
      else if (tok==tt_foreach)
	 ++nest;
      else if (tok==tt_endloop)
	 --nest;
   }
   if (selsize==c->curfv->map->enccount)
      memcpy(c->curfv->selected, sel, selsize);
   free(sel);
}

static void dowhile(Context *c) {
   long here=aftell(c->script);
   int lineno=c->lineno;
   enum token_type tok;
   Val val;
   int nest;

   c->broken=false;
   while (1) {
      if (c->returned || c->broken)
	 break;
      tok=ff_NextToken(c);
      expect(c, tt_lparen, tok);
      val.type=v_void;
      expr(c, &val);
      tok=ff_NextToken(c);
      expect(c, tt_rparen, tok);
      dereflvalif(&val);
      if (!c->donteval) {	/* If we do a dry run, check the syntax of loop body once */
	 if (val.type != v_int)
	    ScriptError(c, "Expected integer expression in while condition");
	 if (val.u.ival==0)
	    break;
      }
      while ((tok=ff_NextToken(c)) != tt_endloop && tok != tt_eof
	     && !c->returned && !c->broken) {
	 ff_backuptok(c);
	 ff_statement(c);
      }
      if (tok==tt_eof)
	 ScriptError(c, "End of file found in while loop");
      cseek(c, here);
      c->lineno=lineno;
      if (c->donteval)
	 break;
   }
   c->broken=false;

   nest=0;
   while ((tok=ff_NextToken(c)) != tt_endloop || nest > 0) {
      if (tok==tt_eof)
	 ScriptError(c, "End of file found in while loop");
      else if (tok==tt_while)
	 ++nest;
      else if (tok==tt_foreach)
	 ++nest;
      else if (tok==tt_endloop)
	 --nest;
   }
}

static void doif(Context *c) {
   enum token_type tok;
   Val val;
   int nest;

   while (1) {
      tok=ff_NextToken(c);
      expect(c, tt_lparen, tok);
      val.type=v_void;
      expr(c, &val);
      tok=ff_NextToken(c);
      expect(c, tt_rparen, tok);
      dereflvalif(&val);
      if (val.type != v_int && !c->donteval)
	 ScriptError(c, "Expected integer expression in if condition");
      if (c->donteval || val.u.ival != 0) {
	 while ((tok=ff_NextToken(c)) != tt_endif && tok != tt_eof
		&& tok != tt_else && tok != tt_elseif && !c->returned
		&& !c->broken) {
	    ff_backuptok(c);
	    ff_statement(c);
	 }
	 if (tok==tt_eof)
	    ScriptError(c, "End of file found in if ff_statement");
	 if (!c->donteval)
	    break;
      } else {
	 nest=0;
	 while (((tok=ff_NextToken(c)) != tt_endif && tok != tt_else
		 && tok != tt_elseif) || nest > 0) {
	    if (tok==tt_eof)
	       ScriptError(c, "End of file found in if ff_statement");
	    else if (tok==tt_if)
	       ++nest;
	    else if (tok==tt_endif)
	       --nest;
	 }
      }
      if (tok==tt_else) {
	 while ((tok=ff_NextToken(c)) != tt_endif && tok != tt_eof
		&& !c->returned && !c->broken) {
	    ff_backuptok(c);
	    ff_statement(c);
	 }
	 break;
      } else if (tok==tt_endif)
	 break;
   }
   if (c->returned || c->broken)
      return;
   if (tok != tt_endif && tok != tt_eof) {
      nest=0;
      while ((tok=ff_NextToken(c)) != tt_endif || nest > 0) {
	 if (tok==tt_eof)
	    return;
	 else if (tok==tt_if)
	    ++nest;
	 else if (tok==tt_endif)
	    --nest;
      }
   }
}

static void doshift(Context *c) {
   int i;

   if (c->donteval)
      return;
   if (c->a.argc==1)
      ScriptError(c, "Attempt to shift when there are no arguments left");
   if (c->a.vals[1].type==v_str)
      free(c->a.vals[1].u.sval);
   if (c->a.vals[1].type==v_arr && c->a.vals[1].u.aval != c->dontfree[1])
      arrayfree(c->a.vals[1].u.aval);
   --c->a.argc;
   for (i=1; i < c->a.argc; ++i) {
      c->a.vals[i]=c->a.vals[i + 1];
      c->dontfree[i]=c->dontfree[i + 1];
   }
}

static void ff_statement(Context *c) {
   enum token_type tok=ff_NextToken(c);
   Val val;

   if (tok==tt_while)
      dowhile(c);
   else if (tok==tt_foreach)
      doforeach(c);
   else if (tok==tt_if)
      doif(c);
   else if (tok==tt_shift)
      doshift(c);
   else if (tok==tt_else || tok==tt_elseif || tok==tt_endif
	    || tok==tt_endloop) {
      unexpected(c, tok);
   } else if (tok==tt_break) {
      c->broken=true;
   } else if (tok==tt_return) {
      tok=ff_NextToken(c);
      ff_backuptok(c);
      c->returned=true;
      c->return_val.type=v_void;
      if (tok != tt_eos) {
	 expr(c, &c->return_val);
	 dereflvalif(&c->return_val);
	 if (c->return_val.type==v_arr) {
	    c->return_val.type=v_arrfree;
	    c->return_val.u.aval=arraycopy(c->return_val.u.aval);
	 }
      }
   } else if (tok==tt_eos) {
      ff_backuptok(c);
   } else {
      ff_backuptok(c);
      expr(c, &val);
      if (c->interactive) {
	 if (c->error) {
	    c->error=false;
	 } else if (val.type != v_void) {
	    printf("-> ");
	    PrintVal(&val);
	    printf("\n");
	    fflush(stdout);
	 }
      }
      if (val.type==v_str)
	 free(val.u.sval);
   }
   tok=ff_NextToken(c);
   if (tok != tt_eos && tok != tt_eof && !c->returned && !c->broken)
      ScriptError(c, "Unterminated ff_statement");
}

/* we know this is never called on stdin.  FIXME it should be abstracted too */
static AFILE *CopyNonSeekableFile(AFILE *former) {
   int ch;
   AFILE *temp=atmpfile();

   if (temp==NULL)
      return (former);

   while ((ch=agetc(former)) >= 0) {
      aputc(ch, temp);
   }

   afseek(temp,0,SEEK_SET);
   return (temp);
}

static void ff_VerboseCheck(void) {
   if (verbose==-1)
     verbose=getenv("FONTANVIL_VERBOSE")!=NULL;
}

int dry_option_selected=0;

static void RunScriptInterpreter(char *script_name,AFILE *script_file,
				 int script_argc,char **script_argv) {
   Context c;
   int i;
   char *tstr;
   enum token_type tok;
   jmp_buf env;

   running_script=true;

   ff_VerboseCheck();

   /* set up the context */
   memset(&c,0,sizeof(Context));
   c.a.argc=script_argc+1;
   c.a.vals=malloc(c.a.argc*sizeof(Val));
   c.dontfree=calloc(c.a.argc,sizeof(Array *));
   c.donteval=dry_option_selected;
   c.return_val.type=v_void;
   c.filename=script_name;
   c.script=script_file;

   /* copy and convert the arguments */
   c.a.vals[0].type=v_str;
   tstr=fastrdup(script_name);
   c.a.vals[0].u.sval=utf82script_copy(tstr);
   free(tstr);
   for (i=0;i<script_argc;i++) {
      c.a.vals[i+1].type=v_str;
      tstr=fastrdup(script_argv[i]);
      c.a.vals[i+1].u.sval=utf82script_copy(tstr);
      free(tstr);
   }

   /* If the file is not seekable, we copy it into a seekable file.
    * On Mac OS/X fseek/ftell appear to be broken and return success even
    * for terminals.  They should return -1, EBADF */
   if (aftell(c.script)==-1 || aisatty(c.script)) {
      if (c.script==astdin) {
	 c.script=atmpfile();
	 if (c.script)
	   c.interactive=true;
      } else {
	 AFILE *tmpfile1=CopyNonSeekableFile(c.script);

	 afclose(c.script);
	 c.script=tmpfile1;
      }
   }

   if (c.script) {
      /* If the script is accessible, we start to parse it. */
      c.lineno=1;

      /* Set the jump environment for returning from the error reporter. */
      if (c.interactive) {
	 while (setjmp(env)); /* SRSLY??? */
	 c.err_env=&env;
      }

      /* Parse and execute. */
      while (c.script && !c.error && !c.returned && !c.broken
	     && (tok=ff_NextToken(&c))!=tt_eof) {
	 ff_backuptok(&c);
	 ff_statement(&c);
      }
      if ((c.script!=astdin) && (c.script!=NULL))
	afclose(c.script);
   } else
     ScriptError(&c,"Internal null file error");

   /* Free previously copied arguments. */
   for (i=0;i<c.a.argc;i++)
     free(c.a.vals[i].u.sval);
   free(c.a.vals);
   free(c.dontfree);
}

void ExecuteOneScriptCommand(char *command,
			     int script_argc,char **script_argv) {
   AFILE *script_file;

   script_file=atmpfile();
   if (script_file) {
      afwrite(command,1,strlen(command),script_file);
      afseek(script_file,0,SEEK_SET);
      RunScriptInterpreter("<command-string>",script_file,
			   script_argc,script_argv);
   } else {
      puts("Cannot create temp file");
      exit(1);
   }
}

void ExecuteScriptCommandsInteractively(int script_argc,char **script_argv) {
   RunScriptInterpreter("<stdin>",astdin,script_argc,script_argv);
}

void ExecuteScriptFile(char *script_filename,
		       int script_argc,char **script_argv) {
   AFILE *script_file;

   script_file=afopen(script_filename,"r");
   if (script_file)
     RunScriptInterpreter(script_filename,script_file,
			  script_argc,script_argv);
   else {
      puts("Cannot open script file");
      exit(1);
   }
}
