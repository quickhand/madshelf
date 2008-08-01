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
#include <dirent.h>
#include <Ecore_File.h>
#include <Edje.h>
#include <Eet.h>
#include <errno.h>
#include <Ewl.h>
#include <ewl_list.h>
#include <ewl_macros.h>
#include "madshelf_extractors.h"
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

/* Forward declarations */

void show_main_menu();


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

int g_nfileslist;
struct dirent** g_fileslist;

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

extractors_t *extractors;

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

#define KILOBYTE (1024)
#define MEGABYTE (1024*1024)

/*
 * Returns malloc(3)ed string.
 */
char* format_size(off_t size)
{
    char* res;

    if(size >= MEGABYTE)
        asprintf(&res, "%.1fM", ((double)size)/((double)MEGABYTE));
    else
        asprintf(&res, "%dk", (int)(((double)size)/((double)KILOBYTE)+0.5));

    return res;
}

#define TIME_LEN 100

/*
 * Returns malloc(3)ed string.
 */
char* format_time(time_t t)
{
    char* res = malloc(TIME_LEN*sizeof(char));
    struct tm* atime = localtime(&t);
    strftime(res, TIME_LEN, gettext("%m-%d-%y"), atime);
    return res;
}

void update_list()
{
    int offset=0;
    int count=0;
    char tempname[20];
    char *pointptr;
    const char *tempstr2;
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
    const char *extracted_title = NULL;
    const char *extracted_author = NULL;
    count=0;

    if(g_nfileslist > 0 && curindex >= g_nfileslist)
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

    if(g_nfileslist > 0)
    {
        if(curindex >= g_nfileslist)
        {
            curindex-=8;
            return;
        }

        for(count = 0; count < 8; count++)
        {
            if(curindex + count >= g_nfileslist)
                break;
            char* file = g_fileslist[curindex+count]->d_name;

            char* fileconcat;
            struct stat stat_p;
            char* time_str;
            char *extension = strrchr(file, '.');

            asprintf(&fileconcat, "%s%s", curdir, file);
            stat(fileconcat, &stat_p);
            time_str = format_time(stat_p.st_mtime);

            if(ecore_file_is_dir(fileconcat))
            {
                ewl_label_text_set(EWL_LABEL(titlelabel[count]),ecore_file_strip_ext(file));

                ewl_label_text_set(EWL_LABEL(infolabel[count]),time_str);
                ewl_label_text_set(EWL_LABEL(authorlabel[count]),"Folder");
                ewl_image_file_path_set(EWL_IMAGE(typeicon[count]),"/usr/share/madshelf/folder.png");
            }
            else
            {
                char* size_str = format_size(stat_p.st_size);
                char* infostr;
                char* imagefile;

                EXTRACTOR_KeywordList* mykeys;
                mykeys = extractor_get_keywords(extractors, fileconcat);

                extracted_title = EXTRACTOR_extractLast(EXTRACTOR_TITLE,mykeys);
                extracted_author = EXTRACTOR_extractLast(EXTRACTOR_AUTHOR,mykeys);

                if(extracted_title && extracted_title[0])
                    ewl_label_text_set(EWL_LABEL(titlelabel[count]), extracted_title);
                else
                    ewl_label_text_set(EWL_LABEL(titlelabel[count]),ecore_file_strip_ext(file));

                extension = strrchr(file, '.');

                asprintf(&infostr, "%s%s%s   %s",
                         extension ? extension : "",
                         extension ? "   " : "",
                         time_str,
                         size_str);

                ewl_label_text_set(EWL_LABEL(infolabel[count]),infostr);

                if(extracted_author && extracted_author[0])
                    ewl_label_text_set(EWL_LABEL(authorlabel[count]), extracted_author);
                else
                    ewl_label_text_set(EWL_LABEL(authorlabel[count]), gettext("Unknown Author"));

                pointptr=strrchr(fileconcat,'.');
                if(pointptr==NULL)
                    tempstr2=ReadString("icons",".","default.png");
                else
                    tempstr2=ReadString("icons",pointptr,"default.png");
                asprintf(&imagefile, "/usr/share/madshelf/%s", tempstr2);
                ewl_image_file_path_set(EWL_IMAGE(typeicon[count]), imagefile);

                free(imagefile);
                free(infostr);
                free(size_str);
            }

            showflag[count]=1;

            free(time_str);
            free(fileconcat);
        }

        for(; count < 8; count++)
        {
            showflag[count]=0;
        }
        if(curindex+8 >= g_nfileslist)
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

char* get_sort_label(int sort_type, int sort_order)
{
    if(sort_type == SORT_BY_NAME)
    {
        if(sort_order==ECORE_SORT_MAX)
            return gettext("reverse-sorted by name");
        else if(sort_order == ECORE_SORT_MIN)
            return gettext("sorted by name");
    }
    else if(sort_type == SORT_BY_TIME)
    {
        if(sort_order==ECORE_SORT_MAX)
            return gettext("reverse-sorted by time");
        else if(sort_order == ECORE_SORT_MIN)
            return gettext("sorted by time");
    }

    fprintf(stderr, "FATAL: unknown sort_type and/or sort_order\n");
    abort();
}

void update_sort_label()
{
    Ewl_Widget *curwidget;

    curwidget = ewl_widget_name_find("sortlabel");
    ewl_label_text_set(EWL_LABEL(curwidget),
                       get_sort_label(sort_type, sort_order));

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

static int rev_alphasort(const void* lhs, const void* rhs)
{
    return alphasort(rhs, lhs);
}

static long long rel_file_mtime(const char* f)
{
    char* filename;
    struct stat st;
    asprintf(&filename, "%s%s", curdir, f);
    stat(filename, &st);
    free(filename);
    return st.st_mtime;
}

static int date_cmp(const struct dirent** lhs, const struct dirent** rhs)
{
    long long lhs_mtime = rel_file_mtime((*lhs)->d_name);
    long long rhs_mtime = rel_file_mtime((*rhs)->d_name);

    if(lhs_mtime > rhs_mtime) return 1;
    if(lhs_mtime < rhs_mtime) return -1;
    return 0;
}

static int rev_date_cmp(const struct dirent** lhs, const struct dirent** rhs)
{
    return date_cmp(rhs, lhs);
}

static int filter_dotfiles(const struct dirent* f)
{
    return (f->d_type == DT_REG || f->d_type == DT_DIR)
        && (f->d_name[0] != '.');
}

typedef int (*compar_t)(const void*, const void*);

void init_filelist()
{
    compar_t cmp;

    if(sort_type == SORT_BY_NAME)
        cmp = (compar_t)(sort_order == ECORE_SORT_MIN ? alphasort : rev_alphasort);
    else
        cmp = (compar_t)(sort_order == ECORE_SORT_MIN ? date_cmp : rev_date_cmp);

    g_nfileslist = scandir(curdir, &g_fileslist,
                           &filter_dotfiles,
                           cmp);

    if(g_nfileslist == -1)
    {
        /* FIXME: handle somehow */

        g_nfileslist = 0;
    }
}

void fini_filelist()
{
    int i;
    for(i = 0; i < g_nfileslist; ++i)
        free(g_fileslist[i]);
    free(g_fileslist);

    g_nfileslist = 0;
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
    if(curindex+(num-1)>= g_nfileslist)
        return;

    //ecore_list_index_goto(filelist,curindex+num-1);
    file = g_fileslist[curindex + num - 1]->d_name;

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
        fini_filelist();
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

/* GUI */

typedef void (*key_handler_t)();
typedef void (*item_handler_t)(int index);

typedef struct
{
    key_handler_t ok_handler;
    key_handler_t esc_handler;
    item_handler_t item_handler;
} key_handler_info_t;

static void _key_handler(Ewl_Widget* w, void *event, void *context)
{
    Ewl_Event_Key_Up* e = (Ewl_Event_Key_Up*)event;
    key_handler_info_t* handler_info = (key_handler_info_t*)context;

    const char* k = e->base.keyname;

    if(!strcmp(k, "Return"))
    {
        if(handler_info->ok_handler)
            (*handler_info->ok_handler)();
    }
    else if(!strcmp(k, "Escape"))
    {
        if(handler_info->esc_handler)
            (*handler_info->esc_handler)();
    }
    else if (isdigit(k[0]) && !k[1])
    {
        if (handler_info->item_handler)
            (*handler_info->item_handler)(k[0] - '0');
    }
}

void set_key_handler(Ewl_Widget* widget, key_handler_info_t* handler_info)
{
    ewl_callback_append(widget, EWL_CALLBACK_KEY_UP,
                        &_key_handler, handler_info);
}

/* Main key handler */

void main_esc()
{
    char* tmpchrptr;

    if(depth==0)
        return;
    tmpchrptr=getUpLevelDir(curdir);
    if(tmpchrptr==NULL)
        return;

    free(curdir);
    curdir=tmpchrptr;
    fini_filelist();
    init_filelist();
    depth--;
    curindex=0;
    update_list();
    update_title();
}

void main_ok(void)
{
    show_main_menu();
}

void main_item(int item)
{
    if(item == 0)
    {
        curindex+=8;
        update_list();
    }
    else if(item == 9)
    {
        if(curindex>0)
        {
            curindex-=8;
            update_list();
        }
    }
    else
        doActionForNum(item);
}

static key_handler_info_t main_info =
{
    .ok_handler = &main_ok,
    .esc_handler = &main_esc,
    .item_handler = &main_item,
};

/* Main menu */

void show_main_menu()
{
    ewl_menu_cb_expand(ewl_widget_name_find("okmenu"),NULL,NULL);
}

void hide_main_menu()
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("okmenu")));
}

void main_menu_esc()
{
    hide_main_menu();
}

void main_menu_item(int item)
{
    Ewl_Widget* curwidget;

    switch(item)
    {
    case 1:
        sort_order=ECORE_SORT_MIN;
        sort_type=SORT_BY_NAME;

        fini_filelist();
        init_filelist();

        curindex=0;
        update_list();
        update_sort_label();
        hide_main_menu();
        break;
    case 2:
        sort_order=ECORE_SORT_MIN;
        sort_type=SORT_BY_TIME;

        fini_filelist();
        init_filelist();

        curindex=0;
        update_list();
        update_sort_label();
        hide_main_menu();
        break;
    case 3:
        if(sort_order==ECORE_SORT_MIN)
            sort_order=ECORE_SORT_MAX;
        else
            sort_order=ECORE_SORT_MIN;

        fini_filelist();
        init_filelist();

        curindex=0;
        update_list();
        update_sort_label();
        hide_main_menu();
        break;
    case 4:
        curwidget = ewl_widget_name_find("menuitem4");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
        break;
    case 5:
        curwidget = ewl_widget_name_find("menuitem5");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
        break;
    case 6:
        curwidget = ewl_widget_name_find("menuitem6");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
        break;
    }
}

static key_handler_info_t main_menu_info =
{
    .ok_handler = &main_menu_esc,
    .esc_handler = &main_menu_esc,
    .item_handler = &main_menu_item,
};

/* Languages menu */

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

void lang_menu_esc()
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem4")));
    hide_main_menu();
}

void lang_menu_item(int item)
{
    Ewl_Widget* curwidget;

    item--;

    setenv("LANGUAGE", g_languages[item].locale, 1);

    /*
     * gettext needs to be notified about language change
     */
    {
        extern int  _nl_msg_cat_cntr;
        ++_nl_msg_cat_cntr;
    }

    curwidget = ewl_widget_name_find("menuitem4");
    ewl_menu_collapse(EWL_MENU(curwidget));
    hide_main_menu();
    update_title();
    update_sort_label();
    update_menu();
}

static key_handler_info_t lang_menu_info =
{
    .ok_handler = &lang_menu_esc,
    .esc_handler = &lang_menu_esc,
    .item_handler = &lang_menu_item,
};

/* "Go to" menu */

void goto_menu_esc()
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem5")));
    hide_main_menu();
}

void goto_menu_item(int item)
{
    item--;

    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem5")));
    hide_main_menu();

    curdir = strdup(g_roots->roots[item].path);
    current_root = item;

    initdirstrlen=strlen(curdir);
    depth=0;
    curindex=0;
    init_filelist();
    update_title();
    update_list();
}

static key_handler_info_t goto_menu_info =
{
    .ok_handler = &goto_menu_esc,
    .esc_handler = &goto_menu_esc,
    .item_handler = &goto_menu_item,
};

/* "Scripts" menu */

void scripts_menu_esc()
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem6")));
    hide_main_menu();
}

void scripts_menu_item(int item)
{
    const char* tempstr;
    char* handler_path;

    item--;

    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem5")));
    hide_main_menu();

    /* ?! */
    if(scriptstrlist[item] == NULL)
        return;

    tempstr=ReadString("scripts",scriptstrlist[item],NULL);
    handler_path = malloc(strlen(getenv("HOME"))+sizeof(SCRIPTS_DIR)/sizeof(char)+strlen(tempstr));
    sprintf(handler_path, "%s%s%s", getenv("HOME"), SCRIPTS_DIR,tempstr);

    system(handler_path);

    free(handler_path);
}

static key_handler_info_t scripts_menu_info =
{
    .ok_handler = &scripts_menu_esc,
    .esc_handler = &scripts_menu_esc,
    .item_handler = &scripts_menu_item,
};

/* State */

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

    extractors= load_extractors();
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
    set_key_handler(win, &main_info);
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
        set_key_handler(EWL_MENU(temp)->popup, &main_menu_info);

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

        set_key_handler(EWL_MENU(temp2)->popup, &lang_menu_info);

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

        set_key_handler(EWL_MENU(temp2)->popup, &goto_menu_info);
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

        set_key_handler(EWL_MENU(temp2)->popup, &scripts_menu_info);
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
    fini_filelist();

    roots_destroy(g_roots);

    free(scriptstrlist);
    free(curdir);
    unload_extractors(extractors);
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
