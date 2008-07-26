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

#define _GNU_SOURCE

#include <ctype.h>
#include <Ecore_File.h>
#include <Edje.h>
#include <Eet.h>
#include <errno.h>
#include <Ewl.h>
#include <ewl_list.h>
#include <ewl_macros.h>
#include <extractor.h>
#include <fcntl.h>
#include "IniFile.h"
#include <libintl.h>
#include <locale.h>
#include "madshelf.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SCRIPTS_DIR "/.madshelf/scripts/"
#define DEFAULT_THEME "/usr/share/madshelf/madshelf.edj"

/*
* Single "root"
*/
typedef struct
{
    char* name;
    char* path;
} root_t;

/*
* All "roots" defined in config file
*/
typedef struct
{
    int nroots;
    root_t* roots;
} roots_t;

roots_t* g_roots;
char **scriptstrlist;
char *statefilename=NULL;
Ecore_List *filelist;

/*
* Not need to be freed.
*/
const char* g_handler;
/*
* Need to be free(3)ed.
*/
const char* g_file;
/*
* Need to be freed by own methods.
*/

EXTRACTOR_ExtractorList *extractors;

char titletext[200];

//***********these variables need to be saved and restored in order to restore the state
int curindex=0;
int depth=0;

int current_root;

char *curdir;
int initdirstrlen;
int sort_type=SORT_BY_NAME;
int sort_order=ECORE_SORT_MIN;
//***********


/*
* roots_create() helper
*/
int count_roots()
{
    int count = 0;
    struct ENTRY* p = FindSection("roots");
    if (p)
        p = p->pNext;

    while (p && p->Type != tpSECTION)
    {
        if(p->Type == tpKEYVALUE)
            count++;
        p = p->pNext;
    }
    return count;
}

/*
* roots_create() helper
*/
void fill_roots(int count, root_t* roots)
{
    int i;
    struct ENTRY* p = FindSection("roots");
    if (p)
        p = p->pNext;

    for (i = 0; i < count; ++i)
    {
        char* name;
        const char* conf_line;
        const char* line_sep;
    
        while (p->Type != tpKEYVALUE)
            p = p->pNext;
    
        conf_line = p->Text;
        line_sep = strchr(conf_line, '=');

        if (!line_sep)
        {
            /*
             * Sin
             */
            fprintf(stderr, "Malformed configuration line: %s\n", conf_line);
            continue;
        }
    
        name = malloc((line_sep - conf_line + 1) * sizeof(char));
        strncpy(name, conf_line, line_sep - conf_line);
        name[line_sep - conf_line] = 0; /* 0-terminate after strncpy */
        roots[i].name = name;
    
        roots[i].path = strdup(line_sep+1);
    
        p = p->pNext;
    }
}

/*
* Parses and returns list of "roots" from the config file.
*
* Returned pointer need to be passed to roots_destroy function.
*/
roots_t* roots_create()
{
    roots_t* roots = malloc(sizeof(roots_t));
    roots->nroots = count_roots();
    roots->roots = malloc(sizeof(root_t) * roots->nroots);
    fill_roots(roots->nroots, roots->roots);
    return roots;
}

/*
* Destroys the passed roots info.
*/
void roots_destroy(roots_t* roots)
{
    int i;
    for (i = 0; i < roots->nroots; ++i)
    {
        free(roots->roots[i].name);
        free(roots->roots[i].path);
    }
    free(roots->roots);
    free(roots);
}

void update_list()
{
    int offset=0;
    int count=0;
    char *file;
    char tempname[20];
    char *imagefile;
    char *pointptr;
    char *fileconcat;
    char *extension;
    const char *tempstr2;
    char timeStr[101];
    char *infostr;
    struct tm* atime;
    struct stat stat_p;
    int filelistcount;
    char sizestr[50];
    Ewl_Widget *labelsbox[8];
    Ewl_Widget *titlelabel[8];
    Ewl_Widget *authorlabel[8];
    Ewl_Widget *infolabel[8];
    Ewl_Widget *bookbox[8];
    Ewl_Widget *separator[8];
    Ewl_Widget *typeicon[8];
    Ewl_Widget *backarr;
    Ewl_Widget *forwardarr;
    int backarrshowflag;
    int forwardarrshowflag;
    int showflag[8];
    const char *extracted_title;
    const char *extracted_author;
    EXTRACTOR_KeywordList *mykeys;
    count=0;
    filelistcount=ecore_list_count(filelist);
    if(filelistcount>0 && curindex>=filelistcount)
    {
        curindex-=8;
        return;
    }
    
    
    
    for(count=0;count<8;count++)
    {
        sprintf (tempname, "titlelabel%d",count);
        titlelabel[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "authorlabel%d",count);
        authorlabel[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "infolabel%d",count);
        infolabel[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "bookbox%d",count);
        bookbox[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "separator%d",count);
        separator[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "type%d",count);
        typeicon[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "labelsbox%d",count);
        labelsbox[count] = ewl_widget_name_find(tempname);
        

    }
    sprintf (tempname, "backarr");
    backarr = ewl_widget_name_find(tempname);
    sprintf (tempname, "forwardarr");
    forwardarr = ewl_widget_name_find(tempname);
    //set arrow offset
    offset=ewl_object_current_w_get(EWL_OBJECT(forwardarr));
    
    for(count=0;count<8;count++)
    {
        ewl_widget_hide(bookbox[count]);
        ewl_widget_hide(labelsbox[count]);
        ewl_widget_hide(separator[count]);
        ewl_widget_hide(authorlabel[count]);
        ewl_widget_hide(titlelabel[count]);
        ewl_widget_hide(infolabel[count]);
        ewl_widget_hide(typeicon[count]);
        
    }
    ewl_widget_hide(forwardarr);
    ewl_widget_hide(backarr);
        
    
    if(filelistcount>0)
    {
        if(curindex>=filelistcount)
        {
            curindex-=8;
            return;
        }
                
        ecore_list_index_goto(filelist,curindex);
                
                
                
        for(count=0;count<8&&(file = (char*)ecore_list_next(filelist));count++)
        {
            fileconcat=(char *)calloc(strlen(file)+strlen(curdir)+1,sizeof(char));
            sprintf(fileconcat,"%s%s",curdir,file);
            
            if(!ecore_file_is_dir(fileconcat))
            {
                mykeys=EXTRACTOR_getKeywords(extractors,fileconcat);
                    
                extracted_title=EXTRACTOR_extractLast(EXTRACTOR_TITLE,mykeys);
                extracted_author=EXTRACTOR_extractLast(EXTRACTOR_AUTHOR,mykeys);
            }
        
                        
            if(extracted_title!=NULL && strlen(extracted_title)>0 && !ecore_file_is_dir(fileconcat))
            {

                ewl_label_text_set(EWL_LABEL(titlelabel[count]),extracted_title);
            }
            else
            {
                ewl_label_text_set(EWL_LABEL(titlelabel[count]),ecore_file_strip_ext(file));
            }
        
        
                        
        
            if(!ecore_file_is_dir(fileconcat))
            {
                stat(fileconcat,&stat_p);
                                
                atime = localtime(&(stat_p.st_mtime));
                strftime(timeStr, 100, gettext("%m-%d-%y"), atime);
                if(stat_p.st_size>=1048576)
                {
                    sprintf(sizestr,"%.1fM",((double)(stat_p.st_size))/((double)1048576.0));
                }
                else
                {
                    sprintf(sizestr,"%dk",(int)(((double)(stat_p.st_size))/((double)1024.0)+0.5));
                }
                                
                extension = strrchr(file, '.');
                if(extension==NULL)
                {
                    infostr=(char *)calloc(strlen(timeStr)+3+strlen(sizestr)+1,sizeof(char));
                }
                else
                {
                    infostr=(char *)calloc(strlen(timeStr)+3+strlen(extension)+3+strlen(sizestr)+1,sizeof(char));
                    strcat(infostr,extension);
                    strcat(infostr,"   ");
                }
                strcat(infostr,timeStr);
                strcat(infostr,"   ");
                strcat(infostr,sizestr);
                ewl_label_text_set(EWL_LABEL(infolabel[count]),infostr);
                free(infostr);
        
        
                if(extracted_author!=NULL && strlen(extracted_author)>0)
                    ewl_label_text_set(EWL_LABEL(authorlabel[count]),extracted_author);
                else
                    ewl_label_text_set(EWL_LABEL(authorlabel[count]),gettext("Unknown Author"));
                                
            }
            else
            {
                stat(fileconcat,&stat_p);
                                
                atime = localtime(&(stat_p.st_mtime));
                strftime(timeStr, 100, gettext("%m-%d-%y"), atime);
                                
                extension = strrchr(file, '.');
                infostr=(char *)calloc(strlen(timeStr)+1,sizeof(char));
                strcat(infostr,timeStr);
                                
                ewl_label_text_set(EWL_LABEL(infolabel[count]),infostr);
                free(infostr);
        
                ewl_label_text_set(EWL_LABEL(authorlabel[count]),"Folder");
            }
                        
        
                        
            if(!ecore_file_is_dir(fileconcat))
            {
                pointptr=strrchr(fileconcat,'.');
                if(pointptr==NULL)
                    tempstr2=ReadString("icons",".","default.png");
                else 
                    tempstr2=ReadString("icons",pointptr,"default.png");
                imagefile=(char *)calloc(strlen(tempstr2)+20+1, sizeof(char));
                strcat(imagefile,"/usr/share/madshelf/");
                strcat(imagefile,tempstr2);
                ewl_image_file_path_set(EWL_IMAGE(typeicon[count]),imagefile);
                free(imagefile);
                        
            }
            else
            {
                                
                ewl_image_file_path_set(EWL_IMAGE(typeicon[count]),"/usr/share/madshelf/folder.png");
            }
                
        
            showflag[count]=1;
            free(fileconcat);
        }
        
        for(;count<8;count++)
        {
            
            
            showflag[count]=0;
        }
        if((curindex+8)>=ecore_list_count(filelist))
        {
            forwardarrshowflag=0;
            
        }
        else
        {
            forwardarrshowflag=1;
            offset=0;
        }
        if(curindex>0)
        {
            backarrshowflag=1;
            ewl_object_padding_set(EWL_OBJECT(backarr),0,offset,0,0);
        }
        else
        {
            backarrshowflag=0;
        }
    
        for(count=0;count<8;count++)
        {
            if(showflag[count])
            {
                ewl_widget_show(typeicon[count]);
                ewl_widget_show(authorlabel[count]);
                ewl_widget_show(titlelabel[count]);
                ewl_widget_show(infolabel[count]);
                ewl_widget_show(separator[count]);
                ewl_widget_show(labelsbox[count]);
                ewl_widget_show(bookbox[count]);
            }
    
        }
        if(backarrshowflag)
        {
            ewl_widget_show(backarr);
            
        }
        if(forwardarrshowflag)
        {
            ewl_widget_show(forwardarr);
            
        }
    }
}

void update_title()
{
    titletext[0]='\0';
    Ewl_Widget *curwidget;
    strcat(titletext,"Madshelf | ");
    strcat(titletext, g_roots->roots[current_root].name);
    if(!(strlen(curdir)==initdirstrlen))
    {
        strcat(titletext,"://");
        strcat(titletext,&(curdir[initdirstrlen]));
    }

    curwidget = ewl_widget_name_find("mainborder");
    ewl_border_label_set(EWL_BORDER(curwidget),titletext);
}
void update_sort_label()
{
    Ewl_Widget *curwidget;

    curwidget = ewl_widget_name_find("sortlabel");
    if(sort_type==SORT_BY_NAME)
    {
        if(sort_order==ECORE_SORT_MAX)
            ewl_label_text_set(EWL_LABEL(curwidget),gettext("reverse-sorted by name"));
        else
            ewl_label_text_set(EWL_LABEL(curwidget),gettext("sorted by name"));
    }
    else if(sort_type==SORT_BY_TIME) 
    {
        if(sort_order==ECORE_SORT_MAX)
            ewl_label_text_set(EWL_LABEL(curwidget),gettext("reverse-sorted by time"));
        else
            ewl_label_text_set(EWL_LABEL(curwidget),gettext("sorted by time"));
    }
}

void update_menu()
{
    Ewl_Widget *curwidget;
    curwidget = ewl_widget_name_find("okmenu");
    ewl_button_label_set(EWL_BUTTON(curwidget),gettext("Menu (OK)"));
    curwidget = ewl_widget_name_find("menuitem1");
    ewl_button_label_set(EWL_BUTTON(curwidget),gettext("1. Sort by Name"));
    curwidget = ewl_widget_name_find("menuitem2");	
    ewl_button_label_set(EWL_BUTTON(curwidget),gettext("2. Sort by Time"));
    curwidget = ewl_widget_name_find("menuitem3");	
    ewl_button_label_set(EWL_BUTTON(curwidget),gettext("3. Reverse Sort Order"));
    curwidget = ewl_widget_name_find("menuitem4");
    ewl_button_label_set(EWL_BUTTON(curwidget),gettext("4. Language Settings"));
    curwidget = ewl_widget_name_find("menuitem5");
    ewl_button_label_set(EWL_BUTTON(curwidget),gettext("5. Go to"));
    curwidget = ewl_widget_name_find("menuitem6");
    ewl_button_label_set(EWL_BUTTON(curwidget),gettext("6. Scripts"));
}

int file_name_compare(const void *data1, const void *data2)
{
    int counter;
    char *fname1,*fname2;
    fname1=(char*)data1;
    fname2=(char*)data2;
    for(counter=0;counter<strlen(fname1)&&counter<strlen(fname2);counter++)
    {
        if(fname1[0]>fname2[0])
            return 1;
        else if(fname1[0]<fname2[0])
            return -1;
    }
    return 0;
}
int file_date_compare(const void *data1, const void *data2)
{
    char *fname1,*fname2;
    fname1=(char *)calloc(strlen((char*)data1) + strlen(curdir)+1, sizeof(char));
    strcat(fname1,curdir);
    strcat(fname1,(char*)data1);
    fname2=(char *)calloc(strlen((char*)data2) + strlen(curdir)+1, sizeof(char));
    strcat(fname2,curdir);
    strcat(fname2,(char*)data2);
    long long ftime1=ecore_file_mod_time(fname1);
    long long ftime2=ecore_file_mod_time(fname2);
    free(fname1);
    free(fname2);
    if(ftime1>ftime2)
        return 1;
    else if(ftime1<ftime2)
        return -1;
    else
        return 0;
}

void init_filelist()
{
    int i;
    int listcount;
    char *file;
    filelist = ecore_file_ls(curdir);
    listcount=ecore_list_count(filelist);
    ecore_list_index_goto(filelist,0);

    for(i=0;i<listcount;i++)
    {
        file = (char*)ecore_list_current(filelist);
        if(file[0]=='.')
        {
            ecore_list_remove_destroy(filelist);
        }
        else
            ecore_list_next(filelist);
    }
    if(sort_type==SORT_BY_NAME)
        ecore_list_sort(filelist,file_name_compare,sort_order);
    else if(sort_type==SORT_BY_TIME)
        ecore_list_sort(filelist,file_date_compare,sort_order);
}

void destroy_cb ( Ewl_Widget *w, void *event, void *data )
{
    ewl_widget_destroy ( w );
    ewl_main_quit();
}

/*
* Looks up handler for given absolute file path. Returns string with filename
* of the program-handler.
*
* Returned string will be invalidated by the next call to IniFile.c::Read* or
* lookup_handler.
*/
const char* lookup_handler(const char* file_path)
{
    const char* file_name = basename(file_path);

    const char* extension = strrchr(file_name, '.');
    if (!extension)
        return NULL;

    return ReadString("apps", extension, NULL);
}

void doActionForNum(unsigned int num)
{
    char *file;
    char *tempo;
    if(curindex+(num-1)>=ecore_list_count(filelist))
        return;

    ecore_list_index_goto(filelist,curindex+num-1);
    file = (char*)ecore_list_next(filelist);
    tempo=(char *)calloc(strlen(file) + strlen(curdir)+2, sizeof(char));
    strcat(tempo,curdir);
    strcat(tempo,file);

    if(!ecore_file_is_dir(tempo))
    {
        const char* handler = lookup_handler(tempo);
        if (handler)
        {
            /* Sin */
            g_handler = handler;
            g_file = tempo;
            ewl_main_quit();
        }
        else
        {
            fprintf(stderr, "Unable to find handler for %s\n", file);
        }
    }
    else
    {
        free(curdir);
        strcat(tempo,"/");
        curdir=tempo;
        ecore_list_destroy(filelist);
        init_filelist();
        depth++;
        curindex=0;
        update_list();
        update_title();
    }
}

char *getUpLevelDir(char *thedir)
{
    char *strippeddir;
    char *ptr;
    int loc=-1;
    int count;
        
    strippeddir=(char *)calloc(strlen(thedir), sizeof(char));
    strncpy(strippeddir,thedir,strlen(thedir)-1);
    strippeddir[strlen(thedir)-1]='\0';
    ptr=strrchr(strippeddir,'/');
    if(ptr==NULL)
    {
        free(strippeddir);
        return NULL;
    }
    for(count=0;count<strlen(strippeddir);count++)
        if(&(strippeddir[count])==ptr)
            loc=count+1;
    if(loc==-1)
    {
        free(strippeddir);
        return NULL;
    }
    ptr=(char *)calloc(loc +1, sizeof(char));
    strncpy(ptr,strippeddir,loc);
    ptr[loc]='\0';
    free(strippeddir);
    return ptr;
}

#define K_UNKNOWN -1
#define K_ESCAPE 10
#define K_RETURN 11

int translate_key(Ewl_Event_Key_Down* e)
{
    const char* k = e->base.keyname;

    if (!strcmp(k, "Escape"))
        return K_ESCAPE;
    if (!strcmp(k, "Return"))
        return K_RETURN;
    if (isdigit(k[0]) && !k[1])
        return k[0] - '0';
    return K_UNKNOWN;
}

void cb_key_down(Ewl_Widget *w, void *ev, void *data)
{
    Ewl_Event_Key_Down *e;
    Ewl_Widget *curwidget;
    char *tmpchrptr;
    e = (Ewl_Event_Key_Down*)ev;

    int k = translate_key(e);

    if(k == 0)
    {
        curindex+=8;
        update_list();	
    }
    else if (k == 9)
    {
        if(curindex>0)
        {
            curindex-=8;
            update_list();	
        }
    }
    else if (k >= 0 && k <= 8)
    {
        doActionForNum(k);
    }
    else if (k == K_RETURN)
    {
        curwidget = ewl_widget_name_find("okmenu");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
    }
    else if(k == K_ESCAPE)
    {
        if(depth==0)
            return;
        tmpchrptr=getUpLevelDir(curdir);
        if(tmpchrptr==NULL)
            return;

        free(curdir);
        curdir=tmpchrptr;
        ecore_list_destroy(filelist);
        init_filelist();
        depth--;
        curindex=0;
        update_list();
        update_title();
    }
}

void cb_menu_key_down(Ewl_Widget *w, void *ev, void *data)
{
    Ewl_Event_Key_Down *e;
    Ewl_Widget *curwidget;

    e = (Ewl_Event_Key_Down*)ev;
    int k = translate_key(e);
        
    if (k == K_ESCAPE)
    {
        curwidget = ewl_widget_name_find("okmenu");
        ewl_menu_collapse(EWL_MENU(curwidget));
    }
    else if(k == 1)
    {
        ecore_list_sort(filelist,file_name_compare,ECORE_SORT_MIN);
        sort_order=ECORE_SORT_MIN;
        sort_type=SORT_BY_NAME;
        curindex=0;
        update_list();
        update_sort_label();
        curwidget = ewl_widget_name_find("okmenu");
        ewl_menu_collapse(EWL_MENU(curwidget));
    }
    else if(k == 2)
    {
        ecore_list_sort(filelist,file_date_compare,ECORE_SORT_MIN);
        sort_order=ECORE_SORT_MIN;
        sort_type=SORT_BY_TIME;
        curindex=0;
        update_list();
        update_sort_label();
        curwidget = ewl_widget_name_find("okmenu");
        ewl_menu_collapse(EWL_MENU(curwidget));
    }
    else if(k == 3)
    {
        if(sort_order==ECORE_SORT_MIN)
            sort_order=ECORE_SORT_MAX;
        else
            sort_order=ECORE_SORT_MIN;
        if(sort_type==SORT_BY_NAME)
            ecore_list_sort(filelist,file_name_compare,sort_order);
        else if(sort_type==SORT_BY_TIME)
            ecore_list_sort(filelist,file_date_compare,sort_order);
        curindex=0;
        update_list();
        update_sort_label();
        curwidget = ewl_widget_name_find("okmenu");
        ewl_menu_collapse(EWL_MENU(curwidget));
    }
    else if(k == 4)
    {
        curwidget = ewl_widget_name_find("menuitem4");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
    }
    else if(k == 5)
    {
        curwidget = ewl_widget_name_find("menuitem5");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
    }
    else if(k == 6)
    {
        curwidget = ewl_widget_name_find("menuitem6");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
    }	
}

typedef struct
{
    const char name[64];
    const char locale[6];
} language_t;

static language_t g_languages[] =
{
    { "1. English", "en" },
    { "2. Français", "fr" },
    { "3. Русский", "ru" },
    { "4. 简体中文", "zh_CN" }
};

static const int g_nlanguages = sizeof(g_languages)/sizeof(language_t);

void cb_lang_menu_key_down(Ewl_Widget *w, void *ev, void *data)
{
    Ewl_Event_Key_Down* e = (Ewl_Event_Key_Down*)ev;
    Ewl_Widget *curwidget;

    int k = translate_key(e);

    if(k == K_ESCAPE)
    {
        curwidget = ewl_widget_name_find("menuitem4");
        ewl_menu_collapse(EWL_MENU(curwidget));
        curwidget = ewl_widget_name_find("okmenu");
        ewl_menu_collapse(EWL_MENU(curwidget));
        return;
    }

    if (k > 0 && k <= g_nlanguages)
    {
        setenv("LANGUAGE", g_languages[k-1].locale, 1);

        /*
        * gettext needs to be notified about language change
        */
        {
            extern int  _nl_msg_cat_cntr;
            ++_nl_msg_cat_cntr;
        }

        curwidget = ewl_widget_name_find("menuitem4");
        ewl_menu_collapse(EWL_MENU(curwidget));
        curwidget = ewl_widget_name_find("okmenu");
        ewl_menu_collapse(EWL_MENU(curwidget));
        update_title();
        update_sort_label();
        update_menu();
    }
}

void cb_goto_menu_key_down(Ewl_Widget *w, void *ev, void *data)
{
    Ewl_Event_Key_Down *e;
    Ewl_Widget *curwidget;
    int index=-1;
    e = (Ewl_Event_Key_Down*)ev;

    if(!strcmp(e->base.keyname,"Escape"))
    {
        curwidget = ewl_widget_name_find("menuitem5");
        ewl_menu_collapse(EWL_MENU(curwidget));
        curwidget = ewl_widget_name_find("okmenu");
        ewl_menu_collapse(EWL_MENU(curwidget));
        return;
    }

    if (isdigit(e->base.keyname[0]) && !e->base.keyname[1])
        index = e->base.keyname[0] - '0';
    else
        return;

    curwidget = ewl_widget_name_find("menuitem5");
    ewl_menu_collapse(EWL_MENU(curwidget));
    curwidget = ewl_widget_name_find("okmenu");
    ewl_menu_collapse(EWL_MENU(curwidget));

    /* roots are counted from zero */
    index--;

    if (g_roots->nroots < index)
        return;

    curdir = strdup(g_roots->roots[index].path);
    current_root = index;

    initdirstrlen=strlen(curdir);
    depth=0;
    curindex=0;
    init_filelist();
    update_title();
    update_list();
}

void cb_script_menu_key_down(Ewl_Widget *w, void *ev, void *data)
{
    Ewl_Event_Key_Down *e;
    Ewl_Widget *curwidget;
    int index=-1;
    int count=0;
    const char *tempstr;
    char* handler_path;
    e = (Ewl_Event_Key_Down*)ev;
    if(!strcmp(e->base.keyname,"Escape"))
    {
        curwidget = ewl_widget_name_find("menuitem6");
        ewl_menu_collapse(EWL_MENU(curwidget));
        curwidget = ewl_widget_name_find("okmenu");
        ewl_menu_collapse(EWL_MENU(curwidget));
        return;
    }

    if (isdigit(e->base.keyname[0]) && !e->base.keyname[1])
        index = e->base.keyname[0] - '0';
    else
        return;

    curwidget = ewl_widget_name_find("menuitem5");
    ewl_menu_collapse(EWL_MENU(curwidget));
    curwidget = ewl_widget_name_find("okmenu");
    ewl_menu_collapse(EWL_MENU(curwidget));

    //if (g_roots->nroots < index)
    //    return;
    index--;
    if(index<0)
        return;
    for(count=0;count<=index;count++)
        if(scriptstrlist[count]==NULL)
            return;

    tempstr=ReadString("scripts",scriptstrlist[index],NULL);
    handler_path = malloc(strlen(getenv("HOME"))+sizeof(SCRIPTS_DIR)/sizeof(char)+strlen(tempstr));
    sprintf(handler_path, "%s%s%s", getenv("HOME"), SCRIPTS_DIR,tempstr);

    system(handler_path);
}

void save_state()
{
    Eet_File *state;
    state=eet_open(statefilename,EET_FILE_MODE_WRITE);
    const int a=1;
    eet_write(state,"statesaved",(void *)&a, sizeof(int),0);
    eet_write(state,"curindex",(void *)&curindex,sizeof(int),0);
    eet_write(state,"depth",(void *)&depth,sizeof(int),0);
    eet_write(state,"rootname",(void *)g_roots->roots[current_root].name,
              sizeof(char)*(strlen(g_roots->roots[current_root].name)+1),0);
    eet_write(state,"curdir",curdir,sizeof(char)*(strlen(curdir)+1),0);
    eet_write(state,"initdirstrlen",(void *)&initdirstrlen,sizeof(int),0);
    eet_write(state,"sort_type",(void *)&sort_type,sizeof(int),0);
    eet_write(state,"sort_order",(void *)&sort_order,sizeof(int),0);
    eet_close(state);
}

void refresh_state()
{
    char *temp;
    Eet_File *state;
    int size;
    state=eet_open(statefilename,EET_FILE_MODE_READ);
    int i;
    if(eet_read(state,"statesaved",&size)==NULL)
    {
        eet_close(state);
        return;
    }
    curindex=*((int*)eet_read(state,"curindex",&size));
    depth=*((int*)eet_read(state,"depth",&size));

    current_root = 0;
    temp=(char *)eet_read(state,"rootname",&size);
    for(i = 0; i < g_roots->nroots; ++i)
        if (!strcmp(temp, g_roots->roots[i].name))
    {
        current_root = i;
        break;
    }
        
    temp=(char *)eet_read(state,"curdir",&size);
    free(curdir);
    curdir=(char *)calloc(strlen(temp) + 1, sizeof(char));
    strcpy(curdir,temp);
    initdirstrlen=*((int*)eet_read(state,"initdirstrlen",&size));
    sort_type=*((int*)eet_read(state,"sort_type",&size));
    sort_order=*((int*)eet_read(state,"sort_order",&size));
    eet_close(state);
}

int valid_dir(const char* dir)
{
    struct stat st;

    int res = stat(dir, &st);
    return res == 0 && S_ISDIR(st.st_mode) && ((S_IRUSR | S_IXUSR) & st.st_mode);
}

int main ( int argc, char ** argv )
{	
    int file_desc;
        //char *eetfilename;
    char tempname1[20];
    char tempname2[20];
    char tempname3[20];
    char tempname4[20];
    char tempname5[20];
    Ewl_Widget *win = NULL;
    Ewl_Widget *border = NULL;

    Ewl_Widget *box = NULL;
    Ewl_Widget *box2=NULL;
    Ewl_Widget *box3=NULL;
    Ewl_Widget *box4=NULL;
    Ewl_Widget *box5=NULL;
    Ewl_Widget *authorlabel[8];
    Ewl_Widget *titlelabel[8];
    Ewl_Widget *infolabel[8];
    Ewl_Widget *iconimage[8];
    Ewl_Widget *menubar=NULL;
    Ewl_Widget *forwardarr=NULL;
    Ewl_Widget *backarr=NULL;
    Ewl_Widget *sorttypetext;
    Ewl_Widget *dividewidget;
    char *homedir;
    char *configfile;
    int count=0;
    int count2=0;
    char *tempstr4;
    char *tempstr6;
    struct ENTRY *scriptlist;

    if ( !ewl_init ( &argc, argv ) )
    {
        return 1;
    }
    eet_init();
    ewl_theme_theme_set(DEFAULT_THEME);

    setlocale(LC_ALL, "");
    textdomain("madshelf");
        
    homedir=getenv("HOME");
    configfile=(char *)calloc(strlen(homedir) + 1+18 + 1, sizeof(char));
    strcat(configfile,homedir);
    strcat(configfile,"/.madshelf/");
    if(!ecore_file_path_dir_exists(configfile))
    {
        ecore_file_mkpath(configfile);
    }
    strcat(configfile,"config");
    if(!ecore_file_exists(configfile))
    {
        file_desc=open(configfile, O_CREAT |O_RDWR | O_CREAT,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        write(file_desc,"[roots]\nHome=", 13*sizeof(char));
        write(file_desc,getenv("HOME"),strlen(getenv("HOME"))*sizeof(char));
        if(homedir&&homedir[strlen(homedir)-1]!='/')
            write(file_desc,"/",sizeof(char));
        write(file_desc,"\n[apps]\n[icons]\n[scripts]",15*sizeof(char));
        close(file_desc);
    }
    OpenIniFile (configfile);
    free(configfile);
    
    //load extractors
    extractors=EXTRACTOR_loadConfigLibraries(NULL,"libextractor_pdf:libextractor_html:libextractor_oo:libextractor_ps:libextractor_dvi");//EXTRACTOR_loadDefaultLibraries();
    if(extractors==NULL)
        fprintf(stderr,"Could not load extractors");
    
        //load scripts
    count2=0;
    scriptlist=FindSection("scripts");
    if (scriptlist && scriptlist->pNext)
        scriptlist = scriptlist->pNext;
    while(scriptlist!=NULL &&scriptlist->Type!=tpSECTION &&count2<8)
    {	
        if(scriptlist->Type!=tpKEYVALUE)
            continue;
        count2++;
        scriptlist=scriptlist->pNext;
    }
    scriptstrlist=(char **)calloc(count2 +  1, sizeof(char *));	
    count2=0;
    scriptlist=FindSection("scripts");
    if (scriptlist && scriptlist->pNext)
        scriptlist = scriptlist->pNext;
    while(scriptlist!=NULL &&scriptlist->Type!=tpSECTION && count2<8)
    {	
        if(scriptlist->Type!=tpKEYVALUE)
            continue;
        tempstr6=(char *)calloc(strlen(scriptlist->Text)+1,sizeof(char));
        strcat(tempstr6,scriptlist->Text);
        scriptstrlist[count2]=strtok(tempstr6,"=");		
        count2++;
        scriptlist=scriptlist->pNext;
    }
    scriptstrlist[count2]=NULL;

    // load roots
    g_roots = roots_create();
    current_root = 0;

    curdir = strdup(g_roots->roots[current_root].path);
        
    initdirstrlen=strlen(curdir);
        
    statefilename=(char *)calloc(strlen(homedir) + 1+21 + 1, sizeof(char));
    strcat(statefilename,homedir);
    strcat(statefilename,"/.madshelf/state.eet");
        
    refresh_state();

    if (!valid_dir(curdir))
    {
        free(curdir);

    /*
        * This dir may be invalid too. Oh, well...
    */
        curdir = strdup(g_roots->roots[0].path);
    }

    win = ewl_window_new();
    ewl_window_title_set ( EWL_WINDOW ( win ), "EWL_WINDOW" );
    ewl_window_name_set ( EWL_WINDOW ( win ), "EWL_WINDOW" );
    ewl_window_class_set ( EWL_WINDOW ( win ), "EWLWindow" );
    ewl_object_size_request ( EWL_OBJECT ( win ), 600, 800 );
    ewl_callback_append ( win, EWL_CALLBACK_DELETE_WINDOW, destroy_cb, NULL );
    ewl_callback_append(win, EWL_CALLBACK_KEY_DOWN, cb_key_down, NULL);
    ewl_widget_name_set(win,"mainwindow");
    ewl_widget_show ( win );
        
    init_filelist();

    box2 = ewl_vbox_new();
    ewl_container_child_append(EWL_CONTAINER(win),box2);
    ewl_object_fill_policy_set(EWL_OBJECT(box2), EWL_FLAG_FILL_ALL);
        //ewl_theme_data_str_set(EWL_WIDGET(box2),"/vbox/group","ewl/box/mainbox");
    ewl_widget_show(box2);
        
    border=ewl_border_new();
    ewl_object_fill_policy_set(EWL_OBJECT(border), EWL_FLAG_FILL_ALL);
    ewl_container_child_append(EWL_CONTAINER(box2),border);
    ewl_widget_name_set(border,"mainborder");
    ewl_object_maximum_w_set(EWL_OBJECT(EWL_BORDER(border)->label),500);
    ewl_widget_show(border);
        
    update_title();

    box3 = ewl_vbox_new();
    ewl_container_child_append(EWL_CONTAINER(border),box3);
    ewl_object_fill_policy_set(EWL_OBJECT(box3), EWL_FLAG_FILL_VSHRINK|EWL_FLAG_FILL_HFILL);
    ewl_widget_show(box3);	
        
    box4 = ewl_hbox_new();
    ewl_container_child_append(EWL_CONTAINER(border),box4);
    ewl_object_fill_policy_set(EWL_OBJECT(box4), EWL_FLAG_FILL_HSHRINK|EWL_FLAG_FILL_VSHRINK);
    ewl_object_alignment_set(EWL_OBJECT(box4),EWL_FLAG_ALIGN_RIGHT);
    ewl_widget_show(box4);	

    backarr = ewl_image_new();
    ewl_image_file_path_set(EWL_IMAGE(backarr),"/usr/share/madshelf/backarr.png");
    ewl_container_child_append(EWL_CONTAINER(box4), backarr);
    ewl_widget_name_set(backarr,"backarr");
    ewl_object_alignment_set(EWL_OBJECT(backarr),EWL_FLAG_ALIGN_LEFT|EWL_FLAG_ALIGN_TOP);
        
    forwardarr = ewl_image_new();
    ewl_image_file_path_set(EWL_IMAGE(forwardarr),"/usr/share/madshelf/forwardarr.png");
    ewl_container_child_append(EWL_CONTAINER(box4), forwardarr);
    ewl_widget_name_set(forwardarr,"forwardarr");
    ewl_object_alignment_set(EWL_OBJECT(forwardarr),EWL_FLAG_ALIGN_RIGHT|EWL_FLAG_ALIGN_TOP);

    menubar=ewl_hmenubar_new();

    {
        int i;
        Ewl_Widget *temp=NULL;
        Ewl_Widget *temp2=NULL;
        Ewl_Widget *temp3=NULL;
        temp=ewl_menu_new();	

        ewl_container_child_append(EWL_CONTAINER(menubar),temp);
        ewl_widget_name_set(temp,"okmenu");
        ewl_callback_append(EWL_MENU(temp)->popup, EWL_CALLBACK_KEY_DOWN, cb_menu_key_down, NULL);
        
                
                
        ewl_widget_show(temp);
                

        temp2=ewl_menu_item_new();
        ewl_widget_name_set(temp2,"menuitem1");

        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
                
        ewl_widget_show(temp2);

        temp2=ewl_menu_item_new();

        ewl_widget_name_set(temp2,"menuitem2");
        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        ewl_widget_show(temp2);

        temp2=ewl_menu_item_new();

        ewl_widget_name_set(temp2,"menuitem3");
        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        ewl_widget_show(temp2);

        temp2=ewl_menu_new();

        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
                
        ewl_widget_name_set(temp2,"menuitem4");
        ewl_callback_append(EWL_MENU(temp2)->popup, EWL_CALLBACK_KEY_DOWN, cb_lang_menu_key_down, NULL);
                
                //ewl_object_alignment_set(EWL_OBJECT(temp2),EWL_FLAG_ALIGN_BOTTOM);
        ewl_widget_show(temp2);

        for(i = 0; i < g_nlanguages; ++i)
        {
            Ewl_Widget* lang_menu_item = ewl_menu_item_new();
            ewl_button_label_set(EWL_BUTTON(lang_menu_item), g_languages[i].name);
            ewl_container_child_append(EWL_CONTAINER(temp2), lang_menu_item);
            ewl_widget_show(lang_menu_item);
        }

        temp2=ewl_menu_new();
        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        ewl_widget_name_set(temp2,"menuitem5");
                
        ewl_callback_append(EWL_MENU(temp2)->popup, EWL_CALLBACK_KEY_DOWN, cb_goto_menu_key_down, NULL);
        ewl_widget_show(temp2);

        for(i = 0; i < MIN(g_roots->nroots, 8); ++i)
        {
            temp3=ewl_menu_item_new();
            tempstr4=(char *)calloc(strlen(g_roots->roots[i].name)+3+1,sizeof(char));
            sprintf(tempstr4,"%d. %s",i+1, g_roots->roots[i].name);
            ewl_button_label_set(EWL_BUTTON(temp3),tempstr4);
            free(tempstr4);
            ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
            ewl_widget_show(temp3);
            count++;
        }

        temp2=ewl_menu_new();
        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        ewl_widget_name_set(temp2,"menuitem6");
                
        ewl_callback_append(EWL_MENU(temp2)->popup, EWL_CALLBACK_KEY_DOWN, cb_script_menu_key_down, NULL);
        ewl_widget_show(temp2);

        count=0;
        while(scriptstrlist[count]!=NULL)
        {
            temp3=ewl_menu_item_new();
            tempstr4=(char *)calloc(strlen(scriptstrlist[count])+3+1,sizeof(char));
            sprintf(tempstr4,"%d. %s",count+1,scriptstrlist[count]);
            ewl_button_label_set(EWL_BUTTON(temp3),tempstr4);
            free(tempstr4);
            ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
            ewl_widget_show(temp3);
            count++;
        }
    }
    ewl_container_child_append(EWL_CONTAINER(box2),menubar);
        
    update_menu();
    ewl_widget_show(menubar);

    sorttypetext=ewl_label_new();
    ewl_container_child_append(EWL_CONTAINER(box3), sorttypetext);
    ewl_widget_name_set(sorttypetext,"sortlabel");
    ewl_theme_data_str_set(EWL_WIDGET(sorttypetext),"/label/textpart","ewl/oi_label/sorttext");
    update_sort_label();
    ewl_widget_show(sorttypetext);

    for(count=0;count<8;count++)
    {
        sprintf(tempname1,"bookbox%d",count);
        box = ewl_hbox_new();
        ewl_container_child_append(EWL_CONTAINER(box3), box);
        sprintf(tempname5,"ewl/box/oi_bookbox%d",count+1);
        ewl_theme_data_str_set(EWL_WIDGET(box),"/hbox/group",tempname5);
        ewl_widget_name_set(box,tempname1 );
        //ewl_widget_show(box);
                
        sprintf (tempname2, "type%d",count);
        iconimage[count] = ewl_image_new();
                
        ewl_container_child_append(EWL_CONTAINER(box), iconimage[count]);
                //ewl_object_size_request ( EWL_OBJECT ( iconimage[count] ), 64,64 );
        ewl_object_insets_set(EWL_OBJECT ( iconimage[count] ),strtol(edje_file_data_get(DEFAULT_THEME,"/madshelf/icon/inset"),NULL,10),0,0,0);
        ewl_object_padding_set(EWL_OBJECT ( iconimage[count] ),0,0,0,0);
        ewl_widget_name_set(iconimage[count],tempname2 );
        ewl_object_alignment_set(EWL_OBJECT(iconimage[count]),EWL_FLAG_ALIGN_LEFT|EWL_FLAG_ALIGN_BOTTOM);
        //ewl_widget_show(iconimage[count]);
                
        sprintf (tempname3, "labelsbox%d",count);        
        box5 = ewl_vbox_new();
        ewl_container_child_append(EWL_CONTAINER(box),box5);
        ewl_widget_name_set(box5,tempname3 );
        ewl_object_fill_policy_set(EWL_OBJECT(box5), EWL_FLAG_FILL_ALL);
        //ewl_widget_show(box5);

        sprintf (tempname3, "authorlabel%d",count);
        authorlabel[count] = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box5), authorlabel[count]);
        ewl_widget_name_set(authorlabel[count],tempname3 );
        ewl_label_text_set(EWL_LABEL(authorlabel[count]), "Unknown Author");
        ewl_object_padding_set(EWL_OBJECT(authorlabel[count]),3,0,0,0);
        ewl_theme_data_str_set(EWL_WIDGET(authorlabel[count]),"/label/textpart","ewl/oi_label/authortext");
        ewl_object_fill_policy_set(EWL_OBJECT(authorlabel[count]), EWL_FLAG_FILL_VSHRINK| EWL_FLAG_FILL_HFILL);
        //ewl_widget_show(authorlabel[count]);

        sprintf (tempname3, "titlelabel%d",count);
        titlelabel[count] = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box5), titlelabel[count]);
        ewl_widget_name_set(titlelabel[count],tempname3 );
        ewl_label_text_set(EWL_LABEL(titlelabel[count]), "");
        ewl_object_padding_set(EWL_OBJECT(titlelabel[count]),3,0,0,0);
        ewl_theme_data_str_set(EWL_WIDGET(titlelabel[count]),"/label/textpart","ewl/oi_label/titletext");
        ewl_object_fill_policy_set(EWL_OBJECT(titlelabel[count]), EWL_FLAG_FILL_VSHRINK| EWL_FLAG_FILL_HFILL);

        //ewl_widget_show(titlelabel[count]);
        sprintf (tempname3, "infolabel%d",count);
        infolabel[count] = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box5), infolabel[count]);
        ewl_widget_name_set(infolabel[count],tempname3 );
        ewl_label_text_set(EWL_LABEL(infolabel[count]), "blala");
        ewl_object_padding_set(EWL_OBJECT(infolabel[count]),0,3,0,0);
        ewl_object_alignment_set(EWL_OBJECT(infolabel[count]),EWL_FLAG_ALIGN_RIGHT|EWL_FLAG_ALIGN_BOTTOM);

        ewl_theme_data_str_set(EWL_WIDGET(infolabel[count]),"/label/textpart","ewl/oi_label/infotext");
        ewl_object_fill_policy_set(EWL_OBJECT(infolabel[count]), EWL_FLAG_FILL_VSHRINK| EWL_FLAG_FILL_HFILL);
        //ewl_widget_show(infolabel[count]);
        
        sprintf(tempname4,"separator%d",count);
        dividewidget = ewl_hseparator_new();
        ewl_container_child_append(EWL_CONTAINER(box3), dividewidget);

        ewl_widget_name_set(dividewidget,tempname4 );
        //ewl_widget_show(dividewidget);
    }
    update_list();
    ewl_widget_focus_send(EWL_WIDGET(border));
        
    ewl_main();

    save_state();
    free(statefilename);
    eet_shutdown();
    CloseIniFile ();
    ecore_list_destroy(filelist);

    roots_destroy(g_roots);

    free(scriptstrlist);
    free(curdir);
    EXTRACTOR_removeAll(extractors);
    if (g_file)
    {
        const char* home = getenv("HOME");

        if (home)
        {
            char* handler_path = malloc(strlen(home)
                    + sizeof(SCRIPTS_DIR)/sizeof(char)
                    + strlen(g_handler));
            sprintf(handler_path, "%s%s%s", home, SCRIPTS_DIR, g_handler);

            execl(handler_path, handler_path, g_file, NULL);

            if (errno != ENOENT)
            {
                perror("madshelf: execl");
                return 1;
            }
        }

        execlp(g_handler, g_handler, g_file, NULL);
        perror("madshelf: execlp");
        return 1;
    }
        
    return 0;
}
