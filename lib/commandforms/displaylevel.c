/* displaylevel.c - library functions for display levels. */

/*
 * Copyright (C) 1999-2002 Free Software Foundation, Inc.
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

/*
 * display level utilities
 */

DISPLAYLEVEL *
displaylevel_init (displaylevel)
      DISPLAYLEVEL *displaylevel;
{
  memset (displaylevel, 0, sizeof (DISPLAYLEVEL));

  displaylevel->screenfieldlist = (FIELDSPEC **) NULL;
  displaylevel->generationfieldlist = (FIELDSPEC **) NULL;
  displaylevel->displaylevel = (char *) NULL;
  displaylevel->fieldcount = 0;
  return displaylevel;
}

/* Dispose of display level */
void
displaylevel_clear (displaylevel)
     DISPLAYLEVEL *displaylevel;
{

  FREE (displaylevel->screenfieldlist);
  FREE (displaylevel->generationfieldlist);
  FREE (displaylevel->displaylevel);
}

#endif /* COMMAND_FORMS */
