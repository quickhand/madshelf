/*
 * MadShelf - bookshelf application.
 *
 * Copyright (C) 2008 by Marc Lajoie
 * Copyright (C) 2008 Mikhail Gusarov <dottedmag@dottedmag.net>
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


//***********these variables need to be saved and restored in order to restore the state
int current_index;

/*
 * It is guaranteed that g_roots[current_root]->path is a prefix of cwd.
 */
int current_root;

int sort_type=SORT_BY_NAME;
int sort_order=ECORE_SORT_MIN;
//***********
int num_books=8;
int nav_mode;
int nav_sel=0;
int nav_menu_sel=0;
int nav_lang_menu_sel=0;
int nav_goto_menu_sel=0;
int nav_scripts_menu_sel=0;


#define REL_THEME "themes/madshelf.edj"
#define SYSTEM_THEME "/usr/share/madshelf/madshelf.edj"

/*
 * Returns edje theme file name.
 */
char* get_theme_file()
{
    char* cwd = get_current_dir_name();
    char* rel_theme;
    asprintf(&rel_theme, "%s/%s", cwd, REL_THEME);
    free(cwd);
    if(0 == access(rel_theme, R_OK))
        return rel_theme;
    free(rel_theme);

    if(0 == access(SYSTEM_THEME, R_OK))
        return strdup(SYSTEM_THEME);

    fprintf(stderr, "Unable to find any theme. Silly me.\n");
    exit(1);
}


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
        int path_len;

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

        asprintf(&name, "%.*s", line_sep - conf_line, conf_line);
        roots[i].name = name;

        roots[i].path = strdup(line_sep+1);
        path_len = strlen(roots[i].path);
        if(path_len > 0 && roots[i].path[path_len-1] == '/')
            roots[i].path[path_len-1] = 0;

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

int next_page_exists()
{
    return g_nfileslist > current_index + num_books;
}

void next_page()
{
    if(next_page_exists())
    {
        current_index += num_books;
        update_list();
    }
}

int prev_page_exists()
{
    return current_index > 0;
}

void prev_page()
{
    if(prev_page_exists())
    {
        current_index -= num_books;
        if(current_index < 0)
            current_index = 0;
        update_list();
    }
}

/*
 * If something wicked happened, just move to some allowed directory. If it is
 * not possible - crash loudly.
 */
void emergency_chdir()
{
    int i;
    fprintf(stderr,
            "Emergency chdir activated: trying to find any chdir'able root.\n");

    for(i = 0; i < g_roots->nroots; ++i)
    {
        fprintf(stderr, "Trying %s\n", g_roots->roots[i].path);
        if(0 == chdir(g_roots->roots[i].path))
        {
            current_root = i;
            fprintf(stderr, "Succesfully. Back to normal operation.\n");
            return;
        }
    }

    /*
     * Okay, something is *really* bad.
     */
    fprintf(stderr,
            "** Unable to find any remotely sane directory to chdir to. **\n");
    exit(1);
}


/* Maintaining invariant "current root is a prefix of current dir" */
int is_cwd_in_root(int n)
{
    char* cwd = get_current_dir_name();
    char* root = g_roots->roots[n].path;
    int rootlen = strlen(root);

    fprintf(stderr, "Checking legality of %s in root %d (%s)\n",
            cwd, n, root);

    int in_root = !strncmp(cwd, root, rootlen)
        && (cwd[rootlen] == 0 || cwd[rootlen] == '/');

    fprintf(stderr, "%s is %slegal in %d (%s)\n",
            cwd, in_root ? "" : "not ", n, root);

    free(cwd);
    return in_root;
}

/*
 * chdir, checking that target directory is in given root.
 */
int chdir_to_in_root(const char* file, int root)
{
    fprintf(stderr, "Trying to chdir %s in root %d.\n", file, root);

    /* Keeping current directory, as a fallback if something goes wrong later */
    int curfd = open(".", O_DIRECTORY | O_RDONLY);

    if(-1 == chdir(file))
    {
        fprintf(stderr, "Unable to chdir to %s: %s.\n", file, strerror(errno));

        /* Unable to chdir due to some reason. */
        if(curfd != -1)
            close(curfd);
        return -1;
    }

    fprintf(stderr, "Successfully chdir to %s. Checking legality.\n", file);

    if(!is_cwd_in_root(root))
    {
        fprintf(stderr, "%s is illegal in root %d. Falling back.\n",
                file, root);

        if(curfd != -1 && fchdir(curfd) == 0)
            return -1;

        /*
         * We've moved to directory which is not allowed, and unable to move
         * back. Time to ask for help.
         */
        if(curfd != -1)
            close(curfd);
        emergency_chdir();
        return -1;
    }

    if(curfd != -1)
        close(curfd);

    return 0;
}

int chdir_to(const char* file)
{
    return chdir_to_in_root(file, current_root);
}

void update_list()
{
    int count=0;
    char tempname[20];
    char *pointptr;
    const char *tempstr2;
    Ewl_Widget **labelsbox;
    Ewl_Widget **titlelabel;
    Ewl_Widget **authorlabel;
    Ewl_Widget **infolabel;
    Ewl_Widget **bookbox;
    Ewl_Widget **separator;
    Ewl_Widget **typeicon;
    Ewl_Widget *arrow_widget;
    int *showflag;
    const char *extracted_title = NULL;
    const char *extracted_author = NULL;
    count=0;

    labelsbox=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    titlelabel=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    authorlabel=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    infolabel=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    bookbox=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    separator=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    typeicon=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));

    showflag = alloca(num_books * sizeof(int));

    for(count=0;count<num_books;count++)
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
    sprintf (tempname, "arrow_widget");
    arrow_widget = ewl_widget_name_find(tempname);
    

    for(count=0;count<num_books;count++)
    {
        ewl_widget_hide(bookbox[count]);
        ewl_widget_hide(labelsbox[count]);
        ewl_widget_hide(separator[count]);
        ewl_widget_hide(authorlabel[count]);
        ewl_widget_hide(titlelabel[count]);
        ewl_widget_hide(infolabel[count]);
        ewl_widget_hide(typeicon[count]);
    }

    for(count = 0; count < num_books; count++)
    {
        if(current_index + count >= g_nfileslist)
        {
            showflag[count]=0;
            continue;
        }

        if(nav_mode==1)
        {
            if(count==nav_sel)
                ewl_widget_state_set(bookbox[count],"select",EWL_STATE_PERSISTENT);
            else
                ewl_widget_state_set(bookbox[count],"unselect",EWL_STATE_PERSISTENT);
        }
            
        char* file = g_fileslist[current_index + count]->d_name;

        struct stat stat_p;
        char* time_str;
        char *extension = strrchr(file, '.');

        stat(file, &stat_p);
        time_str = format_time(stat_p.st_mtime);

        if(ecore_file_is_dir(file))
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
            mykeys = extractor_get_keywords(extractors, file);

            extracted_title = extractor_get_last(EXTRACTOR_TITLE, mykeys);
            extracted_author = extractor_get_last(EXTRACTOR_AUTHOR, mykeys);

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

            pointptr=strrchr(file,'.');
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
    }

    if(next_page_exists() && prev_page_exists())
        ewl_widget_state_set(arrow_widget,"both_on",EWL_STATE_PERSISTENT);
    else if(next_page_exists())
        ewl_widget_state_set(arrow_widget,"right_only",EWL_STATE_PERSISTENT);
    else if(prev_page_exists())
        ewl_widget_state_set(arrow_widget,"left_only",EWL_STATE_PERSISTENT);

    for(count=0;count<num_books;count++)
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
}

void update_title()
{
    char* titletext;
    char* cwd = get_current_dir_name();

    int notroot = strcmp(g_roots->roots[current_root].path, cwd);

    asprintf(&titletext, "Madshelf | %s%s%s",
             g_roots->roots[current_root].name,
             notroot ? "://" : "",
             notroot ? cwd + strlen(g_roots->roots[current_root].path) : "");

    ewl_border_label_set(EWL_BORDER(ewl_widget_name_find("mainborder")),
                         titletext);

    free(titletext);
    free(cwd);
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
    char *tempstrings[]={"Sort by Name","Sort by Time","Reverse Sort Order","Language Settings","Go to","Scripts"};
    char tempname[30];
    char temptext[40];
    int i=0;
    Ewl_Widget *curwidget;
    curwidget = ewl_widget_name_find("okmenu");
    ewl_button_label_set(EWL_BUTTON(curwidget),gettext("Menu"));
    for(i=0;i<6;i++)
    {
        sprintf(tempname,"menuitem%d",i+1);
        curwidget = ewl_widget_name_find(tempname);
        if(nav_mode==0)
            sprintf(temptext,"%d. %s",i+1,gettext(tempstrings[i]));
        else
            sprintf(temptext,"%s",gettext(tempstrings[i]));
        ewl_button_label_set(EWL_BUTTON(curwidget),temptext);
    }
}

static int rev_alphasort(const void* lhs, const void* rhs)
{
    return alphasort(rhs, lhs);
}

static long long rel_file_mtime(const char* f)
{
    struct stat st;
    stat(f, &st);
    return st.st_mtime;
}

static int date_cmp(const struct dirent** lhs, const struct dirent** rhs)
{
    long long lhs_mtime = rel_file_mtime((*lhs)->d_name);
    long long rhs_mtime = rel_file_mtime((*rhs)->d_name);

    if(lhs_mtime > rhs_mtime) return 1;
    if(lhs_mtime < rhs_mtime) return -1;

    return alphasort(lhs, rhs);
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

void fini_filelist()
{
    int i;
    for(i = 0; i < g_nfileslist; ++i)
        free(g_fileslist[i]);
    free(g_fileslist);
}

void init_filelist()
{
    compar_t cmp;

    fini_filelist();

    if(sort_type == SORT_BY_NAME)
        cmp = (compar_t)(sort_order == ECORE_SORT_MIN ? alphasort : rev_alphasort);
    else
        cmp = (compar_t)(sort_order == ECORE_SORT_MIN ? date_cmp : rev_date_cmp);

    g_nfileslist = scandir(".", &g_fileslist,
                           &filter_dotfiles,
                           cmp);

    if(g_nfileslist == -1)
    {
        /* FIXME: handle somehow */

        g_nfileslist = 0;
    }

    current_index = 0;
    nav_sel = 0;
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

void update_filelist_in_gui()
{
    update_list();
    update_title();
}

void change_dir_in_gui()
{
    init_filelist();
    update_filelist_in_gui();
}

void doActionForNum(unsigned int num)
{
    char *file;
    int file_index = current_index + num - 1;

    if(file_index >= g_nfileslist)
        return;

    file = g_fileslist[file_index]->d_name;

    if(!ecore_file_is_dir(file))
    {
        const char* handler = lookup_handler(file);
        if (handler)
        {
            /* Sin */
            g_handler = handler;
            g_file = file;
            ewl_main_quit();
        }
        else
        {
            fprintf(stderr, "Unable to find handler for %s\n", file);
        }
    }
    else
    {
        chdir_to(file);
        change_dir_in_gui();
    }
}

void change_root(int item)
{
    if(chdir_to_in_root(g_roots->roots[item].path, item) == 0)
        current_root = item;
}

/* GUI */

typedef void (*key_handler_t)();
typedef void (*item_handler_t)(int index);

typedef struct
{
    key_handler_t ok_handler;
    key_handler_t esc_handler;
    key_handler_t nav_up_handler;
    key_handler_t nav_down_handler;
    key_handler_t nav_left_handler;
    key_handler_t nav_right_handler;
    key_handler_t nav_sel_handler;
    key_handler_t nav_menubtn_handler;
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
    else if (!strcmp(k,"Up"))
    {
        /* FIXME: HACK */
        if(nav_mode == 1)
        {
            if(handler_info->nav_up_handler)
                (*handler_info->nav_up_handler)();
        }
        else
        {
            if(handler_info->nav_left_handler)
                (*handler_info->nav_left_handler)();
        }
    }
    else if (!strcmp(k,"Down"))
    {
        /* FIXME: HACK */
        if(nav_mode == 1)
        {
            if(handler_info->nav_down_handler)
                (*handler_info->nav_down_handler)();
        }
        else
        {
            if(handler_info->nav_right_handler)
                (*handler_info->nav_right_handler)();
        }
    }
    else if (!strcmp(k,"Left"))
    {
        if(handler_info->nav_left_handler)
            (*handler_info->nav_left_handler)();
    }
    else if (!strcmp(k,"Right"))
    {
        if(handler_info->nav_right_handler)
            (*handler_info->nav_right_handler)();
    }
    else if (!strcmp(k," "))
    {
        if(handler_info->nav_sel_handler)
            (*handler_info->nav_sel_handler)();
    }
    else if (!strcmp(k,"F1"))
    {
        if(handler_info->nav_menubtn_handler)
            (*handler_info->nav_menubtn_handler)();
    }
    else
        fprintf(stderr,k);

}

void set_key_handler(Ewl_Widget* widget, key_handler_info_t* handler_info)
{
    ewl_callback_append(widget, EWL_CALLBACK_KEY_UP,
                        &_key_handler, handler_info);
}

/* Main key handler */

void main_esc()
{
    int i;
    char* cwd = get_current_dir_name();
    char* cur_name = strrchr(cwd, '/') + 1;

    chdir_to("..");
    init_filelist();

    for(i = 0; i < g_nfileslist; i++)
        if(!strcmp(cur_name, g_fileslist[i]->d_name))
        {
            nav_sel = i%num_books;
            current_index = i-nav_sel;
            break;
        }

    free(cwd);

    update_filelist_in_gui();
}

void main_ok(void)
{
    show_main_menu();
}

void main_nav_up(void)
{
    char tempname[30];
    Ewl_Widget *curwidget=NULL;
    if((nav_sel-1)>=0)
    {
        sprintf (tempname, "bookbox%d",nav_sel);
        curwidget = ewl_widget_name_find(tempname);
        ewl_widget_state_set(curwidget,"unselect",EWL_STATE_PERSISTENT);
        nav_sel--;
        sprintf (tempname, "bookbox%d",nav_sel);
        curwidget = ewl_widget_name_find(tempname);
        ewl_widget_state_set(curwidget,"select",EWL_STATE_PERSISTENT);
    }       
}

void main_nav_down(void)
{
    char tempname[30];
    Ewl_Widget *curwidget=NULL;
    if((nav_sel+1)<num_books)
    {
        sprintf (tempname, "bookbox%d",nav_sel);
        curwidget = ewl_widget_name_find(tempname);
        ewl_widget_state_set(curwidget,"unselect",EWL_STATE_PERSISTENT);
        nav_sel++;
        sprintf (tempname, "bookbox%d",nav_sel);
        curwidget = ewl_widget_name_find(tempname);
        ewl_widget_state_set(curwidget,"select",EWL_STATE_PERSISTENT);
    }
}

void main_nav_left(void)
{
    nav_sel=0;
    prev_page();
}

void main_nav_right(void)
{
    nav_sel=0;
    next_page();
}

void main_nav_sel(void)
{
    
    doActionForNum(nav_sel+1);
    
}
void main_nav_menubtn(void)
{
    
    show_main_menu();
    
}
void main_item(int item)
{
    if(item == 0)
        next_page();
    else if(item == 9)
        prev_page();
    else
        doActionForNum(item);
}

static key_handler_info_t main_info =
{
    .ok_handler = &main_ok,
    .nav_up_handler=&main_nav_up,
    .nav_down_handler=&main_nav_down,
    .nav_left_handler=&main_nav_left,
    .nav_right_handler=&main_nav_right,
    .nav_sel_handler=&main_nav_sel,
    .nav_menubtn_handler=&main_nav_menubtn,
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

void main_menu_nav_up(void)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    if((nav_menu_sel-1)>=0)
    {
        
        sprintf (tempname, "menuitem%d",nav_menu_sel+1);
        oldselwid = ewl_widget_name_find(tempname);
        sprintf (tempname, "menuitem%d",nav_menu_sel);
        newselwid = ewl_widget_name_find(tempname);
        if(!oldselwid||!newselwid)
            return;
        ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
        nav_menu_sel--;
        ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
    }       
}

void main_menu_nav_down(void)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    if((nav_menu_sel+1)<num_books)
    {
        sprintf (tempname, "menuitem%d",nav_menu_sel+1);
        oldselwid = ewl_widget_name_find(tempname);
        sprintf (tempname, "menuitem%d",nav_menu_sel+2);
        newselwid = ewl_widget_name_find(tempname);
        if(!oldselwid||!newselwid)
            return;
        ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
        nav_menu_sel++;
        ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
    }
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

        init_filelist();

        update_list();
        update_sort_label();
        hide_main_menu();
        break;
    case 2:
        sort_order=ECORE_SORT_MIN;
        sort_type=SORT_BY_TIME;

        init_filelist();

        update_list();
        update_sort_label();
        hide_main_menu();
        break;
    case 3:
        if(sort_order==ECORE_SORT_MIN)
            sort_order=ECORE_SORT_MAX;
        else
            sort_order=ECORE_SORT_MIN;

        init_filelist();

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

void main_menu_nav_sel(void)
{
    main_menu_item(nav_menu_sel+1);
}

static key_handler_info_t main_menu_info =
{
    .ok_handler = &main_menu_esc,
    .esc_handler = &main_menu_esc,
    .nav_up_handler=&main_menu_nav_up,
    .nav_down_handler=&main_menu_nav_down,
    .nav_sel_handler=&main_menu_nav_sel,
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
    { "English", "en" },
    { "Français", "fr" },
    { "Русский", "ru" },
    { "简体中文", "zh_CN" }
};

static const int g_nlanguages = sizeof(g_languages)/sizeof(language_t);

void lang_menu_esc()
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem4")));
    hide_main_menu();
}
void lang_menu_nav_up(void)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    if((nav_lang_menu_sel-1)>=0)
    {
        
        sprintf (tempname, "langmenuitem%d",nav_lang_menu_sel+1);
        oldselwid = ewl_widget_name_find(tempname);
        sprintf (tempname, "langmenuitem%d",nav_lang_menu_sel);
        newselwid = ewl_widget_name_find(tempname);
        if(!oldselwid||!newselwid)
            return;
        ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
        nav_lang_menu_sel--;
        ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
    }       
}

void lang_menu_nav_down(void)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    
    sprintf (tempname, "langmenuitem%d",nav_lang_menu_sel+1);
    oldselwid = ewl_widget_name_find(tempname);
    sprintf (tempname, "langmenuitem%d",nav_lang_menu_sel+2);
    newselwid = ewl_widget_name_find(tempname);
    if(!oldselwid||!newselwid)
        return;
    ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
    nav_lang_menu_sel++;
    ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
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

void lang_menu_nav_sel(void)
{
    lang_menu_item(nav_lang_menu_sel+1);
}

static key_handler_info_t lang_menu_info =
{
    .ok_handler = &lang_menu_esc,
    .esc_handler = &lang_menu_esc,
    .nav_up_handler=&lang_menu_nav_up,
    .nav_down_handler=&lang_menu_nav_down,
    .nav_sel_handler=&lang_menu_nav_sel,
    .item_handler = &lang_menu_item,
};

/* "Go to" menu */

void goto_menu_esc()
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem5")));
    hide_main_menu();
}

void goto_menu_nav_up(void)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    if((nav_goto_menu_sel-1)>=0)
    {
        
        sprintf (tempname, "gotomenuitem%d",nav_goto_menu_sel+1);
        oldselwid = ewl_widget_name_find(tempname);
        sprintf (tempname, "gotomenuitem%d",nav_goto_menu_sel);
        newselwid = ewl_widget_name_find(tempname);
        if(!oldselwid||!newselwid)
            return;
        ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
        nav_goto_menu_sel--;
        ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
    }       
}

void goto_menu_nav_down(void)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    sprintf (tempname, "gotomenuitem%d",nav_goto_menu_sel+1);
    oldselwid = ewl_widget_name_find(tempname);
    sprintf (tempname, "gotomenuitem%d",nav_goto_menu_sel+2);
    newselwid = ewl_widget_name_find(tempname);
    if(!oldselwid||!newselwid)
        return;
    ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
    nav_goto_menu_sel++;
    ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
}


void goto_menu_item(int item)
{
    if(item == 0)
        item = 10;

    item--;

    if(item < 0 || item >= g_roots->nroots)
        return;

    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem5")));
    hide_main_menu();

    change_root(item);
    change_dir_in_gui();
}

void goto_menu_nav_sel(void)
{
    goto_menu_item(nav_goto_menu_sel+1);
}

static key_handler_info_t goto_menu_info =
{
    .ok_handler = &goto_menu_esc,
    .esc_handler = &goto_menu_esc,
    .nav_up_handler=&goto_menu_nav_up,
    .nav_down_handler=&goto_menu_nav_down,
    .nav_sel_handler=&goto_menu_nav_sel,
    .item_handler = &goto_menu_item,
};

/* "Scripts" menu */

void scripts_menu_esc()
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem6")));
    hide_main_menu();
}

void scripts_menu_nav_up(void)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    if((nav_scripts_menu_sel-1)>=0)
    {
        sprintf (tempname, "scriptsmenuitem%d",nav_scripts_menu_sel+1);
        oldselwid = ewl_widget_name_find(tempname);
        sprintf (tempname, "scriptsmenuitem%d",nav_scripts_menu_sel);
        newselwid = ewl_widget_name_find(tempname);
        if(!oldselwid||!newselwid)
            return;
        ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
        nav_scripts_menu_sel--;
        ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
    }       
}

void scripts_menu_nav_down(void)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    sprintf (tempname, "scriptsmenuitem%d",nav_scripts_menu_sel+1);
    oldselwid = ewl_widget_name_find(tempname);
    sprintf (tempname, "scriptsmenuitem%d",nav_scripts_menu_sel+2);
    newselwid = ewl_widget_name_find(tempname);
    if(!oldselwid||!newselwid)
        return;
    ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
    nav_scripts_menu_sel++;
    ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
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

void scripts_menu_nav_sel(void)
{
    scripts_menu_item(nav_scripts_menu_sel+1);
}

static key_handler_info_t scripts_menu_info =
{
    .ok_handler = &scripts_menu_esc,
    .esc_handler = &scripts_menu_esc,
    .nav_up_handler=&scripts_menu_nav_up,
    .nav_down_handler=&scripts_menu_nav_down,
    .nav_sel_handler=&scripts_menu_nav_sel,
    .item_handler = &scripts_menu_item,
};

/* State */

void save_state()
{
    Eet_File *state;
    state=eet_open(statefilename,EET_FILE_MODE_WRITE);
    const int a=1;
    char* cwd = get_current_dir_name();

    eet_write(state,"statesaved",(void *)&a, sizeof(int),0);
    eet_write(state,"curindex",(void *)&current_index,sizeof(int),0);
    eet_write(state,"rootname",(void *)g_roots->roots[current_root].name,
              sizeof(char)*(strlen(g_roots->roots[current_root].name)+1),0);
    eet_write(state,"curdir",cwd,sizeof(char)*(strlen(cwd)+1),0);
    eet_write(state,"sort_type",(void *)&sort_type,sizeof(int),0);
    eet_write(state,"sort_order",(void *)&sort_order,sizeof(int),0);
    eet_close(state);

    free(cwd);
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

    change_root(0);

    temp=(char *)eet_read(state,"rootname",&size);
    for(i = 1; i < g_roots->nroots; ++i)
        if (!strcmp(temp, g_roots->roots[i].name))
    {
        change_root(i);
        break;
    }

    chdir_to((char*)eet_read(state, "curdir", &size));
    init_filelist();
    current_index = *((int*)eet_read(state,"curindex",&size));
    if(current_index < 0 || current_index > g_nfileslist)
        current_index = 0;

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
    char tempname1[20];
    char tempname2[20];
    char tempname3[20];
    char tempname4[20];
    char tempname5[20];
    char tempname6[20];
    Ewl_Widget *win = NULL;
    Ewl_Widget *border = NULL;

    Ewl_Widget *box = NULL;
    Ewl_Widget *box2=NULL;
    Ewl_Widget *box3=NULL;
    Ewl_Widget *box4=NULL;
    Ewl_Widget *box5=NULL;
    Ewl_Widget *authorlabel;
    Ewl_Widget *titlelabel;
    Ewl_Widget *infolabel;
    Ewl_Widget *iconimage;
    Ewl_Widget *menubar=NULL;
    Ewl_Widget *arrow_widget=NULL;
    Ewl_Widget *sorttypetext;
    Ewl_Widget *dividewidget;
    char *homedir;
    char *configfile;
    int count=0;
    int count2=0;
    char *tempstr4;
    char *tempstr6;
    struct ENTRY *scriptlist;

    char* theme_file = get_theme_file();

    if ( !ewl_init ( &argc, argv ) )
    {
        return 1;
    }
    eet_init();
    ewl_theme_theme_set(theme_file);

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

    nav_mode=ReadInt("general","nav_mode",0);
    
    extractors= load_extractors();

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

    statefilename=(char *)calloc(strlen(homedir) + 1+21 + 1, sizeof(char));
    strcat(statefilename,homedir);
    strcat(statefilename,"/.madshelf/state.eet");

    refresh_state();

    win = ewl_window_new();
    ewl_window_title_set ( EWL_WINDOW ( win ), "EWL_WINDOW" );
    ewl_window_name_set ( EWL_WINDOW ( win ), "EWL_WINDOW" );
    ewl_window_class_set ( EWL_WINDOW ( win ), "EWLWindow" );
    ewl_object_size_request ( EWL_OBJECT ( win ), 600, 800 );
    ewl_callback_append ( win, EWL_CALLBACK_DELETE_WINDOW, destroy_cb, NULL );
    set_key_handler(win, &main_info);
    ewl_widget_name_set(win,"mainwindow");
    ewl_widget_show ( win );

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

    arrow_widget = ewl_widget_new();
    ewl_container_child_append(EWL_CONTAINER(box4), arrow_widget);
    ewl_widget_name_set(arrow_widget,"arrow_widget");
    ewl_object_alignment_set(EWL_OBJECT(arrow_widget),EWL_FLAG_ALIGN_RIGHT|EWL_FLAG_ALIGN_TOP);
    ewl_theme_data_str_set(EWL_WIDGET(arrow_widget),"/group","ewl/widget/oi_arrows");
    ewl_widget_show(arrow_widget);
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
        if(nav_mode==1)
            ewl_widget_state_set((EWL_MENU_ITEM(temp2)->button).label_object,"select",EWL_STATE_PERSISTENT);
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
            tempstr4=(char *)calloc(strlen(g_languages[i].name)+3+1,sizeof(char));
            if(nav_mode==0)
                sprintf(tempstr4,"%d. %s",i+1, g_languages[i].name);
            else
                sprintf(tempstr4,"%s",g_languages[i].name);
            ewl_button_label_set(EWL_BUTTON(lang_menu_item),tempstr4);
            ewl_container_child_append(EWL_CONTAINER(temp2), lang_menu_item);
            if(nav_mode==1 && i==0)
                ewl_widget_state_set((EWL_MENU_ITEM(lang_menu_item)->button).label_object,"select",EWL_STATE_PERSISTENT);
            sprintf(tempname6,"langmenuitem%d",i+1);
            ewl_widget_name_set(lang_menu_item,tempname6);
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
            if(nav_mode==0)
                sprintf(tempstr4,"%d. %s",i+1, g_roots->roots[i].name);
            else
                sprintf(tempstr4,"%s",g_roots->roots[i].name);
            ewl_button_label_set(EWL_BUTTON(temp3),tempstr4);
            free(tempstr4);
            ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
            if(nav_mode==1 && i==0)
                ewl_widget_state_set((EWL_MENU_ITEM(temp3)->button).label_object,"select",EWL_STATE_PERSISTENT);
            sprintf(tempname6,"gotomenuitem%d",i+1);
            ewl_widget_name_set(EWL_WIDGET(temp3),tempname6);
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
            if(nav_mode==0)
                sprintf(tempstr4,"%d. %s",count+1,scriptstrlist[count]);
            else
                sprintf(tempstr4,"%s",scriptstrlist[count]);
            ewl_button_label_set(EWL_BUTTON(temp3),tempstr4);
            free(tempstr4);
            ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
            if(nav_mode==1 && count==0)
                ewl_widget_state_set((EWL_MENU_ITEM(temp3)->button).label_object,"select",EWL_STATE_PERSISTENT);
            sprintf(tempname6,"scriptsmenuitem%d",count+1);
            ewl_widget_name_set(temp3,tempname6);
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

    for(count=0;count<num_books;count++)
    {
        sprintf(tempname1,"bookbox%d",count);
        box = ewl_hbox_new();
        ewl_container_child_append(EWL_CONTAINER(box3), box);
        sprintf(tempname5,"%d",count+1);
        ewl_theme_data_str_set(EWL_WIDGET(box),"/hbox/group","ewl/box/oi_bookbox");//tempname5);
        if(nav_mode==0)
            ewl_widget_appearance_part_text_set(box,"ewl/box/oi_bookbox/text",tempname5);
        
        ewl_widget_name_set(box,tempname1 );

        sprintf (tempname2, "type%d",count);
        iconimage = ewl_image_new();

        ewl_container_child_append(EWL_CONTAINER(box), iconimage);
        ewl_object_insets_set(EWL_OBJECT ( iconimage ),strtol(edje_file_data_get(theme_file, "/madshelf/icon/inset"),NULL,10),0,0,0);
        ewl_object_padding_set(EWL_OBJECT ( iconimage ),0,0,0,0);
        ewl_widget_name_set(iconimage,tempname2 );
        ewl_object_alignment_set(EWL_OBJECT(iconimage),EWL_FLAG_ALIGN_LEFT|EWL_FLAG_ALIGN_BOTTOM);


        sprintf (tempname3, "labelsbox%d",count);
        box5 = ewl_vbox_new();
        ewl_container_child_append(EWL_CONTAINER(box),box5);
        ewl_widget_name_set(box5,tempname3 );
        ewl_object_fill_policy_set(EWL_OBJECT(box5), EWL_FLAG_FILL_ALL);
        

        sprintf (tempname3, "authorlabel%d",count);
        authorlabel = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box5), authorlabel);
        ewl_widget_name_set(authorlabel,tempname3 );
        ewl_label_text_set(EWL_LABEL(authorlabel), "Unknown Author");
        ewl_object_padding_set(EWL_OBJECT(authorlabel),3,0,0,0);
        ewl_theme_data_str_set(EWL_WIDGET(authorlabel),"/label/textpart","ewl/oi_label/authortext");
        ewl_object_fill_policy_set(EWL_OBJECT(authorlabel), EWL_FLAG_FILL_VSHRINK| EWL_FLAG_FILL_HFILL);

        sprintf (tempname3, "titlelabel%d",count);
        titlelabel = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box5), titlelabel);
        ewl_widget_name_set(titlelabel,tempname3 );
        ewl_label_text_set(EWL_LABEL(titlelabel), "");
        ewl_object_padding_set(EWL_OBJECT(titlelabel),3,0,0,0);
        ewl_theme_data_str_set(EWL_WIDGET(titlelabel),"/label/textpart","ewl/oi_label/titletext");
        ewl_object_fill_policy_set(EWL_OBJECT(titlelabel), EWL_FLAG_FILL_VSHRINK| EWL_FLAG_FILL_HFILL);

        sprintf (tempname3, "infolabel%d",count);
        infolabel = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box5), infolabel);
        ewl_widget_name_set(infolabel,tempname3 );
        ewl_label_text_set(EWL_LABEL(infolabel), "blala");
        ewl_object_padding_set(EWL_OBJECT(infolabel),0,3,0,0);
        ewl_object_alignment_set(EWL_OBJECT(infolabel),EWL_FLAG_ALIGN_RIGHT|EWL_FLAG_ALIGN_BOTTOM);

        ewl_theme_data_str_set(EWL_WIDGET(infolabel),"/label/textpart","ewl/oi_label/infotext");
        ewl_object_fill_policy_set(EWL_OBJECT(infolabel), EWL_FLAG_FILL_VSHRINK| EWL_FLAG_FILL_HFILL);

        sprintf(tempname4,"separator%d",count);
        dividewidget = ewl_hseparator_new();
        ewl_container_child_append(EWL_CONTAINER(box3), dividewidget);

        ewl_widget_name_set(dividewidget,tempname4 );
    }

    free(theme_file);

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
