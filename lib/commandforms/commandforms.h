/* commandforms.h - structure definitions and other stuff for command
   forms. */

/* Copyright (C) 1999-2003 Free Software Foundation, Inc.

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

#if !defined (_COMMANDFORMS_H_)
#  define _COMMANDFORMS_H_

#include "stdc.h"
#include "hashlib.h"

typedef enum
{
  CF_NO_NAVIGATION,             /* No navigation specified */
  CF_RETURN,                    /* Go to next field  and complete form if at end */
  CF_NEXT_FIELD,                /* Go to next field */
  CF_PREVIOUS_FIELD,            /* Go to previous field */
  CF_REDRAW,                    /* Redraw the form  */
  CF_ENTER,                     /* Complete form */
  CF_ESCAPE                     /* Escape out of form */
} CF_NAVIGATION;

typedef enum
{
  CF_MODE_FORM,
  CF_MODE_LINE
} CF_EDIT_MODE;

typedef enum
{
  CF_MODE_DISPLAY,              /* Display the command */
  CF_MODE_EXECUTE               /* Execute the command */
} CF_EXECUTION_MODE;

/*
	Types of field
 */
typedef enum
{
  CF_FIELD_TYPE_INVALID,
  CF_FIELD_TYPE_POSITIONAL,
  CF_FIELD_TYPE_FLAG,
  CF_FIELD_TYPE_FLAGWITHVALUE,
  CF_FIELD_TYPE_LAST,
  CF_FIELD_TYPE_REST,
  CF_FIELD_TYPE_UPTOLAST
} CF_FIELD_TYPE;

/* Field on a screen when a form is displayed */
struct screenfield
{
  struct fieldspec *fieldspec;
  char *value;
  char *displayvalue;
  char *label;
  int x;
  int y;
  int inputx;
  int inputy;
  int height;
  int width;
  int currentvalueindex;
  STRINGLIST *completionlist;
  int completionstartindex;
  int completionnextindex;
  int completioncurrentindex;
};
typedef struct screenfield SCREENFIELD;

struct screenform
{
  /* Form specification */
  struct formspec *formspec;
  /* Array of screen fields */
  SCREENFIELD *screenfields;
  /* Height in rows displayed */
  int height;
  /* Width in columns displayed */
  int width;
  /* Label to display at top of form */
  char *label;
  /* Number of fields displayed */
  int fieldcount;
  /* Current column position */
  int currentx;
  /* Current row position */
  int currenty;
  /* Width in columns for displaying field labels */
  int maxlabelwidth;
  /* Current screen field */
  SCREENFIELD *currentscreenfield;
  /* After the current action the next navigation action
   * for example move to next field or previous field or complete form.
   * This is asserted by the readline invoked keymap routines for action
   * by the main form input loop.
   */
  CF_NAVIGATION nextnavigation;
  /* Display level within formspec being displayed */
  struct displaylevel *displaylevel;
};
typedef struct screenform SCREENFORM;

/* Field specification for a type of a field */
struct fieldspec
{
  int refcount;
  /* Name of field */
  char *name;
  /* Label to appear on screen */
  char *label;
  /* Type of field */
  CF_FIELD_TYPE fieldtype;
  /* Optional function for command completion */
  char *compspec;
  /* Optional separator string */
  char *separator;
  /* If value present then precede with this flag */
  char *flag;
  /* Values to use for generation of command  - first value is default
	if more than one value then select from list of values */
  int valuescount;
  char **values;
   /* Values to use for displaying list of options if different from above */
  char **displayvalues;
  /* Hint text to display when field selected. Either one string or
   * a string per value  */
  int hinttextcount;
  char **hinttext;
  /* Help text */
  char *helptext;
};
typedef struct fieldspec FIELDSPEC;

/* Display level */
struct displaylevel
{
  /* Name of display level */
  char *displaylevel;
  /* List of fields as they appear on screen */
  FIELDSPEC **screenfieldlist;
  /* List of fields in the order of command generation */
  FIELDSPEC **generationfieldlist;
  /* Could of fields */
  int fieldcount;

};
typedef struct displaylevel DISPLAYLEVEL;

/* Form Specification */
struct formspec
{
  /* Command to execute */
  char *command;
  /* Count of fields */
  int displaylevelcount;
  /* Display levels supported by this form - the first level is the default */
  DISPLAYLEVEL *displaylevels;
};
typedef struct formspec FORMSPEC;

/* Table for definition of pre-defined field types */
struct fieldtypedef
{
  char *type_name;
};
typedef struct fieldtypedef FIELDTYPEDEF;

/*
    Data types
 */
#define FIELDDATATYPE_STRING 1
#define FIELDDATATYPE_NUMBER 2


/*
 */
/*
 Methods of field type flag
 */
char *fielddef_flag_generateargument (FIELDSPEC * fieldspec,
                                      SCREENFIELD * field);
char *fielddef_flag_complete (FIELDSPEC * fieldspec, SCREENFIELD * field);
char *fielddef_flag_defaultvalue (FIELDSPEC * fieldspec, SCREENFIELD * field);

extern FIELDTYPEDEF fieldtypes[];
extern HASH_TABLE *fieldspecs;
extern HASH_TABLE *formspecs;
extern int commandforms_enabled;

/* Functions from commandformslib.c */
extern FIELDSPEC *fieldspec_create __P ((void));

extern void fieldspecs_flush __P ((void));
extern int fieldspec_insert __P ((char *, FIELDSPEC *));
extern void fieldspec_retain __P ((FIELDSPEC *));
extern int fieldspec_remove __P ((char *));
extern int fieldspec_print __P ((char *, FIELDSPEC *));
extern FIELDSPEC *fieldspec_search __P ((char *));
extern void fieldspecs_walk __P ((hash_wfunc *));
extern STRINGLIST *fieldspec_to_stringlist __P ((char **));
extern STRINGLIST *gen_fieldspec_completions
__P ((FIELDSPEC *, char *, char *, int, int));
extern char **field_specs __P ((char *, char *, int, int, int *));
extern int fieldspec_valueindex __P ((FIELDSPEC *, char *));
extern int fieldspec_displayvalueindex __P ((FIELDSPEC *, char *, int));



extern FORMSPEC *formspec_create __P ((void));
extern void formspec_dispose __P ((FORMSPEC * form));
extern void formspecs_flush __P ((void));
extern void formspecs_dispose __P ((void));
extern int formspec_insert __P ((char *, FORMSPEC *));
extern int formspec_remove __P ((char *));
extern int formspec_print __P ((char *, FORMSPEC *));
extern FORMSPEC *formspec_search __P ((char *));
extern void formspecs_walk __P ((hash_wfunc *));
extern STRINGLIST *formspec_to_stringlist __P ((char **));
extern DISPLAYLEVEL *formspec_finddisplaylevel __P ((FORMSPEC *, char *));

extern DISPLAYLEVEL *displaylevel_init __P ((DISPLAYLEVEL *));
extern void displaylevel_clear __P ((DISPLAYLEVEL *));

extern void screenform_dispose __P ((SCREENFORM *));
extern SCREENFORM *screenform_init __P ((FORMSPEC *, DISPLAYLEVEL *, char *));
extern void screenform_draw __P ((SCREENFORM *));
extern void screenform_layout __P ((SCREENFORM *));
extern void screenform_displayhinttext __P ((SCREENFORM *, CF_EDIT_MODE));
extern void screenform_populatefieldsfrompartialcommand
__P ((SCREENFORM *, WORD_LIST *));
extern void screenform_gotofield
__P ((SCREENFORM *, SCREENFIELD *, CF_EDIT_MODE));
extern int screenform_gotonextfield __P ((SCREENFORM *, CF_EDIT_MODE));
extern void screenform_gotopreviousfield __P ((SCREENFORM *, CF_EDIT_MODE));
extern void screenform_editscreenfield __P ((SCREENFORM *, CF_EDIT_MODE));
extern SCREENFIELD *screenform_locatescreenfield __P ((SCREENFORM *, FIELDSPEC *));

extern int screenfield_generatedargumentlength
__P ((SCREENFIELD * screenfield));
extern char *screenfield_generateargument __P ((SCREENFIELD *));

SCREENFORM *cf_screenform;

#endif /* _COMMANDFORMS_H_ */
