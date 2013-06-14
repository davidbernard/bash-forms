/* commandformlib.c - library functions for command forms. */

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

#include "bashansi.h"
#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "shell.h"
#include "pcomplete.h"
#include "commandforms.h"

#define FIELDSPEC_HASH_BUCKETS	32	/* must be power of two */
#define FORMSPEC_HASH_BUCKETS	32	/* must be power of two */

#define STRDUP(x)	((x) ? savestring (x) : (char *)NULL)

HASH_TABLE *fieldspecs = (HASH_TABLE *)NULL;
HASH_TABLE *formspecs = (HASH_TABLE *)NULL;


/* Forward references */
static void fieldspec_free __P((PTR_T));
static void fieldspec_dispose __P(( FIELDSPEC *));
static int fieldspec_flushfree __P(( BUCKET_CONTENTS *));
static void fieldspecs_create __P((void));

static void formspec_free __P((PTR_T));

/*	Create a new field spec  */
FIELDSPEC *
fieldspec_create ()
{
	FIELDSPEC *ret;

	ret = (FIELDSPEC *)xmalloc (sizeof (FIELDSPEC));
	memset(ret, 0, sizeof (FIELDSPEC) );
	ret->refcount = 0;

	ret->fieldtype = CF_FIELD_TYPE_INVALID;
	ret->hinttext = (char *)NULL;
	ret->helptext = (char *)NULL;
	ret->label = (char *)NULL;
	ret->flag = (char *)NULL;
	ret->valuescount = 0;
	ret->values = (char **)0;
	ret->displayvalues = (char **)0;
	ret->compspec = (char *)NULL;
	ret->separator = (char *)NULL;

	return ret;
}

/*	Dispose of a fieldspec based on reference count */
static void
fieldspec_dispose (cs)
	FIELDSPEC *cs;
{
  cs->refcount--;
  if (cs->refcount == 0)
    {
      FREE (cs->hinttext);
      FREE (cs->helptext);
      FREE (cs->label);
	  FREE(cs->flag);
	  if ( cs->values && cs->values[0])
	  {
		  FREE(cs->values[0]);
		  FREE(cs->values);
	  }
	  if ( cs->displayvalues &&
			cs->values != cs->displayvalues)
	  {
		 FREE(cs->displayvalues[0]);
		 FREE(cs->displayvalues);
	  }
	  FREE(cs->compspec);
	  FREE(cs->separator);
      free (cs);
    }
}

static void
fieldspec_free (data)
	PTR_T data;
{
  FIELDSPEC *cs;

  cs = (FIELDSPEC *)data;
  fieldspec_dispose (cs);
}

/*	Call back function when flushing fieldspecs hash */
static int
fieldspec_flushfree (item)
	BUCKET_CONTENTS *item;
{
	FIELDSPEC *cs;

	cs = (FIELDSPEC *)item->data;
	if ( cs->refcount == 0)
	{
		hash_remove(item->key, fieldspecs, 0);
		fieldspec_dispose (cs);
	}
return 1;
}


/* Remove a fieldspec from the fieldspec hash and free the fieldspec */
int
fieldspec_remove (cmd)
	char *cmd;
{
  register BUCKET_CONTENTS *item;
  FIELDSPEC *cs;

	if (fieldspecs == 0)
		return 1;

	cs = fieldspec_search(cmd);

	/* Only remove if refcount 0 */
	if ( cs && cs->refcount == 0)
	{
		item = hash_remove (cmd, fieldspecs, 0);
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
		return 0;
}

/* Make fieldspec for retention */
void fieldspec_retain(cs)
	FIELDSPEC *cs;
{
	cs->refcount++;
}

/* Insert a new fieldspec into the fieldspecs hash */
int
fieldspec_insert (cmd, fs)
	char *cmd;
	FIELDSPEC *fs;
{
  register BUCKET_CONTENTS *item;

	if (fs == NULL)
		programming_error ("fieldspec_insert: %s: NULL FIELDSPEC", cmd);

	if (fieldspecs == 0)
		fieldspecs_create ();

	item = hash_insert (cmd, fieldspecs, 0);
	item->key = savestring (cmd);
	item->data = fs;
	return 1;
}

/* 	Locate a field spec via the fieldspecs hash */
FIELDSPEC *
fieldspec_search ( cmd )
	const char *cmd;
{
  register BUCKET_CONTENTS *item;
  FIELDSPEC *cs;

  if (fieldspecs == 0)
    return ((FIELDSPEC *)NULL);

  item = hash_search (cmd, fieldspecs, 0);

  if (item == NULL)
    return ((FIELDSPEC *)NULL);

  cs = (FIELDSPEC *)item->data;

  return (cs);
}

/* Create the fieldspecs hash if required */
static void
fieldspecs_create ()
{
  if (fieldspecs == 0)
    fieldspecs = hash_create (FIELDSPEC_HASH_BUCKETS);
}

/*  Remove all entries from the fieldspecs hash and dispose of the objects
	if not used. */
void
fieldspecs_flush ()
{
  if (fieldspecs)
	  hash_walk (fieldspecs, fieldspec_flushfree);
}

/* FIXME can remove ?
 * Dispose of all the fieldspec hash entries */
static void
fieldspecs_dispose ()
{
  if (fieldspecs)
    hash_dispose (fieldspecs);
  fieldspecs = (HASH_TABLE *)NULL;
}

/*	Walk the fieldspecs hash calling for each entry a "helper" function */
void
fieldspecs_walk (pfunc)
	hash_wfunc *pfunc;
{
  if (fieldspecs == 0 || pfunc == 0 || HASH_ENTRIES (fieldspecs) == 0)
    return;

  hash_walk (fieldspecs, pfunc);
}

/* formspec Utilities */

/* 	Create a new formspec */
FORMSPEC *
formspec_create ()
{
	FORMSPEC *ret;

	ret = (FORMSPEC *)xmalloc (sizeof (FORMSPEC));
	memset(ret, 0, sizeof(FORMSPEC) );

	ret->command = (char *)NULL;
	ret->displaylevelcount = 0;
	ret->displaylevels = (DISPLAYLEVEL **)NULL;
	return ret;
}

/*	Dispose of a formspec */
void
formspec_dispose (form)
	FORMSPEC *form;
{
	DISPLAYLEVEL **dl;

	free(form->command);
	if ( form->displaylevels )
	{
		for(dl=form->displaylevels; *dl; dl++)
		{
			displaylevel_dispose(*dl);
		}
		FREE(form->displaylevels);
	}

	/* Free form */
	free (form);
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
	FORMSPEC *cs;

	cs = (FORMSPEC *)data;
	formspec_dispose (cs);
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
  formspecs = (HASH_TABLE *)NULL;
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


/* 	Insert a new formspec into the formspec hash */
int
formspec_insert (formname, cs)
	char *formname;
	FORMSPEC *cs;
{
  register BUCKET_CONTENTS *item;

  if (cs == NULL)
    programming_error ("formspec_insert: %s: NULL FORMSPEC", formname);

  if (formspecs == 0)
    formspecs_create ();

  item = hash_insert (formname, formspecs, 0);
  if (item->data)
    formspec_free (item->data);
  else
    item->key = savestring (formname);
  item->data = cs;
  return 1;
}


/* Locate a formspec  via the formspec hash */
FORMSPEC *
formspec_search ( formname )
	const char *formname;
{
  register BUCKET_CONTENTS *item;
  FORMSPEC *cs;

  if (formspecs == 0)
    return ((FORMSPEC *)NULL);

  item = hash_search (formname, formspecs, 0);

  if (item == NULL)
    return ((FORMSPEC *)NULL);

  cs = (FORMSPEC *)item->data;

  return (cs);
}

/*
	Walk the formspecs hash calling for each entry a "helper" function
 */
void
formspecs_walk (pfunc)
	hash_wfunc *pfunc;
{
  if (formspecs == 0 || pfunc == 0 || HASH_ENTRIES (formspecs) == 0)
    return;

  hash_walk (formspecs, pfunc);
}

DISPLAYLEVEL *
displaylevel_create ()
{
	DISPLAYLEVEL *ret;

	ret = (DISPLAYLEVEL *)xmalloc (sizeof (DISPLAYLEVEL));
	memset(ret, 0, sizeof(DISPLAYLEVEL) );

	ret->screenfieldlist = (FORMFIELDSPEC **)NULL;
	ret->generationfieldlist = (FORMFIELDSPEC **)NULL;
	ret->displaylevel = (char *)NULL;
	ret->fieldcount = 0;
	return ret;
}

/* Dispose of display level */
void
displaylevel_dispose(displaylevel)
DISPLAYLEVEL *displaylevel;
{
FORMFIELDSPEC **l;

/* 	Decrement the retain counts for all associated fields
and free field list entries */
if (displaylevel->screenfieldlist)
{
	for(l=displaylevel->screenfieldlist; *l; l++)
	{
		(*l)->fieldspec->refcount--;
		free(*l);
	}
	free(displaylevel->screenfieldlist);
}
if (displaylevel->generationfieldlist)
{
	for(l=displaylevel->generationfieldlist; *l; l++)
	{
		free(*l);
	}
	free(displaylevel->generationfieldlist);
}
free(displaylevel->displaylevel);

/* Free form */
free (displaylevel);
}
#endif /* COMMAND_FORMS */
