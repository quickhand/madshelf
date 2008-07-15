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
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include "book_meta.h"
#include "plugins.h"
#include "plugin.h"
#define MAX_PLUGINS 100
/*typedef struct
{
    void *handle;
} plugin_t;

typedef struct
{
    const char* file_ext;
    plugin_t* plugin;
} handler_t;*/
typedef struct
{
    void *plugin_pointer;
    plugin_info_t *info;
    plugin_parse_meta_t parse_meta;
        
} plugin_t;

plugin_t plugin_arr[MAX_PLUGINS];
int numplugs=0;

char *get_lowercase_str(char *str)
{
    if(str==NULL)
        return NULL;
    int i;
    char *lowerstr=(char *)calloc(strlen(str)+1,sizeof(char));;
    for(i=0;i<strlen(str);i++)
        lowerstr[i]=tolower(str[i]);
    lowerstr[i]='\0';
    return lowerstr;
}
char *get_lowercase_ext(char *filename)
{
    char *extension;
    extension=strrchr(filename,'.');
    return get_lowercase_str(extension);
}




void init_plugins(char *plugindir)
{
    DIR *plugdir;
    struct dirent *dp;
    char *tmp;
    char *extension;
    char *lowerext;
    int i;
    int plugcount=0;
    void *pluginlib;
    void *initializer;
    void *parserfunc;
    plugin_init_t initfunc;
    plugdir=opendir (plugindir);
    if(plugdir==NULL)
        return;
    while ((dp=readdir(plugdir)) != NULL && numplugs<MAX_PLUGINS) 
    {
        lowerext=get_lowercase_ext(dp->d_name);
        if(strcmp(lowerext,".so")!=0)
        {
            free(lowerext);
            continue;
        }
                //to business;		
        tmp=(char *)calloc(strlen(plugindir)+strlen(dp->d_name)+1,sizeof(char));
        sprintf(tmp,"%s%s",plugindir,dp->d_name);
        pluginlib=dlopen(tmp, RTLD_NOW|RTLD_GLOBAL);
        if(pluginlib==NULL)
        {
            free(lowerext);
            free(tmp);
            continue;
        }
        else
        {
            initializer = dlsym(pluginlib,"init");
                        
            if(initializer == NULL)
            {
                free(lowerext);
                free(tmp);
                dlclose(pluginlib);

                continue;
            }			
            else
            {
                                
                initfunc=*((plugin_init_t*)(&initializer));
                plugin_arr[numplugs].info=initfunc();

                plugin_arr[numplugs].plugin_pointer=pluginlib;

            }
            parserfunc = dlsym(pluginlib,"parse_meta");
                        
            if(parserfunc == NULL)
            {

                plugin_arr[numplugs].parse_meta=NULL;	
            }			
            else
            {
                                
                                
                plugin_arr[numplugs].parse_meta=*((plugin_parse_meta_t*)(&parserfunc));

            }
            numplugs++;	
        }
        
        free(lowerext);
        free(tmp);
    }
    closedir(plugdir);






}

book_meta_t* get_meta(const char* path)
{
    int i;
    int j;
    char *ext;
    char *plugext;
    ext=get_lowercase_ext(path);
    if(ext==NULL)
        return NULL;
    for(i=0;i<numplugs;i++)
    {
        for(j=0;j<plugin_arr[i].info->nexts;j++)
        {
            plugext=get_lowercase_str(plugin_arr[i].info->exts[j]);
            if(strcmp(ext,plugext)==0)
            {
                free(ext);
                free(plugext);
                            
                if(plugin_arr[i].parse_meta==NULL)
                    return NULL;
                else
                    return (plugin_arr[i].parse_meta)(path);


            }

        }



    }
    free(ext);
    free(plugext);
    return NULL;
}
void free_meta(book_meta_t* meta)
{
    if(meta==NULL)
        return;
    free(meta->title);
    free(meta->author);
    free(meta->series);
    free(meta);
}
void free_plugins()
{
    int i;
    void *finifunc;
    plugin_fini_t castedfunc;
    for(i=0;i<numplugs;i++)
    {
        finifunc = dlsym(plugin_arr[i].plugin_pointer,"fini");
        if(finifunc!=NULL)
        {
            castedfunc=*((plugin_fini_t*)(&finifunc));
            castedfunc(plugin_arr[i].info);

        }


                
                
        dlclose(plugin_arr[i].plugin_pointer);

    }
}
