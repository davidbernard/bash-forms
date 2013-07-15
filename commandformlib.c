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
#if defined (READLINE)
#include "../bashline.h"
#include <readline/readline.h>
#include <readline/tcap.h>
#endif

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



/* Forward References */

static int string_list_contains __P((char **, char *));



static int fieldspec_valueindex __P((FIELDSPEC *, char *));
static int fieldspec_displayvalueindex __P((FIELDSPEC *, char *,  int ));

static void screenfield_setwithindex __P((SCREENFIELD *, int ));
static void screenfield_setwithvalue __P((SCREENFIELD *, char *value));
static void screenfield_setwithdisplayvalue __P((SCREENFIELD *, char *, int));
static void screenfield_appendtovalue __P((SCREENFIELD *, char *));

static int local_output_char __P((int));

static void screenform_hint __P((SCREENFORM *,char *,CF_EDIT_MODE));
static int cf_insert_or_cycle_screenfield __P((int,int));
static int cf_right_arrow_key __P((int,int));
static int cf_left_arrow_key __P((int,int));
static int cf_up_arrow_key __P((int,int));
static int cf_down_arrow_key __P((int,int));
static int cf_redraw __P((int,int));
static void cf_initialize_readline __P((void));

static void cf_display_matches __P((char **,int, int));
static char *cf_complete_return __P((const char *, int ));
static char **cf_completion __P((const char *,int, int));

static Keymap clone_keymap __P((Keymap));

static int screenform_displayvalue __P((void));

/* Static Variables */
static int cf_readline_initialized = 0;
static Keymap cf_keymap;
static rl_hook_func_t *old_rl_startup_hook = (rl_hook_func_t *)NULL;
static CF_EDIT_MODE cf_edit_mode = CF_MODE_FORM;

/* External references to readline globals */
extern char * _rl_term_ku;
extern char * _rl_term_kd;
extern char * _rl_term_kr;
extern char * _rl_term_kl;
extern int _rl_horizontal_scroll_mode;

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
		return (0);
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

/*  Utility Functions */

/* Return whether a string is in a list of strings */
static int
string_list_contains(list, string)
char **list;
char *string;
{
  while(*list)
  {
    if (strcmp(*list, string) == 0)
      return 1;
    list++;
  }
  return 0;
}


/*   Helper function to pass to tputs  */
static int
local_output_char(c)
  int c;
{
  return putc(c, stderr);
}


/* Input processing functions */

/* Process Right arrow key
   If field displays from a value list display the next value wrapping to start
   Otherwise progress cursor to right. */
static int
cf_right_arrow_key(count, c)
  int count;
  int c;
{
  SCREENFIELD *screenfield ;

  if ( cf_screenform )
  {
    screenfield = cf_screenform->currentscreenfield;
    /* If value to display mapping */
    if ( screenfield->fieldspec->valuescount > 1)
    {
      if ( screenfield->currentvalueindex <
          (screenfield->fieldspec->valuescount-1) )
      {
        screenfield_setwithindex(screenfield,
          screenfield->currentvalueindex + 1);
      }
      else
      {
        screenfield_setwithindex(screenfield,
          0);
      }
      rl_replace_line(screenfield->displayvalue, 0);
      rl_point = rl_end;
      rl_mark = 0;
      return 0;
    }
#if defined(SELECTFROMCOMPLETION)
    else if (screenfield->completionlist)
    {
      if ( screenfield->completioncurrentindex == -1 )
      {
        screenfield->completioncurrentindex =
          screenfield->completionstartindex;
      }
      else if ( screenfield->completioncurrentindex <
        (screenfield->completionlist->list_len - 1) )
      {
        screenfield->completioncurrentindex++;
      }
      screenfield_setwithdisplayvalue(screenfield,
        screenfield->completionlist
          ->list[screenfield->completioncurrentindex]
          , 0);
      rl_replace_line(screenfield->displayvalue, 0);
      rl_point = rl_end;
      rl_mark = 0;
      return 0;

    }
#endif
  }
  /* Otherwise */
  return rl_forward_char (count, c);
}

/*  Update the screenfield to display value */
static void
cf_next_value(screenfield)
SCREENFIELD *screenfield;
{
  if ( screenfield->currentvalueindex > 0  )
  {
    screenfield_setwithindex(screenfield,
      screenfield->currentvalueindex - 1);
  }
  else
  {
    screenfield_setwithindex(screenfield,
      screenfield->fieldspec->valuescount - 1);
  }
}


/* Process Left arrow key
   If field displays from a value list display the previous value wrapping to end
   Otherwise progress cursor to left. */
static int
cf_left_arrow_key(count, c)
  int count;
  int c;
{
  SCREENFIELD *screenfield;

  if ( cf_screenform )
  {
    screenfield = cf_screenform->currentscreenfield;
    /* If value to display mapping */
    if ( screenfield->fieldspec->valuescount > 1)
    {
      if ( screenfield->currentvalueindex > 0
          )
      {
        screenfield_setwithindex(screenfield,
          screenfield->currentvalueindex - 1);
      }
      else
      {
        screenfield_setwithindex(screenfield,
          screenfield->fieldspec->valuescount - 1);
      }
      rl_replace_line(screenfield->displayvalue, 0);
      rl_point = rl_end;
      rl_mark = 0;
      return 0;
    }
#if defined(SELECTFROMCOMPLETION)
    else if (screenfield->completionlist)
    {
      if ( screenfield->completioncurrentindex == -1 )
      {
        screenfield->completioncurrentindex =
          screenfield->completionnextindex-1;
      }
      if ( screenfield->completioncurrentindex > 0)
      {
        screenfield->completioncurrentindex--;
        screenfield_setwithdisplayvalue(screenfield,
          screenfield->completionlist
            ->list[screenfield->completioncurrentindex]
            , 0);
        rl_replace_line(screenfield->displayvalue, 0);
        rl_point = rl_end;
        rl_mark = 0;
        return 0;
      }

    }
#endif
  }
    return rl_backward_char (count, c);
}

/*  Process up arrow key
    Set navigation to previous field and complete entry of the field. */
static int
cf_up_arrow_key(count, c)
  int count;
  int c;
{
  if ( cf_screenform )
  {
    cf_screenform->nextnavigation = CF_PREVIOUS_FIELD;
  }
    return rl_newline (count, c);
}

/*  Process down arrow key
  Set navigation to next field and complete entry of the field. */
static int
cf_down_arrow_key(count, c)
  int count;
  int c;
{
  if ( cf_screenform )
  {
    cf_screenform->nextnavigation = CF_NEXT_FIELD;
  }
    return rl_newline (count, c);
}

/* Process redraw key sequence - Ctrl L
  Cycle display level and redraw the form */
static int
cf_redraw(count, c)
  int count;
  int c;
{
  if ( cf_screenform )
  {
    cf_screenform->nextnavigation = CF_REDRAW;
  }
    return rl_newline (count, c);
}

/* Process key press.
    Called from the keymap table during readline.
    The function either inserts the type character or if
    the field has a display value list, it navigates between displayvalue. */
static int
cf_insert_or_cycle_screenfield (count, c)
     int count;
     int c;
{
  SCREENFIELD *screenfield ;
  char buff[2];
  int valueindex;

  buff[0] = (char)c;
  buff[1] = '\0';


  if ( cf_screenform )
  {
    screenfield = cf_screenform->currentscreenfield;
    /* If value to display mapping */
    if ( screenfield->fieldspec->valuescount > 1)
    {
      if ( c == ' ')
      {
        if ( screenfield->currentvalueindex == -1)
        {
          screenfield_setwithindex(screenfield, 0);
        }
        else if ( screenfield->currentvalueindex <
            (screenfield->fieldspec->valuescount-1) )
        {
          screenfield_setwithindex(screenfield,
            screenfield->currentvalueindex + 1);
        }
        else
        {
          screenfield_setwithindex(screenfield,
            0);
        }
      }
      else
      {
        /* Set with partial match */
        valueindex = fieldspec_displayvalueindex(screenfield->fieldspec,
            buff, 1);
        if ( valueindex != -1)
        {
          screenfield_setwithindex(screenfield, valueindex);
        }
        else
        {
          rl_ding();
        }
      }
      rl_replace_line(screenfield->displayvalue, 0);
      rl_point = rl_end;
      rl_mark = 0;
      return 0;
    }
  }

  /* Otherwise */
  return rl_insert(count, c);
}

/* Callback function call by display_matches (readline library) to display the
   possible matches on the hint line for automatic completion */
static void
cf_display_matches(matches, length, max)
    char **matches;
    int length;
    int max;
{
  STRINGLIST *sl;
  char *buff;
  SCREENFIELD *screenfield;
  SCREENFORM *screenform;
  FIELDSPEC *field;
  int i;
  int len;

  if ( ! cf_screenform )
    return;

  /* Context */
  screenform = cf_screenform;
  field = screenform->currentscreenfield->fieldspec;
  screenfield = screenform->currentscreenfield;

  /* Set up buffer */
  buff = xmalloc((unsigned int)(screenform->width+1));
  buff[0] = '\0';
  len = 0;

  /* First in list of displayed */
  if ( screenfield->completionlist )
  {
    if ( screenfield->completionnextindex >=
          screenfield->completionlist->list_len )
      screenfield->completionstartindex = 0;
    else
      screenfield->completionstartindex =
            screenfield->completionnextindex;
    for ( i = screenfield->completionstartindex,
        sl = screenfield->completionlist; i < sl->list_len; i++)
    {
      len += strlen(sl->list[i]) + 1;
      if ( len >=  screenform->width )
      {
        break;
      }
      else
      {
        strcat(buff, sl->list[i]);
        strcat(buff, " ");
      }
    }
    screenfield->completionnextindex = i;
  }
  /* exit displaying buffer */
  screenform_hint(screenform, buff, cf_edit_mode);
  free(buff);

    rl_forced_update_display ();

}

/* Call by rl_completion_match (readline library) to pick up
   all possible completions */
static char *
cf_complete_return(text, state)
  const char *text;
  int state;
{
  static int valueindex = 0;
  char *string;

  if ( state == 0)
    valueindex = 0;

  if ( cf_screenform && cf_screenform->currentscreenfield &&
    cf_screenform->currentscreenfield->completionlist &&
    valueindex < (cf_screenform->currentscreenfield
        ->completionlist->list_len ))
  {
    /* Not sure if we need to dupe here */
      string =
        STRDUP(cf_screenform->currentscreenfield->completionlist->list[valueindex]);
      valueindex ++;
      return string;
  }
  else
    return NULL;
}

/* Called via rl_attempted_completion global variable
   Calculates a list of possible matches and the lowest common denominator
   and sets up if required the display of options on the hint line. */
static char **
cf_completion(text, start, end)
  const char *text;
  int start;
  int end;
{
  SCREENFIELD *screenfield;
  FIELDSPEC *field;
  int newlist;
  COMPSPEC *cs;
  STRINGLIST *sl = NULL;
  char **matches;
  static char *value = NULL;


  /* Sanity check */
  if ( ! cf_screenform )
    return NULL;

  /* Context */
  screenfield = cf_screenform->currentscreenfield;

  field = screenfield->fieldspec;

  newlist = 1;

  /* Locate */
  if ( value )
  {
    if ( strcmp(value, text) != 0 )
    {
      /* Value changed so save new value */
      free(value);
      value = STRDUP(text);
    }
    else
    {
      /* Display next from completion list */
      newlist = 0;
    }
  }
  else
  {
    value = STRDUP(text);
  }

  /* If newlist required then dispose of old list */
  if ( newlist && screenfield->completionlist)
  {
    strlist_dispose(screenfield->completionlist);
    screenfield->completionlist = 0;
    screenfield->completioncurrentindex = -1;
  }

  if ( newlist || !screenfield->completionlist )
  {
      /* FIXME what is found */
      int found = 0;
    if ( field->compspec) {
      cs = progcomp_search(field->compspec);
      if ( cs ) {
        sl = gen_compspec_completions(cs, "compgen",
            text, 0, 0, &found);
      }
      else
      {
        sl = NULL;
        fprintf(stderr, "Invalid completion spec for field %s\n",
          field->compspec);
      }

    }
    else
    {
      /* No completion spec so files and directories */
      cs = compspec_create();
      cs->actions = CA_FILE | CA_DIRECTORY;
      sl = gen_compspec_completions(cs, "compgen",
          text, 0, 0, &found);
      compspec_dispose(cs);
    }
    screenfield->completionlist = sl  ;
    screenfield->completionstartindex = 0  ;
    screenfield->completionnextindex = 0  ;
    screenfield->completioncurrentindex = -1  ;
  }
  else
  {
    screenfield->completionstartindex = screenfield->completionnextindex;
    if ( screenfield->completionstartindex >=
        screenfield->completionlist->list_len )
    {
      screenfield->completionstartindex = 0;
    }
  }


  /* */
  matches = rl_completion_matches(text, cf_complete_return);
  return matches;

}

/* Recursively clone a keymap. If an entry in a keymap is
   a map entry that the function pointer is to a keymap. Clone
   that keymap as well.*/
static Keymap
clone_keymap(keymap)
  Keymap keymap;
{
  Keymap new_keymap;
  int i;

    new_keymap = rl_make_bare_keymap ();

    /* Copy each entry */
    for (i = 0; i < KEYMAP_SIZE; i++)
    {
      /* Copy entry */
      new_keymap[i] = keymap[i];

      /* If the entry points to another map then
         clone that map */
      if ( new_keymap[i].type == ISKMAP)
      {
        /* Warning: Recursive call */
        new_keymap[i].function =
          (rl_command_func_t *)
            (clone_keymap((Keymap)(keymap[i].function)));
      }
    }
    return (new_keymap);

}

/* Initialise the keymap to be used for form input by cloning
   the current keymap */
static void
cf_initialize_readline()
{
  static Keymap base_keymap = NULL;
  Keymap current_keymap;
  int i;


  current_keymap = rl_get_keymap();

  /* if base keymap still current - do nothing */
  if ( base_keymap != NULL
    && base_keymap == current_keymap)
    return;

  /* if a different keymap is used free the command forms keymap */
  if ( base_keymap != NULL
    && base_keymap != current_keymap
    && cf_keymap != NULL)
  {
    /* A different keymap is being used - recalculate keymap */
    rl_discard_keymap(cf_keymap);
    cf_keymap = NULL;
  }

  /* Record the keymap we based things on */
  base_keymap = current_keymap;

  /* Setup form keymaps by cloning current keymap */
  cf_keymap = clone_keymap(current_keymap);
  rl_set_keymap(cf_keymap);

  for (i = 0; i < KEYMAP_SIZE; i++)
  {
      /* Replace rl_insert with handler for navigation */
    if ( current_keymap[i].function ==
      rl_insert)
    {
      cf_keymap[i].function = cf_insert_or_cycle_screenfield;
    }
    }

  /*
    Set special handling for arrow keys in cloned key map
   */
  if ( _rl_term_ku )
    rl_set_key(_rl_term_ku, cf_up_arrow_key, cf_keymap);
  if ( _rl_term_kd )
    rl_set_key(_rl_term_kd, cf_down_arrow_key, cf_keymap);
  if ( _rl_term_kr )
    rl_set_key(_rl_term_kr, cf_right_arrow_key, cf_keymap);
  if ( _rl_term_kl )
    rl_set_key(_rl_term_kl, cf_left_arrow_key, cf_keymap);

  /*
    Set arrow keys for common psuedo ansi terminal keypad
    as a catch all if the TERM variable is not set or
    badly defined terminfo
    XXXX should us "if unbound"
  */
  rl_set_key( "\033[A", cf_up_arrow_key , cf_keymap);
  rl_set_key( "\033[B", cf_down_arrow_key, cf_keymap );
  rl_set_key( "\033[C", cf_right_arrow_key, cf_keymap );
  rl_set_key( "\033[D", cf_left_arrow_key, cf_keymap );
  rl_set_key( "\033OA", cf_up_arrow_key, cf_keymap );
  rl_set_key( "\033OB", cf_down_arrow_key, cf_keymap );
  rl_set_key( "\033OC", cf_right_arrow_key, cf_keymap);
  rl_set_key( "\033OD", cf_left_arrow_key, cf_keymap);

  /* Set redraw */
  cf_keymap[CTRL('l')].function = cf_redraw;
  cf_keymap[CTRL('l')].type = ISFUNC;;


  rl_set_keymap(current_keymap);

  /* Mark as initialized */
  cf_readline_initialized = 1;
}

/* Set up the read line environmnent and call readline.
   Readline will redraw the label of the current screen field
   from the start of the line and will process input.

   A call back function is used to populate the readline input
   with the display value of the field.

   Functions call from the "keymap" set the "next navigation" field
   of the global cf_screenform. This communicates the processing
   of up and down arrows to navigate between screen fields.

   The screen field value and display value are set from the
   string returned by readline.*/
void
screenform_editscreenfield(screenform, edit_mode)
  SCREENFORM *screenform;
  CF_EDIT_MODE edit_mode;
{
  char *ret;
  rl_completion_func_t *old_attempted_completion_function;
  rl_compdisp_func_t *old_completion_display_matches_hook;
  Keymap old_keymap;
  int old_rl_horizontal_scroll_mode;

  if (!bash_readline_initialized)
    initialize_readline ();

  if ( !cf_readline_initialized )
    cf_initialize_readline();

  /* Save readline state */
  old_keymap = rl_get_keymap();
  old_attempted_completion_function = rl_attempted_completion_function;
  old_completion_display_matches_hook = rl_completion_display_matches_hook;
  old_rl_horizontal_scroll_mode = _rl_horizontal_scroll_mode;


  /* Save the startup hook - restored in call back */
  old_rl_startup_hook = rl_startup_hook;

  /* Set call back to display contents of field */
  rl_startup_hook = screenform_displayvalue;

  /* Set custom completion functions */
  rl_attempted_completion_function = cf_completion;
  rl_completion_display_matches_hook = cf_display_matches;
  cf_edit_mode = edit_mode;

  /* XXXXXX Set required key map */
  rl_set_keymap(cf_keymap);

  /* Set horizontal scroll mode */
  _rl_horizontal_scroll_mode = 1;

  /* Read line of input redisplaying field label */
  ret = readline(screenform->currentscreenfield->label);

  /* Restore readline state */
  rl_attempted_completion_function = old_attempted_completion_function;
  rl_set_keymap(old_keymap);
  _rl_horizontal_scroll_mode = old_rl_horizontal_scroll_mode;
  rl_completion_display_matches_hook = old_completion_display_matches_hook;

  /* Set the value using the displayed data */
  screenfield_setwithdisplayvalue(screenform->currentscreenfield, ret, 0);
}

/*
  Screen form functions
*/

/* This function is called from within readline and "pretends" to have
  "inserted" the value of the current screen form field.
   The effect is to display the value of the field and position the
   cursor at the start of the field. The use can then use the readline edit
   commands to edit the value. */
static int
screenform_displayvalue ()
{
  if (cf_screenform->currentscreenfield->value)
    {
      rl_insert_text (cf_screenform->currentscreenfield->displayvalue);
    /* If field wider than display width set edit point at start of field */
    if ( (strlen(cf_screenform->currentscreenfield->displayvalue)
      +  cf_screenform->maxlabelwidth + 3)
      > cf_screenform->width )
      rl_point = 0;
    }
  rl_startup_hook = old_rl_startup_hook;
  return 0;
}

/* Set the value of a screen field according to the index into the
   table of allowable values for the field. The display value is set
   and the value for generation of the command. */
static void
screenfield_setwithindex(screenfield, valueindex)
  SCREENFIELD *screenfield;
  int valueindex;
{
  FIELDSPEC *fieldspec;

  fieldspec = screenfield->fieldspec;

  /* Free existing values */
  if ( screenfield->value )
  {
    free(screenfield->value);
    screenfield->value = NULL;
  }
  if ( screenfield->displayvalue )
  {
    free(screenfield->displayvalue);
    screenfield->displayvalue = NULL;
  }

  if ( valueindex < fieldspec->valuescount &&
    valueindex > -1 )
  {
    screenfield->currentvalueindex = valueindex;
    screenfield->value = STRDUP(fieldspec->values[valueindex]);
    screenfield->displayvalue = STRDUP(fieldspec->displayvalues[valueindex]);
  }
  else
  {
  fprintf(stderr, "\n\n\n\n screenfield_setwithindex - invalid index %d where  valuescount is %d\n", valueindex, fieldspec->valuescount);
  }

}

/* Set the value of a screen field from the value to be place in the generated
   command. If there is a translation between "value" and "display value" apply
   the translation to populate the display value of the field. */
static void
screenfield_setwithvalue(screenfield, value)
  SCREENFIELD *screenfield;
  char *value;
{
  FIELDSPEC *fieldspec;
  int valueindex;

  fieldspec = screenfield->fieldspec;

  /* Free existing values */
  if ( screenfield->value )
  {
    free(screenfield->value);
    screenfield->value = NULL;
  }
  if ( screenfield->displayvalue )
  {
    free(screenfield->displayvalue);
    screenfield->displayvalue = NULL;
  }

  /* Set value and default index */
  screenfield->value = STRDUP(value);

  screenfield->currentvalueindex = -1;

  /* If there is a display translation check if value matches */
  if ( fieldspec->valuescount > 1 )
  {
    valueindex = fieldspec_valueindex(fieldspec, value);
    if ( valueindex != -1 )
    {
      screenfield->displayvalue =
        STRDUP(fieldspec->displayvalues[valueindex]);
      screenfield->currentvalueindex = valueindex;
    }
    else
    {
      screenfield->displayvalue = STRDUP(value);
    }
  }
  else
  {
    screenfield->displayvalue = STRDUP(value);
  }
}

/* Set the value of a screen field from the value to be displayed. If there
   is a translation between "value" and "display value" apply the reverse to
   populate the value of the field. */
static void
screenfield_setwithdisplayvalue(screenfield, displayvalue, partial)
  SCREENFIELD *screenfield;
  char *displayvalue;
  int partial;
{
  FIELDSPEC *fieldspec;
  int valueindex;

  fieldspec = screenfield->fieldspec;

  /* Free existing values */
  if ( screenfield->value )
  {
    free(screenfield->value);
    screenfield->value = NULL;
  }
  if ( screenfield->displayvalue )
  {
    free(screenfield->displayvalue);
    screenfield->displayvalue = NULL;
  }

  /* Set value and default index */

  /* If there is a display translation check if value matches */
  if ( fieldspec->valuescount > 1 )
  {
    valueindex = fieldspec_displayvalueindex(fieldspec, displayvalue, partial);
    if ( valueindex != -1 )
    {
      screenfield_setwithindex(screenfield, valueindex);
      return;
    }
  }

  /* Otherwise  set everything as entered */
  screenfield->currentvalueindex = -1;
  screenfield->displayvalue = STRDUP(displayvalue);
  screenfield->value = STRDUP(displayvalue);
}

/* Append a string to a screen field */
static void
screenfield_appendtovalue(screenfield, value)
  SCREENFIELD *screenfield;
  char *value;
{
  int length;
  char *oldvalue;
  char *newvalue;

  if ( screenfield->value)
  {
    length = strlen(screenfield->value) + strlen(value) + 2;
    oldvalue = screenfield->value;
    newvalue = xmalloc((unsigned int)length);
    strcpy(newvalue, oldvalue);
    strcat(newvalue, " ");
    strcat(newvalue, value);
    screenfield_setwithvalue(screenfield, newvalue);
    free(newvalue);
  }
  else
  {
    screenfield_setwithvalue(screenfield, value);
  }
}
/* Populate the screen field values from the passed in arguments and
  set unspecificed arguments to their defaults. */
void
screenform_populatefieldsfrompartialcommand(screenform, list)
  SCREENFORM *screenform;
  WORD_LIST *list;
{

WORD_LIST *l;
  FORMFIELDSPEC **fieldlist;
  FORMFIELDSPEC **fl;
  FIELDSPEC *fieldspec;
  SCREENFIELD *screenfield;
  int valueindex;
  int found;


  fieldlist = screenform->displaylevel->generationfieldlist;
  for (l = list->next; l && *fieldlist; l = l->next)
  {
    fieldspec = (*fieldlist)->fieldspec;
    screenfield = (*fieldlist)->screenfield;

    /* If next fieldspec is a flag then try to match
       the argument to any flags before the next positional argument */

    if ( fieldspec->fieldtype == CF_FIELD_TYPE_FLAG
      || CF_FIELD_TYPE_FLAGWITHVALUE)
    {
      /* Check for matching flag arguments prior to next positional */
      found = 0;

      for (fl = fieldlist; *fl &&
        ( (*fl)->fieldspec->fieldtype == CF_FIELD_TYPE_FLAG ||
         (*fl)->fieldspec->fieldtype == CF_FIELD_TYPE_FLAGWITHVALUE); fl++)
      {
        fieldspec = (*fl)->fieldspec;
        screenfield = (*fieldlist)->screenfield;

        /* Test for matching flag value */
        if ( fieldspec->fieldtype == CF_FIELD_TYPE_FLAGWITHVALUE
            &&  fieldspec->flag)
        {
          int flaglen = strlen(fieldspec->flag);

          if ( strcmp(fieldspec->flag, l->word->word) == 0 ||
               (fieldspec->flag[flaglen-1] == ' ' &&
              strncmp(fieldspec->flag, l->word->word,
                flaglen-1) == 0) )
          {
            /* Should be another value */
            if ( l->next )
            {
              /* Consume value */
              screenfield_setwithvalue(screenfield,
                l->next->word->word);
              l = l->next;
            }
            /* Consume argument
              but don't progress field list pointer
              as there may be other flags */
            found = 1;
            break;
          }
          else if ( strncmp(fieldspec->flag, l->word->word,
                flaglen) == 0)
          {
            /* split flag from word and set value */
            screenfield_setwithvalue(screenfield,
              l->word->word + flaglen);
            found = 1;
            break;
          }

        }
        else
        {
          /* Check whether it matches the on or off values */
          valueindex = fieldspec_valueindex(fieldspec, l->word->word) ;
          if ( valueindex != -1 )
          {
            /* Set the value of the field */
            screenfield_setwithindex(screenfield, valueindex);

            /* Consume argument
              but don't progress field list pointer
              as there may be other flags */
            found = 1;
            break;
          }
        }
      }
      /* If found then check next argument against flags */
      if ( found )
        continue;

      if ( !*fl)
      {
        /* No more positional arguments -
          all trailing */
        fieldlist = fl;
        break;
      }
      else
      {
        /* No matching flags so position to fieldspec  */
        fieldlist = fl;
        fieldspec = (*fieldlist)->fieldspec;
        screenfield = (*fieldlist)->screenfield;
        /* Drop through */
      }
    }

    /* If a position argument is next then use the value */
    if ( fieldspec->fieldtype == CF_FIELD_TYPE_UPTOLAST)
    {
      /* If not last argument append to "UPTOLAST" field */
      if ( l->next != NULL )
      {
        screenfield_appendtovalue(screenfield, l->word->word);
        /* Note: Don't progress fieldlist */
      }
      else
      {
      /* Last argument so If next field is last field apply to that field */
        if ( *(fieldlist+1) &&
          (*(fieldlist+1))->fieldspec->fieldtype == CF_FIELD_TYPE_LAST)
        {
          screenfield_setwithvalue(
            (*(fieldlist+1))->screenfield,l->word->word);
        }
        else
        {
      /* Note: if no  LAST field after UPTOLAST then add to UPTOLAST */
          screenfield_appendtovalue(screenfield, l->word->word);
        }
      fieldlist++;
      }
    }
    else if ( fieldspec->fieldtype == CF_FIELD_TYPE_POSITIONAL
      || fieldspec->fieldtype == CF_FIELD_TYPE_LAST)
    {
      /* Set value */
      screenfield_setwithvalue(screenfield, l->word->word);
      fieldlist++;
    }

    else if ( fieldspec->fieldtype == CF_FIELD_TYPE_REST)
    {
      /* Append all the rest to the field */
      screenfield_appendtovalue(screenfield, l->word->word);
      /* Note: Don't progress fieldlist */
    }
    else
      fprintf(stderr, "\n\n\n\nInvalid field type %d\n\n\n\n",
        fieldspec->fieldtype);
  }

  /* Set default values for unspecified fields */
  for ( fieldlist = screenform->displaylevel->generationfieldlist ; *fieldlist; fieldlist++)
  {
    fieldspec = (*fieldlist)->fieldspec;
    screenfield = (*fieldlist)->screenfield;

    /* In value not populated then set default value */
    if ( screenfield->value == NULL )
      {
      if ( fieldspec->values )
        screenfield_setwithvalue(screenfield, fieldspec->values[0]);
      }


  }
}

/* Determine the length of the generated argument */
int
screenfield_generatedargumentlength(screenfield)
  SCREENFIELD *screenfield;
{
  int len;

    len = 0;

    /* Length of flag */
    if ( screenfield->fieldspec->flag)
      len += strlen(screenfield->fieldspec->flag);

    /* Length of trailing separator */
    if ( screenfield->fieldspec->separator)
      len += strlen(screenfield->fieldspec->separator);
    else
      /* or default space */
      len++;

    /* Length of value - if value then flag otherwise nothing */
    if ( screenfield->value && strlen(screenfield->value) > 0)
      return len + strlen(screenfield->value);
    else
      /* If not value do not include length of flag */
      return 0;
}

/* Navigate to the next displayable screen field and make that field current
   Reports if there are no subsequent displayable fields. */
int screenform_gotonextfield(screenform, edit_mode)
  SCREENFORM *screenform;
  CF_EDIT_MODE edit_mode;
{
SCREENFIELD *screenfield;
SCREENFIELD *endofscreenfields;

  screenfield = screenform->currentscreenfield;
  endofscreenfields = (screenform->fieldcount-1) + screenform->screenfields;

  /* Find next displayable field */
  ++screenfield;
  if (screenfield <= endofscreenfields)
  {
    screenform_gotofield(screenform, screenfield, edit_mode);
    return 1;
  }
  else
    return 0;
}

/* Navigate to the previous displayable screen field and make that field current */
void
screenform_gotopreviousfield(screenform, edit_mode)
  SCREENFORM *screenform;
  CF_EDIT_MODE edit_mode;
{
  SCREENFIELD *screenfield;

  screenfield = screenform->currentscreenfield;

  if ( screenfield == screenform->screenfields )
  {
    screenform_gotofield(screenform, screenfield, edit_mode);
    rl_ding();
    return;
  }
  else
  {
    screenfield--;
    screenform_gotofield(screenform, screenfield, edit_mode);
  }
}

/* Generate the text to be put into the generated command for the screenfield */
char *
screenfield_generateargument(screenfield)
  SCREENFIELD *screenfield;
{
  char *argument;
  int len;

    if ( screenfield->value && (strlen(screenfield->value) > 0))
    {
      len = strlen(screenfield->value);
      if ( screenfield->fieldspec->flag )
      {
        len += strlen(screenfield->fieldspec->flag);
        argument = xmalloc((unsigned int)len+1);
        strcpy(argument, screenfield->fieldspec->flag);
        strcat(argument, screenfield->value);
      }
      else
      {
        argument = xmalloc((unsigned int)len+1);
        memcpy(argument, screenfield->value, len+1);
      }
      return argument;
    }
    else
      return NULL;
}



/* Change the current field and position the cursor to that the line of that
  field either using <CR> or UP character sequences. */
void
screenform_gotofield(screenform, screenfield, edit_mode)
  SCREENFORM *screenform;
  SCREENFIELD *screenfield;
  CF_EDIT_MODE edit_mode;
{
int i;
int delta;

  delta = screenform->currenty - screenfield->y;
  if ( delta > 0 )
  {
    if (edit_mode == CF_MODE_FORM &&   UP && *UP)
      {
        for( i=0; i < delta; i++)
          tputs(UP, 1, local_output_char);
      }
  }
  else
  {
    for( i=0; i < -delta; i++)
    {
      putc('\n', stderr);
    }
  }
  screenform->currentscreenfield = screenfield;
  screenform->currenty = screenfield->y;
}

/* Draw text on the hint line of the screen by <CR> to
   go to the hint line, drawing the text up to the screen width
   and using the UP character sequence to go back to the line
   you first started on. */
static void
screenform_hint(screenform, hint, edit_mode)
  SCREENFORM *screenform;
  char *hint;
  CF_EDIT_MODE edit_mode;
{
int i;
int delta;

    if ( edit_mode == CF_MODE_FORM &&
       UP && *UP)
    {
      delta = (screenform->height - screenform->currenty) - 1;
    /* Go to hint line */
      for( i=0; i < delta; i++)
      {
        putc('\n', stderr);
      }

    /* Output hint on bottom line  - limit to screen width */
      i = strlen(hint) >
          screenform->width ?
        screenform->width :
        strlen(hint);
      fwrite(hint,
        1, (unsigned int)i, stderr);
    /* Clear to end of line */
      for ( i =
        strlen(hint);
        i <screenform->width; i++)
        putc(' ', stderr);

    /* Go back to field */
      for( i=0; i < delta; i++)
        tputs(UP, 1, local_output_char);
    }
    else
    {
    /* If not UP suppported then just print the help text
        - do not care if it goes over width of screen */
      fputs(hint, stderr);
      putc('\n', stderr);
    }
  /* Return to first column */
  /* Note that editscreenfield will redraw label */
    putc('\r', stderr);
}

/* Draw the hint text for currently displayed screen field */
void
screenform_displayhinttext(screenform, edit_mode)
  SCREENFORM *screenform;
  CF_EDIT_MODE edit_mode;
{

  if ( screenform->currentscreenfield->fieldspec->hinttext)
  {
    screenform_hint(screenform,
      screenform->currentscreenfield->fieldspec->hinttext,
      edit_mode);
  }

}
/* Initial drawing of complete screen form. Cursor is moved to first field. */
void
screenform_draw(screenform)
  SCREENFORM *screenform;
{
SCREENFIELD *screenfield;

char *buff;
char *string;
int len;
char *cp;
int i;
int y;




/* Display top line */
  buff = xmalloc((unsigned int)(screenform->width + 1));
  memset(buff, '-', (unsigned int)(screenform->width));
  buff[screenform->width] = '\0';
  len = strlen(screenform->label);
  cp = buff + ( (screenform->width/2) - (len/2) );
  if ( cp < buff)
  {
    cp = buff;
    len = screenform->width;
  }
  memcpy(cp, screenform->label, (unsigned int)len);

  /* Output string */
  fputs(buff, stderr);
  rl_crlf();


/* Display each field */

  y= 0;
  for(screenfield= screenform->screenfields, i=0; i<screenform->fieldcount ; screenfield++, i++)
  {
      /* Go to correct line */
      while(y < screenfield->y)
      {
        rl_crlf();
        y++;
      }
      /* Display label */
      fputs(screenfield->label, stderr);

      /* Display value */
      if ( screenfield->displayvalue)
      {
        len = strlen(screenfield->displayvalue) ;
        if ( (len + screenform->maxlabelwidth + 4) >
          screenform->width)
        {
          len = screenform->width -
            screenform->maxlabelwidth - 5;
          fwrite(screenfield->displayvalue,
            1, (unsigned int)len, stderr);
          putc('>', stderr);
        }
        else
        {
          fwrite(screenfield->displayvalue,
            1, (unsigned int)len, stderr);
        }
      }

      /* Go to next line */
      rl_crlf();
      y++;

      /* If field is a LAST and not last field */
      if ( (screenfield->fieldspec->fieldtype == CF_FIELD_TYPE_LAST
          || screenfield->fieldspec->fieldtype == CF_FIELD_TYPE_REST)
        && i < (screenform->fieldcount-1) )
      {
        memset(buff, ' ', (unsigned int)(screenform->width));
        memset(buff, '+', (unsigned int)(screenform->maxlabelwidth));
        buff[screenform->width] = '\0';
        string = "flags";
        len = strlen(string);
        cp = buff + ( (screenform->maxlabelwidth/2) - (len/2) );
        if ( cp < buff)
        {
          cp = buff;
        }
        memcpy(cp, string, (unsigned int)len);

        /* Output string */
        fputs(buff, stderr);
        rl_crlf();
        y++;
      }

  }

/* add a line for the hint text */
  memset(buff, '-', (unsigned int)(screenform->width));
  len = strlen(screenform->displaylevel->displaylevel);
  if (len > screenform->width)
    len = screenform->width/2;

  memcpy(buff + (screenform->width - len), screenform->displaylevel->displaylevel, (unsigned int)len);
  fputs(buff, stderr);
  rl_crlf();
  y++;
  rl_crlf();
  y++;

  /* Position to first field */
  screenform->currentx = 0;
  screenform->currenty = screenform->height;

  free(buff);
}

/* Layout the fields of the form for the current screen width */
void
screenform_layout(screenform)
  SCREENFORM *screenform;
{
FORMFIELDSPEC **fieldlist;
SCREENFIELD *screenfield;
FIELDSPEC *field;
int labelwidth;
int maxlabelwidth;
int y;
int i;
char *ap;
char *bp;

int rows;
int cols;



/* Get size of screen */

rl_get_screen_size(&rows, &cols);

/* Layout form */

/* 1. Set up screen fields and determine label width */
  maxlabelwidth = 0;
  for(fieldlist=screenform->displaylevel->screenfieldlist, screenfield= screenform->screenfields;
       *fieldlist; fieldlist++, screenfield++)
  {
    field = (*fieldlist)->fieldspec;
    /* Cross link screenfield and fieldspec */
    (*fieldlist)->screenfield = screenfield;
    /* Cross link matching generationfieldlist entry */
    (*fieldlist)->crosslink->screenfield = screenfield;
    screenfield->fieldspec = field;
    labelwidth = strlen(field->label);
    if (labelwidth > maxlabelwidth)
      maxlabelwidth = labelwidth;
  }
  /* hardcoded esthetic
    IF label width greater than a third of screen width
        set label width to 40 */
  if ( maxlabelwidth > (cols / 3) )
    if ( maxlabelwidth > 40)
      maxlabelwidth = 40;

  screenform->maxlabelwidth = maxlabelwidth;
/* 2. Layout each field  - right align field labels */
  y= 0;
  for(fieldlist=screenform->displaylevel->screenfieldlist, screenfield= screenform->screenfields; *fieldlist; fieldlist++, screenfield++)
  {
    field = (*fieldlist)->fieldspec;
    if ( screenfield->label )
      free(screenfield->label);
    screenfield->label= xmalloc((unsigned int)(maxlabelwidth+2));
    memset(screenfield->label, ' ', (unsigned int)maxlabelwidth);
    labelwidth= strlen(field->label);
    /* Watch for large labels */
    if ( labelwidth > maxlabelwidth)
      labelwidth = maxlabelwidth;
    /* Right justify label */
    for( i=0, ap= field->label,
        bp= screenfield->label+(maxlabelwidth-labelwidth);
        *ap && i < maxlabelwidth;
          bp++, ap++, i++
          )
      *bp = *ap;

    /* Add delimiter */
    *bp++ = ':';
    *bp++ = ' ';
    *bp++ = '\0';
    screenfield->x = 0;
    screenfield->y = y;
    screenfield->inputy = y++;

    /* last field level space for separator */
    if (( field->fieldtype == CF_FIELD_TYPE_LAST
            || field->fieldtype == CF_FIELD_TYPE_REST )
            && *(fieldlist+1) )
      y++;

    screenfield->inputx = maxlabelwidth + 1;
    screenfield->height = 1;
    /* XXXX defaulting to screen width */
    screenfield->width  = cols - maxlabelwidth - 1;
    /* Note: defaults set by screenform_polutatefrompartialcommand */

  }
  /*  Add line for hints */
  screenform->height = y+2;
  screenform->width = cols;
}

/* Instansiate a screen form, laying out the fields */
SCREENFORM *
screenform_init(formspec, displaylevel, label)
  FORMSPEC *formspec;
  DISPLAYLEVEL *displaylevel;
   char *label;
{
  SCREENFORM *screenform;

  /* Create SCREENFORM and SCREENFIELDs */
  screenform =  xmalloc(sizeof *screenform);
  memset(screenform, 0, sizeof(SCREENFORM) );

  screenform->displaylevel = displaylevel;

  /* Allocate screen fields */
  screenform->screenfields = (SCREENFIELD *)xmalloc( (unsigned int)(screenform->displaylevel->fieldcount * sizeof(SCREENFIELD) ) );
  memset(screenform->screenfields, 0, (unsigned int)(screenform->displaylevel->fieldcount * sizeof(SCREENFIELD) ));

  /* Initialising housekeeping */
  screenform->label = savestring(label);
  screenform->fieldcount = screenform->displaylevel->fieldcount;
  screenform->formspec = formspec;

  return screenform;
}

/* Dispose of a screen form */
void
screenform_dispose(screenform)
  SCREENFORM *screenform;
{
  FORMFIELDSPEC **fieldlist;
  SCREENFIELD *screenfield;

   /* 1. loop through the generate field list to construct command arguments */
  for(fieldlist=screenform->displaylevel->generationfieldlist; *fieldlist; fieldlist++)
  {
    screenfield = (*fieldlist)->screenfield;
    free(screenfield->value);
    free(screenfield->displayvalue);
    free(screenfield->label);
  }
  /* 2. Free up screen fields */
  free(screenform->label);
  free(screenform->screenfields);
  cf_screenform = NULL;
  free(screenform);
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
  CONDITIONALSQPRINTSTRING(cs->label, "+label")
  CONDITIONALSQPRINTSTRING(string, "+fieldtype");
  if ( cs->fieldtype == CF_FIELD_TYPE_FLAG)
    string = "flag";
  else if ( cs->fieldtype == CF_FIELD_TYPE_FLAGWITHVALUE)
    string = "flagwithvalue";
  else if ( cs->fieldtype == CF_FIELD_TYPE_POSITIONAL)
    string = "positional";
  else if ( cs->fieldtype == CF_FIELD_TYPE_UPTOLAST)
    string = "uptolast";
  else if ( cs->fieldtype == CF_FIELD_TYPE_LAST)
    string = "last";
  else if ( cs->fieldtype == CF_FIELD_TYPE_REST)
    string = "rest";
  CONDITIONALSQPRINTSTRING(cs->compspec, "+compspec")
  CONDITIONALSQPRINTSTRING(cs->separator, "+separator")
  CONDITIONALSQPRINTSTRING(cs->flag, "+flag")
  if (cs->displayvalues && *(cs->displayvalues))
  {
    printf("+displayvalues ");
    for (pvalue=cs->displayvalues; *pvalue; pvalue++)
    {
      x = sh_single_quote(*pvalue);
      printf("%s ", x);
      free(x);
    }
  }
  if (cs->values && *(cs->values))
  {
    printf("+values ");
    for (pvalue=cs->values; *pvalue; pvalue++)
    {
      x = sh_single_quote(*pvalue);
      printf("%s ", x);
      free(x);
    }
  }
  CONDITIONALSQPRINTSTRING(cs->hinttext, "+hinttext")
  CONDITIONALSQPRINTSTRING(cs->helptext, "+helptext")
  printf ("%s\n", cmd);

  return (0);
}




/* Determine the index into the translation table for a value. */
static int
fieldspec_valueindex(fieldspec, value)
  FIELDSPEC *fieldspec;
  char *value;
{
  int i;
  for (i=0; i<fieldspec->valuescount; i++)
  {
    if (strcmp(fieldspec->values[i], value) == 0 )
      return i;
  }
  return -1;
}

/* Determine the index into the translation table for a display value. */
static int
fieldspec_displayvalueindex(fieldspec, displayvalue, partial)
  FIELDSPEC *fieldspec;
   char *displayvalue;
  int partial;
{
  int i;
  int len = strlen(displayvalue);

  for (i=0; i<fieldspec->valuescount; i++)
  {
    if ( partial )
    {
      if (strncasecmp(fieldspec->displayvalues[i], displayvalue, (unsigned int)len) == 0 )
        return i;
    }
    else
    {
      if (strcmp(fieldspec->displayvalues[i], displayvalue) == 0 )
        return i;
    }
  }
  return -1;
}

/*
  Form specifier functions

*/


/*
  Print a form specifier
 */
int
formspec_print ( formname, form)
    char *formname;
    FORMSPEC *form;
{

  FORMFIELDSPEC **l;
  DISPLAYLEVEL **d;
  char *x;

  printf ("formspec ");

  for (d=form->displaylevels; *d; d++)
    {
    printf("+displaylevel=%s ", (*d)->displaylevel);
    printf("+screenfieldlist ");
    for(l=(*d)->screenfieldlist; *l; l++)
    {
      printf("%s ", (*l)->fieldspecname);
    }
    printf("+generationformlist ");
    for(l=(*d)->generationfieldlist; *l; l++)
    {
      printf("%s ", (*l)->fieldspecname);
    }
  }
  CONDITIONALSQPRINTSTRING(form->command, "+command")
  CONDITIONALSQPRINTSTRING(formname, "+formname")
  printf("\n");
  return (0);
}




#endif /* COMMAND_FORMS */
