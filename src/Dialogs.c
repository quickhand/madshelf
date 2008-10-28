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
#include "database.h"
#include "IniFile.h"
// Options dialogs
static int filterschanged=0;
static long minl(long a, long b)
{
   if(a < b) return a;
   return b;
}

void filters_dialog_closehandler(Ewl_Widget *widget,void *userdata)
{
    if(filterschanged)
    {
        init_filelist();
        update_filelist_in_gui();
        update_filters();
    }
    
    
}
void filters_dialog_choicehandler(int choice, Ewl_Widget *parent,void *userdata)
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
    filterschanged=1;
}

void FiltersDialog()
{
    if(getNumFilters()<=0)
        return;
	Ewl_Widget *w = ewl_widget_name_find("mainwindow");
    
	char **initchoices,**values;
    initchoices=(char**)malloc(sizeof(char*)*getNumFilters());
    values=(char**)malloc(sizeof(char*)*getNumFilters());
    int i;
    
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
    
    
	ewl_widget_show(init_choicebox(initchoices, values, getNumFilters(), filters_dialog_choicehandler, filters_dialog_closehandler,"File Filters", w,NULL, TRUE));
    for(i=0;i<getNumFilters();i++)
    {
        free(initchoices[i]);
        free(values[i]);
        
    }
    free(initchoices);
    free(values);
    
    filterschanged=0;
}


//Tags dialog

typedef struct _tag_dlg_info {
    char *filename;
    int nchoices;
    char **choices;
    char **values;
    int nfiletags;
    char **filetags;
    
    
    int tagschanged;
    
} tag_dlg_info;

void tags_dialog_closehandler(Ewl_Widget *widget,void *userdata)
{
    tag_dlg_info *curinfo=(tag_dlg_info *)userdata;
    int i,j;
    if(curinfo->tagschanged)
    {
        int reallychanged=0;
        for(i=0;i<curinfo->nchoices;i++)
        {
            int already_assigned=0;
            for(j=0;j<curinfo->nfiletags;j++)
            {
                if(strcmp(curinfo->choices[i],curinfo->filetags[j])==0)
                {
                    already_assigned=1;
                    break;    
                }
            }
            if(strlen(curinfo->values[i])==0)
            {
                if(already_assigned)
                {
                    //remove the tag
                    
                    remove_tag(curinfo->filename,curinfo->choices[i]);
                    reallychanged=1;
                }
            }
            else
            {
                if(!already_assigned)
                {
                    //add the tag
                    const char *newtag[]={curinfo->choices[i]};
                    set_tags(curinfo->filename,newtag,1);
                    reallychanged=1;
                }
                
            }
            
        }
        if(reallychanged)
        {
            if(filter_filelist())
                reset_file_position();
            //update the GUI    
            update_list();
            
            
            
        }
        
        
    
    
    
    
    }
    
    for(i=0;i<curinfo->nchoices;i++)
    {
        free(curinfo->choices[i]);
        free(curinfo->values[i]);
        
    }
    free(curinfo->choices);
    free(curinfo->values);

    for(i=0;i<curinfo->nfiletags;i++)
    {
        free(curinfo->filetags[i]);
        
    }
    if(curinfo->filetags)
        free(curinfo->filetags);
    free(curinfo->filename);
}
void tags_dialog_choicehandler(int choice, Ewl_Widget *parent,void *userdata)
{
    tag_dlg_info *curinfo=(tag_dlg_info *)userdata;
    
    if(strlen(curinfo->values[choice])==0)
    {
        free(curinfo->values[choice]);
        asprintf(&(curinfo->values[choice]),"assigned");
        update_label(parent, choice,"assigned");
        
    }
    else
    {
        free(curinfo->values[choice]);
        asprintf(&(curinfo->values[choice]),"");
        update_label(parent, choice,"");
        
    }
    curinfo->tagschanged=1;
}

void TagsDialog(char *filename)
{
    if(getNumFilters()<=0)
        return;
	Ewl_Widget *w = ewl_widget_name_find("mainwindow");
    
    
    char *tagstring=ReadString("tags","tagnames",NULL);
    if(tagstring==NULL)
        return;
    
    int tagcount=0;
    char *tempo;
    asprintf(&tempo,tagstring);
    char *tok = strtok(tempo, ",");
    
    if(!tok || !tok[0])
        return;
    while (tok != NULL) {
        // Do something with the tok
        if(tok[0])
        {
        
            tagcount++;
        }
        tok = strtok(NULL,",");
    }
    
    free(tempo);
	char **initchoices,**values,**choices;
    initchoices=(char**)malloc(sizeof(char*)*tagcount);
    choices=(char**)malloc(sizeof(char*)*tagcount);
    values=(char**)malloc(sizeof(char*)*tagcount);
    
    
    //get file tags
    char **filetags=NULL;
    int numfiletags=get_tags(filename,&(filetags));
    
    
    int count=0;
    
    asprintf(&tempo,tagstring);
    tok = strtok(tempo, ",");
    if(!tok || !tok[0])
        return;
    while (tok != NULL) {
        // Do something with the tok
        if(tok[0])
        {
            
            asprintf(&(initchoices[count]),"%d. %s",count+1,tok);
            asprintf(&(choices[count]),tok);
            int j;
            for(j=0;j<numfiletags;j++)
            {
                if(strcmp(tok,filetags[j])==0)
                {
                    asprintf(&(values[count]),"assigned");
            
                    break;
                }
                
                
            }
            if(j==numfiletags)
            {
                asprintf(&(values[count]),"");
            
            }
            count++;
        }
        tok = strtok(NULL,",");
        
    }
   
    
    free(tempo);
    
    tag_dlg_info *infostruct=malloc(sizeof(tag_dlg_info));
    asprintf(&(infostruct->filename),filename);
    infostruct->nchoices=tagcount;
    infostruct->choices=choices;
    infostruct->values=values;
    infostruct->tagschanged=0;
    infostruct->nfiletags=numfiletags;
    infostruct->filetags=filetags;
	ewl_widget_show(init_choicebox(initchoices, values, tagcount, tags_dialog_choicehandler, tags_dialog_closehandler,"Assign Tags", w, (void *)infostruct,TRUE));
    int i;
    for(i=0;i<tagcount;i++)
    {
        free(initchoices[i]);
        
        
    }
    free(initchoices);
    
}
