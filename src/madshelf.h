/* ------------------------------------------------------------------------
 Copyright (C) 2008 by Marc Lajoie

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 PARTICULAR PURPOSE.  See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 Place - Suite 330, Boston, MA 02111-1307, USA.

 ------------------------------------------------------------------------ */

#define SORT_BY_NAME 0
#define SORT_BY_TIME 1

void update_list();
void update_title();
void cleanup();
int file_date_compare(const void *data1, const void *data2);
void init_filelist();
void destroy_cb ( Ewl_Widget *w, void *event, void *data );
void doActionForNum(unsigned int num);
char *getUpLevelDir(char *thedir);
