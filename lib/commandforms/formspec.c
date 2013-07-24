/* formspec.c - library functions for form specifiers. */

/*
 * Copyright (C) 2013 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Bash, the Bourne Again SHell.
 * 
 * Bash is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2, or (at your option) any later version.
 * 
 * Bash is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * Bash; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place, Suite 330, Boston, MA 02111 USA.
 */

#include <config.h>

#if defined (COMMAND_FORMS)

#include "bashansi.h"
#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#if defined (_MINIX)
#include <sys/types.h>
#endif
#include <unistd.h>
#endif

#include "shell.h"
#include "pcomplete.h"

#include "commandforms.h"

#if defined (READLINE)
#include "bashline.h"
#include <readline/readline.h>
#include <readline/tcap.h>
#endif

#define FORMSPEC_HASH_BUCKETS  32       /* must be power of two */


HASH_TABLE *formspecs = (HASH_TABLE *) NULL;

/* Forward references */
static void formspec_free __P ((PTR_T));



/* Forward References */

static int string_list_contains __P ((char **, char *));

/* Static Variables */

/* External references to readline globals */

static void
formfieldspec_dispose(formfieldspec)
    FORMFIELDSPEC *formfieldspec;
{
/* XXX */
  FREE (formfieldspec->fieldspecname);
  free(formfieldspec);
  
}


/* formspec Utilities */

/* Create a new formspec */
FORMSPEC *
formspec_create ()
{
  FORMSPEC *formspec;

  formspec = (FORMSPEC *) xmalloc (sizeof (FORMSPEC));
  memset (formspec, 0, sizeof (FORMSPEC));

  formspec->command = (char *) NULL;
  formspec->displaylevelcount = 0;
  formspec->displaylevels = (DISPLAYLEVEL **) NULL;
  return formspec;
}

/* Dispose of a formspec */
void
formspec_dispose (formspec)
     FORMSPEC *formspec;
{
  DISPLAYLEVEL **displaylevel;
  int i;

  FREE (formspec->command);
  if (formspec->displaylevels)
    {
      for (i = 0, displaylevel = formspec->displaylevels;
           i < formspec->displaylevelcount ; i++, displaylevel++)
        {
          displaylevel_dispose (*displaylevel);
        }
      free (formspec->displaylevels);
    }
  /* Free form */
  free (formspec);
}

/* Create the formspecs hash if required */
static void
formspecs_create ()
{
  if (formspecs == 0)
    formspecs = hash_create (FORMSPEC_HASH_BUCKETS);
}


/* Free a formspec */
static void
formspec_free (data)
     PTR_T data;
{
  FORMSPEC *formspec;

  formspec = (FORMSPEC *) data;
  formspec_dispose (formspec);
}

/* Flush all the formspecs from the formspecs hash and free them */
void
formspecs_flush ()
{
  if (formspecs)
    hash_flush (formspecs, formspec_free);
}

/* Dispose of all the field spec hash entries */
void
formspecs_dispose ()
{
  if (formspecs)
    hash_dispose (formspecs);
  formspecs = (HASH_TABLE *) NULL;
}

/* Remove a formspec from the formspec hash and free the formspec */
int
formspec_remove (formname)
     char *formname;
{
  register BUCKET_CONTENTS *item;

  if (formspecs == 0)
    return 1;

  item = hash_remove (formname, formspecs, 0);
  if (item)
    {
      if (item->data)
        formspec_free (item->data);
      free (item->key);
      free (item);
      return (1);
    }
  return (0);
}


/* Insert a new formspec into the formspec hash */
int
formspec_insert (formname, formspec)
     char *formname;
     FORMSPEC *formspec;
{
  register BUCKET_CONTENTS *item;

  if (formspec == NULL)
    programming_error ("formspec_insert: %s: NULL FORMSPEC", formname);

  if (formspecs == 0)
    formspecs_create ();

  item = hash_insert (formname, formspecs, 0);
  if (item->data)
    formspec_free (item->data);
  else
    item->key = savestring (formname);
  item->data = formspec;
  return 1;
}


/* Locate a formspec  via the formspec hash */
FORMSPEC *
formspec_search (formname)
     char *formname;
{
  register BUCKET_CONTENTS *item;
  FORMSPEC *formspec;

  if (formspecs == 0)
    return ((FORMSPEC *) NULL);

  item = hash_search (formname, formspecs, 0);

  if (item == NULL)
    return ((FORMSPEC *) NULL);

  formspec = (FORMSPEC *) item->data;

  return (formspec);
}

/*
 * Walk the formspecs hash calling for each entry a "helper" function
 */
void
formspecs_walk (pfunc)
     hash_wfunc *pfunc;
{
  if (formspecs == 0 || pfunc == 0 || HASH_ENTRIES (formspecs) == 0)
    return;

  hash_walk (formspecs, pfunc);
}

/* Utility Functions */

/* Return whether a string is in a list of strings */
static int
string_list_contains (list, string)
     char **list;
     char *string;
{
  while (*list)
    {
      if (strcmp (*list, string) == 0)
        return 1;
      list++;
    }
  return 0;
}



/* Field specifier functions */


/* Print field specification */
#define CONDITIONALSQPRINTSTRING(a, f) \
    if (a && *a) \
      { \
        x = sh_single_quote (a); \
  printf ("%s %s ", f, x); \
  free (x); \
      }

#define CONDITIONALPRINTDOUBLE(a, f) \
    if (a != 0.0 ) \
      printf ("%s %g ", f, a);

#define CONDITIONALPRINTINT(a, f) \
    if (a != 0 ) \
      printf ("%s %d ",  f, a);

#define CONDITIONALPRINTSTRING(a, f) \
    if ( a && *(a) )  \
      printf ("%s %s", f, a);


/*
 * Form specifier functions
 * 
 */


/*
 * Print a form specifier
 */
int
formspec_print (formname, form)
     char *formname;
     FORMSPEC *form;
{

  FORMFIELDSPEC **l;
  DISPLAYLEVEL **d;
  char *x;
  int i;
  int j;

  printf ("formspec ");

  for (i = 0, d = form->displaylevels; i < form->displaylevelcount; i++, d++)
    {
      printf ("+displaylevel=%s ", (*d)->displaylevel);
      printf ("+screenfieldlist ");
      for (j = 0, l = (*d)->screenfieldlist; j < (*d)->fieldcount; j++, l++)
        {
          printf ("%s ", (*l)->fieldspecname);
        }
      printf ("+generationformlist ");
      for (l = (*d)->generationfieldlist; *l; l++)
        {
          printf ("%s ", (*l)->fieldspecname);
        }
    }
  CONDITIONALSQPRINTSTRING (form->command, "+command")
    CONDITIONALSQPRINTSTRING (formname, "+formname") printf ("\n");
  return (0);
}




#endif /* COMMAND_FORMS */
