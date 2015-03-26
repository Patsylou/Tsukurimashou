/* $Id: afile.h 3865 2015-03-26 10:37:06Z mskala $ */
/*
 * File abstraction for FontAnvil
 * Copyright (C) 2015  Matthew Skala
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Matthew Skala
 * http://ansuz.sooke.bc.ca/
 * mskala@ansuz.sooke.bc.ca
 */

#ifndef _AFILE_H
#define _AFILE_H

#include <stdarg.h>

#if 0
#define AFILE FILE
#else
typedef int AFILE;
#endif

#define astdin ((AFILE *)stdin)
#define astdout ((AFILE *)stdout)
#define astderr ((AFILE *)stderr)

AFILE *afopen(const char *,const char *);
int afclose(AFILE *);

AFILE *atmpfile(void);

int afeof(AFILE *);
int aferror(AFILE *);

void arewind(AFILE *);
int afseek(AFILE *,long,int);
long aftell(AFILE *);

int agetc(AFILE *);
int aungetc(int,AFILE *);
int aputc(int,AFILE *);
int afputs(const char *,AFILE *);

size_t afread(void *,size_t, size_t,AFILE *);
size_t afwrite(const void *,size_t, size_t,AFILE *);

int afprintf(AFILE *,const char *,...);
int avfprintf(AFILE *,const char * restrict format,va_list);


/*
 * Error message levels:
 *   0: informational (not an error, but might be written in verbose mode)
 *   1: unavoidable warning and debug messages (occurs in normal operation
 *      and user can't prevent it, such as overlap-related nonsense)
 *   2: non-fatal error that the user ought to prevent
 *   3: fatal error that terminates the program
 */

void ErrorMsg(int,const char *,...);

#endif /* ndef _AFILE_H */
