/*
 * Copyright (C) 2008 Alexander Kerner <lunohod@openinkpot.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ewl/Ewl.h>
#include "filefilter.h"
#include "Choicebox.h"
#include <libintl.h>
#include <locale.h>
#include "madshelf.h"

// Options dialogs

static long minl(long a, long b)
{
   if(a < b) return a;
   return b;
}

void filters_dialog_closehandler(Ewl_Widget *widget)
{
    init_filelist();
    update_filelist_in_gui();
    
    
}
void filters_dialog_choicehandler(int choice, Ewl_Widget *parent)
{
    if(isFilterActive(choice))
    {
        update_label(parent, choice,"inactive");
        setFilterActive(choice,0);
    }
    else
    {
        update_label(parent, choice,"active");
        setFilterActive(choice,1);
        
    }
}

void FiltersDialog()
{
    if(getNumFilters<=0)
        return;
	Ewl_Widget *w = ewl_widget_name_find("mainwindow");
    
	char **initchoices,**values;
    initchoices=(char**)malloc(sizeof(char*)*getNumFilters());
    values=(char**)malloc(sizeof(char*)*getNumFilters());
    int i;
    fprintf(stderr,"Debug #1, nfilters=%d",getNumFilters());
    for(i=0;i<getNumFilters();i++)
    {
        if(isFilterActive(i))
        {
            
            asprintf(&values[i],"active");
            
        }
        else
        {
            
            asprintf(&values[i],"inactive");
        }
        asprintf(&initchoices[i],"%d. %s",i+1,getFilterName(i));
    }
    
    
	ewl_widget_show(init_choicebox(initchoices, values, getNumFilters(), filters_dialog_choicehandler, filters_dialog_closehandler,"filters to apply", w, TRUE));
    for(i=0;i<getNumFilters();i++)
    {
        free(initchoices[i]);
        free(values[i]);
        
    }
    free(initchoices);
    free(values);
}

