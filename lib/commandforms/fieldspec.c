/* fieldspec.c - library functions for field specifiers. */

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

#define FIELDSPEC_HASH_BUCKETS  32      /* must be power of two */

HASH_TABLE *fieldspecs = (HASH_TABLE *) NULL;


/* Forward references */
static void fieldspec_free __P ((PTR_T));
static void fieldspec_dispose __P ((FIELDSPEC *));
static int fieldspec_flushfree __P ((BUCKET_CONTENTS *));
static void fieldspecs_create __P ((void));

/* Static Variables */


/* Create a new field spec  */
FIELDSPEC *
fieldspec_create ()
{
  FIELDSPEC *fieldspec;

  fieldspec = (FIELDSPEC *) xmalloc (sizeof (FIELDSPEC));
  memset (fieldspec, 0, sizeof (FIELDSPEC));
  fieldspec->refcount = 0;

  fieldspec->fieldtype = CF_FIELD_TYPE_INVALID;
  fieldspec->helptext = (char *) NULL;
  fieldspec->label = (char *) NULL;
  fieldspec->flag = (char *) NULL;
  fieldspec->valuescount = 0;
  fieldspec->hinttextcount = 0;
  fieldspec->values = (char **) 0;
  fieldspec->displayvalues = (char **) 0;
  fieldspec->hinttext = (char **) NULL;
  fieldspec->compspec = (char *) NULL;
  fieldspec->separator = (char *) NULL;

  return fieldspec;
}

/* Dispose of a fieldspec based on reference count */
static void
fieldspec_dispose (fieldspec)
     FIELDSPEC *fieldspec;
{
  fieldspec->refcount--;
  if (fieldspec->refcount == 0)
    {
      FREE (fieldspec->helptext);
      FREE (fieldspec->label);
      FREE (fieldspec->flag);
      if (fieldspec->values)
        {
          FREE (fieldspec->values[0]);
          free (fieldspec->values);
        }
      if (fieldspec->displayvalues && fieldspec->values != fieldspec->displayvalues)
        {
          FREE (fieldspec->displayvalues[0]);
          free (fieldspec->displayvalues);
        }
      if (fieldspec->hinttext)
        {
          FREE (fieldspec->hinttext[0]);
          free (fieldspec->hinttext);
        }
      FREE (fieldspec->compspec);
      FREE (fieldspec->separator);
      free (fieldspec);
    }
}


static void
fieldspec_free (data)
     PTR_T data;
{
  FIELDSPEC *fieldspec;

  fieldspec = (FIELDSPEC *) data;
  fieldspec_dispose (fieldspec);
}

/* Call back function when flushing fieldspecs hash */
static int
fieldspec_flushfree (item)
     BUCKET_CONTENTS *item;
{
  FIELDSPEC *fieldspec;

  fieldspec = (FIELDSPEC *) item->data;
  if (fieldspec->refcount == 0)
    {
      hash_remove (item->key, fieldspecs, 0);
      fieldspec_dispose (fieldspec);
    }
  return 1;
}


/* Remove a fieldspec from the fieldspec hash and free the fieldspec */
int
fieldspec_remove (fieldspecname)
     char *fieldspecname;
{
  register BUCKET_CONTENTS *item;
  FIELDSPEC *fieldspec;

  if (fieldspecs == 0)
    return 1;

  fieldspec = fieldspec_search (fieldspecname);

  /* Only remove if refcount 0 */
  if (fieldspec && fieldspec->refcount == 0)
    {
      item = hash_remove (fieldspecname, fieldspecs, 0);
      if (item)
        {
          if (item->data)
            fieldspec_free (item->data);
          free (item->key);
          free (item);
          return (1);
        }
      else
        return (0);
    }
  else
    return (0);
}

/* Make fieldspec for retention */
void
fieldspec_retain (fieldspec)
     FIELDSPEC *fieldspec;
{
  fieldspec->refcount++;
}

/* Insert a new fieldspec into the fieldspecs hash */
int
fieldspec_insert (fieldspecname, fieldspec)
     char *fieldspecname;
     FIELDSPEC *fieldspec;
{
  register BUCKET_CONTENTS *item;

  if (fieldspec == NULL)
    programming_error ("fieldspec_insert: %s: NULL FIELDSPEC", fieldspecname);

  if (fieldspecs == 0)
    fieldspecs_create ();

  item = hash_insert (fieldspecname, fieldspecs, 0);
  item->key = savestring (fieldspecname);
  item->data = fieldspec;
  return 1;
}

/* Locate a field spec via the fieldspecs hash */
FIELDSPEC *
fieldspec_search (fieldspecname)
     char *fieldspecname;
{
  register BUCKET_CONTENTS *item;
  FIELDSPEC *fieldspec;

  if (fieldspecs == 0)
    return ((FIELDSPEC *) NULL);

  item = hash_search (fieldspecname, fieldspecs, 0);

  if (item == NULL)
    return ((FIELDSPEC *) NULL);

  fieldspec = (FIELDSPEC *) item->data;

  return (fieldspec);
}

/* Create the fieldspecs hash if required */
static void
fieldspecs_create ()
{
  if (fieldspecs == 0)
    fieldspecs = hash_create (FIELDSPEC_HASH_BUCKETS);
}

/*
 * Remove all entries from the fieldspecs hash and dispose of the objects if
 * not used.
 */
void
fieldspecs_flush ()
{
  if (fieldspecs)
    hash_walk (fieldspecs, fieldspec_flushfree);
}

/*
 * FIXME can remove ? Dispose of all the fieldspec hash entries
 */
static void
fieldspecs_dispose ()
{
  if (fieldspecs)
    hash_dispose (fieldspecs);
  fieldspecs = (HASH_TABLE *) NULL;
}

/* Walk the fieldspecs hash calling for each entry a "helper" function */
void
fieldspecs_walk (pfunc)
     hash_wfunc *pfunc;
{
  if (fieldspecs == 0 || pfunc == 0 || HASH_ENTRIES (fieldspecs) == 0)
    return;

  hash_walk (fieldspecs, pfunc);
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

int
fieldspec_print (cmd, cs)
     char *cmd;
     FIELDSPEC *cs;
{
  char **pvalue;
  char *string;
  char *x;

  printf ("fieldspec ");

  string = NULL;
  CONDITIONALSQPRINTSTRING (cs->label, "+label")
    CONDITIONALSQPRINTSTRING (string, "+fieldtype");
  if (cs->fieldtype == CF_FIELD_TYPE_FLAG)
    string = "flag";
  else if (cs->fieldtype == CF_FIELD_TYPE_FLAGWITHVALUE)
    string = "flagwithvalue";
  else if (cs->fieldtype == CF_FIELD_TYPE_POSITIONAL)
    string = "positional";
  else if (cs->fieldtype == CF_FIELD_TYPE_UPTOLAST)
    string = "uptolast";
  else if (cs->fieldtype == CF_FIELD_TYPE_LAST)
    string = "last";
  else if (cs->fieldtype == CF_FIELD_TYPE_REST)
    string = "rest";
  CONDITIONALSQPRINTSTRING (cs->compspec, "+compspec")
    CONDITIONALSQPRINTSTRING (cs->separator, "+separator")
    CONDITIONALSQPRINTSTRING (cs->flag, "+flag")
    if (cs->displayvalues && *(cs->displayvalues))
    {
      printf ("+displayvalues ");
      for (pvalue = cs->displayvalues; *pvalue; pvalue++)
        {
          x = sh_single_quote (*pvalue);
          printf ("%s ", x);
          free (x);
        }
    }
  if (cs->values && *(cs->values))
    {
      printf ("+values ");
      for (pvalue = cs->values; *pvalue; pvalue++)
        {
          x = sh_single_quote (*pvalue);
          printf ("%s ", x);
          free (x);
        }
    }
  if (cs->hinttext && *(cs->hinttext))
    {
      printf ("+hinttext ");
      for (pvalue = cs->hinttext; *pvalue; pvalue++)
        {
          x = sh_single_quote (*pvalue);
          printf ("%s ", x);
          free (x);
        }
    }
    CONDITIONALSQPRINTSTRING (cs->helptext, "+helptext") printf ("%s\n", cmd);

  return (0);
}




/* Determine the index into the translation table for a value. */
int
fieldspec_valueindex (fieldspec, value)
     FIELDSPEC *fieldspec;
     char *value;
{
  int i;
  for (i = 0; i < fieldspec->valuescount; i++)
    {
      if (strcmp (fieldspec->values[i], value) == 0)
        return i;
    }
  return -1;
}

/* Determine the index into the translation table for a display value. */
int
fieldspec_displayvalueindex (fieldspec, displayvalue, partial)
     FIELDSPEC *fieldspec;
     char *displayvalue;
     int partial;
{
  int i;
  int len = strlen (displayvalue);

  for (i = 0; i < fieldspec->valuescount; i++)
    {
      if (partial)
        {
          if (strncasecmp
              (fieldspec->displayvalues[i], displayvalue,
               (unsigned int) len) == 0)
            return i;
        }
      else
        {
          if (strcmp (fieldspec->displayvalues[i], displayvalue) == 0)
            return i;
        }
    }
  return -1;
}

#endif /* COMMAND_FORMS */
