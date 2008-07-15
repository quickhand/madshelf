/*
 * Copyright (C) 2008 Marc Lajoie
 * Copyright (C) 2008 Mikhail Gusarov <dottedmag@dottedmag.net>
 *
 * MadShelf - bookshelf application.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef PLUGINS_H
#define PLUGINS_H
#include "book_meta.h"
#include "plugin.h"
/*
 * Must be called before invoking get_handler() or parse_meta()
 */
void init_plugins();

/*
 * Frees all resources associated with plugins system
 */
void fini_plugins();



/*
 * Parses the passed file and returns metadata.
 * May return NULL if file can't be parsed.
 *
 * Returned value must be freed by passing to free_meta() function.
 *
 * @arg path full path to the file
 */
book_meta_t* get_meta(const char* path);

/*
 * Frees all resources associated with passed book metadata.
 */
void free_meta(book_meta_t* meta);

#endif
