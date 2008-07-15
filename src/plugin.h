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

#ifndef PLUGIN_H
#define PLUGIN_H
#include "book_meta.h"
/*
 * Every plugin must export three functions:
 *
 * plugin_init_t init
 * plugin_fini_t fini
 * plugin_parse_meta_t parse_meta
 */

typedef struct
{
    /* Name of plugin */
    char* name;

    /* File extensions handled by plugin */
    int nexts;
    char** exts;
} plugin_info_t;

typedef plugin_info_t* (*plugin_init_t)();

typedef void (*plugin_fini_t)(plugin_info_t* plugin_info);

typedef book_meta_t* (*plugin_parse_meta_t)(const char* filename);

#endif
