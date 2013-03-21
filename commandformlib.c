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



static void fieldspec_free __P((PTR_T));
static void formspec_free __P((PTR_T));


static void fieldspec_dispose __P(( FIELDSPEC *));

static int fieldspec_flushfree __P(( BUCKET_CONTENTS *));
/*
	Create a new field spec 
 */
FIELDSPEC *
fieldspec_create ()
{
  FIELDSPEC *ret;

  ret = (FIELDSPEC *)xmalloc (sizeof (FIELDSPEC));
  memset(ret, 0, sizeof (FIELDSPEC) );
  ret->refcount = 0;

  return ret;
}
/*
	Create a new formspec
 */

FORMSPEC *
formspec_create ()
{
  FORMSPEC *ret;

  ret = (FORMSPEC *)xmalloc (sizeof (FORMSPEC));
  memset(ret, 0, sizeof(FORMSPEC) );


  return ret;
}
/*
	Dispose of a fieldspec based on reference count
 */
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
	  if ( cs->displaylevels )
	  {
		 FREE(cs->displaylevels[0]);
		 FREE(cs->displaylevels);
	  }
	  FREE(cs->compspec);
	  FREE(cs->separator);
      free (cs);
    }
}

/*
	Displose of a form spec
 */
void
formspec_dispose (form)
	FORMSPEC *form;
{
	FORMFIELDSPEC **l;

	/*
		Decrement the retain counts for all associated fields
		and free field list entries
     */
	 for(l=form->screenfieldlist; *l; l++)
	  {
		  (*l)->fieldspec->refcount--;
		  free(*l);
	  }
	 for(l=form->generationfieldlist; *l; l++)
	  {
		  free(*l);
	  }
	  if ( form->displaylevels )
	  {
		 FREE(form->displaylevels[0]);
		 FREE(form->displaylevels);
	  }
	
	/*
		 Discard field lists
	*/
	free(form->screenfieldlist);
	free(form->generationfieldlist);
	free(form->command);

	/* Free form */
	free (form);
}

/*
	Create the formspecs hash is required
 */
void
fieldspecs_create ()
{
  if (fieldspecs == 0)
    fieldspecs = hash_create (FIELDSPEC_HASH_BUCKETS);
}

/*
	Create the formspecs hash is required
 */
void
formspecs_create ()
{
  if (formspecs == 0)
    formspecs = hash_create (FORMSPEC_HASH_BUCKETS);
}
/* 
	Return size of fieldspecs hash
 */
int
fieldspecs_size ()
{
  return (HASH_ENTRIES (fieldspecs));
}

/* 
	Return size of formspec hash
*/
int
formspecs_size ()
{
  return (HASH_ENTRIES (formspecs));
}
/*
	Free fieldspec
 */
static void
fieldspec_free (data)
	PTR_T data;
{
  FIELDSPEC *cs;

  cs = (FIELDSPEC *)data;
  fieldspec_dispose (cs);
}
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
  
/*
	Free a formspec
 */
static void
formspec_free (data)
	PTR_T data;
{
	FORMSPEC *cs;

	cs = (FORMSPEC *)data;
	formspec_dispose (cs);
}
  
/*
	Remove all entries from the fieldspecs hash and dispose of the objects
	if not used.
 */
void
fieldspecs_flush ()
{
  if (fieldspecs)
	  hash_walk (fieldspecs, fieldspec_flushfree);
}

void
formspecs_flush ()
{
  if (formspecs)
    hash_flush (formspecs, formspec_free);
}
/*
	Dispose of all the field spec hash entries
 */
void
fieldspecs_dispose ()
{
  if (fieldspecs)
    hash_dispose (fieldspecs);
  fieldspecs = (HASH_TABLE *)NULL;
}

/*
	Dispose of all the field spec hash entries
 */
void
formspecs_dispose ()
{
  if (formspecs)
    hash_dispose (formspecs);
  formspecs = (HASH_TABLE *)NULL;
}
/*
	Remove a fieldspec from the fieldspec hash and free the fieldspec
 */
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

/*
	Remove a formspec from the formspec hash and free the formspec
 */
int
formspec_remove ( cmd)
	char *cmd;
{
  register BUCKET_CONTENTS *item;

  if (formspecs == 0)
    return 1;

  item = hash_remove (cmd, formspecs, 0);
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

void fieldspec_retain(cs)
	FIELDSPEC *cs;
{
	cs->refcount++;
}
/*
	Insert a new fieldspec into the fieldspec hash
 */
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


/* 
	Insert a new formspec into the formspec hash
 */
int
formspec_insert ( cmd, cs)
	char *cmd;
	FORMSPEC *cs;
{
  register BUCKET_CONTENTS *item;

  if (cs == NULL)
    programming_error ("formspec_insert: %s: NULL FORMSPEC", cmd);

  if (formspecs == 0)
    formspecs_create ();

  item = hash_insert (cmd, formspecs, 0);
  if (item->data)
    formspec_free (item->data);
  else
    item->key = savestring (cmd);
  item->data = cs;
  return 1;
}

/* 
	Locate a field spec via the fieldspec hash
 */
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

/* 
	Locate a formspec  via the fieldspec hash
 */
FORMSPEC *
formspec_search ( cmd )
	const char *cmd;
{
  register BUCKET_CONTENTS *item;
  FORMSPEC *cs;

  if (formspecs == 0)
    return ((FORMSPEC *)NULL);

  item = hash_search (cmd, formspecs, 0);

  if (item == NULL)
    return ((FORMSPEC *)NULL);

  cs = (FORMSPEC *)item->data;

  return (cs);
}
/*
	Walk the fieldspec hash calling for each entry a "helper" function
 */
void
fieldspecs_walk (pfunc)
	hash_wfunc *pfunc;
{
  if (fieldspecs == 0 || pfunc == 0 || HASH_ENTRIES (fieldspecs) == 0)
    return;

  hash_walk (fieldspecs, pfunc);
}

/*
	Walk the fieldspec hash calling for each entry a "helper" function
 */
void
formspecs_walk (pfunc)
	hash_wfunc *pfunc;
{
  if (formspecs == 0 || pfunc == 0 || HASH_ENTRIES (formspecs) == 0)
    return;

  hash_walk (formspecs, pfunc);
}

#endif /* COMMAND_FORMS */
