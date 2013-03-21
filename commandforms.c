/* commandforms.c - functions to display command forms
		  */

/* Copyright (C) 1999-2002 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   Bash is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with Bash; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place, Suite 330, Boston, MA 02111 USA. */

#include <config.h>

#if defined (COMMAND_FORMS)

#include "bashtypes.h"
#include "posixstat.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <signal.h>

#if defined (PREFER_STDARG)
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#include <stdio.h>
#include "bashansi.h"

#include "shell.h"
#include "pcomplete.h"
#include "commandforms.h"
#include "alias.h"
#include "bashline.h"
#include "execute_cmd.h"
#include "pathexp.h"

#if defined (JOB_CONTROL)
#  include "jobs.h"
#endif

#if !defined (NSIG)
#  include "trap.h"
#endif

#include "builtins.h"
#include "builtins/common.h"

#include <glob/glob.h>
#include <glob/strmatch.h>

#include <readline/rlconf.h>
#include <readline/readline.h>
#include <readline/history.h>

#ifdef STRDUP
#  undef STRDUP
#endif
#define STRDUP(x)	((x) ? savestring (x) : (char *)NULL)

typedef SHELL_VAR **SVFUNC ();

#ifndef HAVE_STRPBRK
extern char *strpbrk __P((char *, char *));
#endif


#if defined (DEBUG)
#if defined (PREFER_STDARG)
static void debug_printf (const char *, ...)  __attribute__((__format__ (printf, 1, 2)));
#endif
#endif /* DEBUG */


static int shouldexp_filterpat __P((char *));
static char *preproc_filterpat __P((char *, char *));

static void init_itemlist_from_varlist __P((ITEMLIST *, SVFUNC *));

static STRINGLIST *gen_matches_from_itemlist __P((ITEMLIST *, const char *));


#ifdef DEBUG
static int commandforms_debug = 1;
#endif

int commandforms_enabled = 1;

#ifdef DEBUG
/* Debugging code */
static void
#if defined (PREFER_STDARG)
debug_printf (const char *format, ...)
#else
debug_printf (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  if (fieldspec_debug == 0)
    return;

  SH_VA_START (args, format);

  fprintf (stdout, "DEBUG: ");
  vfprintf (stdout, format, args);
  fprintf (stdout, "\n");

  rl_on_new_line ();

  va_end (args);
}
#endif


static int
shouldexp_filterpat (s)
     char *s;
{
  register char *p;

  for (p = s; p && *p; p++)
    {
      if (*p == '\\')
	p++;
      else if (*p == '&')
	return 1;
    }
  return 0;
}

/* Replace any instance of `&' in PAT with TEXT.  Backslash may be used to
   quote a `&' and inhibit substitution.  Returns a new string.  This just
   calls stringlib.c:strcreplace(). */
static char *
preproc_filterpat (pat, text)
     char *pat;
     char *text;
{
  char *ret;

  ret = strcreplace (pat, '&', text, 1);
  return ret;
}
	
static void
init_itemlist_from_varlist (itp, svfunc)
     ITEMLIST *itp;
     SVFUNC *svfunc;
{
  SHELL_VAR **vlist;
  STRINGLIST *sl;
  register int i, n;

  vlist = (*svfunc) ();
  for (n = 0; vlist[n]; n++)
    ;
  sl = strlist_create (n+1);
  for (i = 0; i < n; i++)
    sl->list[i] = savestring (vlist[i]->name);
  sl->list[sl->list_len = n] = (char *)NULL;
  itp->slist = sl;
}

/* Generate a list of all matches for TEXT using the STRINGLIST in itp->slist
   as the list of possibilities.  If the itemlist has been marked dirty or
   it should be regenerated every time, destroy the old STRINGLIST and make a
   new one before trying the match. */
static STRINGLIST *
gen_matches_from_itemlist (itp, text)
     ITEMLIST *itp;
     const char *text;
{
  STRINGLIST *ret, *sl;
  int tlen, i, n;

  if ((itp->flags & (LIST_DIRTY|LIST_DYNAMIC)) ||
      (itp->flags & LIST_INITIALIZED) == 0)
    {
      if (itp->flags & (LIST_DIRTY | LIST_DYNAMIC))
	clean_itemlist (itp);
      if ((itp->flags & LIST_INITIALIZED) == 0)
	initialize_itemlist (itp);
    }
  if (itp->slist == 0)
    return ((STRINGLIST *)NULL);
  ret = strlist_create (itp->slist->list_len+1);
  sl = itp->slist;
  tlen = STRLEN (text);
  for (i = n = 0; i < sl->list_len; i++)
    {
      if (tlen == 0 || STREQN (sl->list[i], text, (size_t)tlen))
          ret->list[n++] = STRDUP (sl->list[i]);
    }
  ret->list[ret->list_len = n] = (char *)NULL;
  return ret;
}


#ifdef ARRAY_VARS

static SHELL_VAR *
bind_formp_words (lwords)
     WORD_LIST *lwords;
{
  SHELL_VAR *v;

  v = find_variable ("FORM_WORDS");
  if (v == 0)
    v = make_new_array_variable ("FORM_WORDS");
  if (readonly_p (v))
    VUNSETATTR (v, att_readonly);
  if (array_p (v) == 0)
    v = convert_var_to_array (v);
  v = assign_array_var_from_word_list (v, lwords, 0);
  return v;
}
#endif /* ARRAY_VARS */



/* The driver function for the field specification  code.  Returns a list
   of matches for WORD, which is an argument to command CMD.  START and END
   bound the command currently being completed in rl_line_buffer. */
char **
testfieldspecs (cmd, word, start, end, foundp)
     const char *cmd;
     const char *word;
     int start, end, *foundp;
{
  FIELDSPEC *cs;
  STRINGLIST *ret;
  char **rmatches, *t;

  /* We look at the basename of CMD if the full command does not have
     an associated FIELDSPEC. */
  cs = fieldspec_search (cmd);
  if (cs == 0)
    {
      t = strrchr (cmd, '/');
      if (t)
	cs = fieldspec_search (++t);
    }
  if (cs == 0)
    {
      if (foundp)
	*foundp = 0;
      return ((char **)NULL);
    }

  /* Signal the caller that we found a FIELDSPEC for this command, and pass
     back any meta-options associated with the fieldspec. */
  //ret = gen_fieldspec_completions (cs, cmd, word, start, end);

  if (ret)
    {
      rmatches = ret->list;
      free (ret);
    }
  else
    rmatches = (char **)NULL;

  return (rmatches);
}

#endif /* COMMAND_FORMS */
