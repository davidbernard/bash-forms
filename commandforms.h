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

typedef enum { 
		CF_NO_NAVIGATION,  /* No navigation specified */
		CF_RETURN,  /* Go to next field  and complete form if at end */
		CF_NEXT_FIELD,  /* Go to next field */
		CF_PREVIOUS_FIELD,  /* Go to previous field */
		CF_REDRAW,  /* Redraw the form  */
		CF_ENTER, 		/* Complete form */
		CF_ESCAPE 		/* Escape out of form */
		} CF_NAVIGATION;

typedef enum {
		CF_MODE_FORM,
		CF_MODE_LINE
		} CF_EDIT_MODE;

typedef enum {
		CF_MODE_DISPLAY,	/* Display the command */
		CF_MODE_EXECUTE		/* Execute the command */
		} CF_EXECUTION_MODE;

/*
	Types of field
 */
typedef enum {
	CF_FIELD_TYPE_INVALID,
	CF_FIELD_TYPE_POSITIONAL ,
	CF_FIELD_TYPE_FLAG ,
	CF_FIELD_TYPE_FLAGWITHVALUE ,
	CF_FIELD_TYPE_LAST ,
	CF_FIELD_TYPE_REST ,
	CF_FIELD_TYPE_UPTOLAST
	} CF_FIELD_TYPE;

/* Field on a screen when a form is displayed */
typedef struct screenfield {
	struct fieldspec *fieldspec;
	char *value;
	char *displayvalue;
	char **displaylevels;
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
	} SCREENFIELD;

typedef struct screenform {
	struct formspec *formspec;
	SCREENFIELD *screenfields;
	int height;
	int width;
	char *label;
	int fieldcount;
	int currentx;
	int currenty;
	int maxlabelwidth;
	SCREENFIELD *currentscreenfield;
	CF_NAVIGATION nextnavigation;
	char *displaylevel;
	} SCREENFORM;

/* Field specification for a type of a field */
typedef struct fieldspec {
	int refcount;
/* Type of field */
	CF_FIELD_TYPE fieldtype;
/* Hint and help text */
	char *hinttext;
	char *helptext;
/* Label to appear on screen */
	char *label;
/* If value present then precede with this flag */
	char *flag;
/* Values to use for generation of command  - first value is default
	if more than one value then select from list of values */
	int valuescount;
	char **values;
/* Values to use for dislaying list of options if different from above */
	char **displayvalues;
/* Optional function for command completion */
	char *compspec;
/* Optional separator string */
	char *separator;
/* Determines the display levels the field is displayed in */
	char **displaylevels;
	} FIELDSPEC;

/* Links the name used to add a field  to a formspec */
typedef struct formfieldspec {
	char *fieldspecname;
	FIELDSPEC *fieldspec;
	/* Currently displayed screen field */
	SCREENFIELD *screenfield;  
	/* Link between generation and display field list fields */
	struct formfieldspec *crosslink;
	} FORMFIELDSPEC;

/* Form Specification */
typedef struct formspec {
	/* List of fields as they appear on screen */
	FORMFIELDSPEC **screenfieldlist; 		
	/* List of fields in the order of command generation */
	FORMFIELDSPEC **generationfieldlist;	
	/* Command to execute */
	char *command;
	/* Count of fields */
	int fieldcount;
	/* Display levels supported by this form - the first level is the default*/
	char **displaylevels;
	} FORMSPEC;

/* Table for definition of pre-defined field types */
typedef struct fieldtypedef {
	char *type_name;
	} FIELDTYPEDEF;

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
char * fielddef_flag_generateargument(FIELDSPEC *fieldspec, SCREENFIELD *field);
char * fielddef_flag_complete (FIELDSPEC *fieldspec, SCREENFIELD *field);
char * fielddef_flag_defaultvalue (FIELDSPEC *fieldspec, SCREENFIELD *field);

extern FIELDTYPEDEF fieldtypes[];
extern HASH_TABLE *fieldspecs;
extern HASH_TABLE *formspecs;
extern int commandforms_enabled;

/* Functions from commandformslib.c */
extern FIELDSPEC *fieldspec_create __P((void));

extern void fieldspecs_flush __P((void));
extern int fieldspec_insert __P((char *, FIELDSPEC *));
extern void fieldspec_retain __P(( FIELDSPEC *));
extern int fieldspec_remove __P((char *));
extern FIELDSPEC *fieldspec_search __P((const char *));
extern void fieldspecs_walk __P((hash_wfunc *));
extern STRINGLIST *fieldspec_to_stringlist __P((char **));
extern STRINGLIST *gen_fieldspec_completions __P((FIELDSPEC *, const char *, const char *, int, int));
extern char **field_specs __P((const char *, const char *, int, int, int *));

extern FORMSPEC *formspec_create __P((void));
extern void formspec_dispose __P((FORMSPEC *form));
extern void formspecs_flush __P((void));
extern void formspecs_dispose __P((void));
extern int formspec_insert __P((char *, FORMSPEC *));
extern int formspec_remove __P((char *));
extern FORMSPEC *formspec_search __P((const char *));
extern void formspecs_walk __P((hash_wfunc *));
extern STRINGLIST *formspec_to_stringlist __P((char **));

SCREENFORM *cf_screenform;

#endif /* _COMMANDFORMS_H_ */
