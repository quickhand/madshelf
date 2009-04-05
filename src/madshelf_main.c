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
#include <libgen.h>
#include "Keyhandler.h"
#include "filefilter.h"
#include "Dialogs.h"
#include "database.h"
#include "tags.h"
#define BUFSIZE 4096

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

typedef struct _mad_file
{
    struct dirent *filestr;
    char *path;
} mad_file;
mad_file** g_fileslist;
//struct dirent** g_fileslist;

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

char* current_dir;

int sort_type=SORT_BY_NAME;
int sort_order=ECORE_SORT_MIN;
long filters_modtime=-1;
int *filterstatus=NULL;
int file_list_mode=FILE_LIST_FOLDER_MODE;
//***********
int num_books=8;

int nav_sel=0;
int nav_menu_sel=0;
int nav_lang_menu_sel=0;
int nav_goto_menu_sel=0;
int nav_scripts_menu_sel=0;
int nav_fileops_menu_sel=0;
int nav_sort_menu_sel=0;
int nav_filemode_menu_sel=0;
int nav_mc_menu_sel=0;
int key_shifted=0;
int context_index=0;
int file_action=FILE_NO_ACTION;
char *action_filename=NULL;
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


void set_g_handler(const char* new_g_handler)
{
    g_handler=new_g_handler;    
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


/* File array convenience functions */
mad_file *get_mad_file(int index)
{
    if(index<0 || index>=g_nfileslist)
        return NULL;
    //return ((mad_file *)(((mad_file *)*g_fileslist) + index));
    return *(mad_file **)(g_fileslist+index);
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
            free(current_dir);
            current_dir = get_current_dir_name();
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

    free(current_dir);
    current_dir = get_current_dir_name();

    if(!is_cwd_in_root(root))
    {
        fprintf(stderr, "%s is illegal in root %d. Falling back.\n",
                file, root);

        if(curfd != -1 && fchdir(curfd) == 0)
        {
            free(current_dir);
            current_dir = get_current_dir_name();
            return -1;
        }

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

/*
 * Copies a file. Returns 0 on success. If anything goes wrong, deletes the
 * target file and returns -1 and error in errno. See the
 * open(2)/write(2)/close(2) manpages for the list of possible errors.
 */
int copy_file(const char* old, const char* new)
{
    int saved_errno;
    int rfd = open(old, O_RDONLY);
    if(rfd == -1)
        return -1;

    struct stat s;
    if(-1 == fstat(rfd, &s))
    {
        saved_errno = errno;
        close(rfd);
        errno = saved_errno;
        return -1;
    }
    
    int wfd = creat(new, s.st_mode);
    if(wfd == -1)
    {
        saved_errno = errno;
        close(rfd);
        errno = saved_errno;
        return -1;
    }

    for(;;)
    {
        char buf[BUFSIZE];
        int count = read(rfd, buf, BUFSIZE);
        if(count == 0)
            break;
        if(count == -1)
        {
            if(errno == EINTR || errno == EAGAIN)
                continue;
            goto err;
        }
        char* pos = buf;
        while(count > 0)
        {
            int written = write(wfd, pos, count);
            if(written == -1)
            {
                if(errno == EINTR || errno == EAGAIN)
                    continue;
                goto err;
            }
            pos += written;
            count -= written;
        }
    }

    if(-1 == close(wfd))
        goto err;
    close(rfd);
    return 0;

err:
    saved_errno = errno;
    unlink(new);
    close(wfd);
    close(rfd);
    errno = saved_errno;
    return -1;
}

/*
 * Moves file, resorting to copying+deleting if source and target are on
 * different filesystems.
 *
 * Returns 0 on success and -1 and error in errno if unable to move file.
 */
int move_file(const char* old, const char* new)
{
    struct stat s_old;
    struct stat s_new;

    if(-1 == stat(old, &s_old))
        return -1;

    char* new_copy = strdup(new);
    char* new_dir = dirname(new_copy);

    if(-1 == stat(new_dir, &s_new))
    {
        int saved_errno = errno;
        free(new_copy);
        errno = saved_errno;
        return -1;
    }

    free(new_copy);

    if(s_old.st_dev == s_new.st_dev)
        return rename(old ,new);

    int res = copy_file(old, new);
    if(res == 0)
        return unlink(old);
    else
        return res;
}

/* Returns the string which need to be free(3) ed*/
char* get_authors_string(char *authors[],int authornum)
{
     
    char* authorstr=NULL;
    
    int length=0;
    int i;
    for(i=0;i<authornum;i++)
    {
        
        length+=strlen(authors[i])+2;
        
    }
    length--;
    
    authorstr=malloc(length*sizeof(char));
    authorstr[0]='\0';
    for(i=0;i<authornum;i++)
    {
        
        strcat(authorstr,authors[i]);
        if(i<(authornum-1))
            strcat(authorstr,", ");
    }
    
    return authorstr;
}

int get_item_labels_array(char ***stringarr)
{
    const char *item_labels_const=ReadString("general","item_labels",NULL);
    char* item_labels=NULL;
    if(item_labels_const)
        item_labels=strdup(item_labels_const);
    char *tok;
    if(item_labels)
        tok = strtok(item_labels, ",");
    else
        return 0;
    int count=0;
    while(tok)
    {
        count++;
        tok=strtok(NULL,",");    
    }
    free(item_labels);
    item_labels=strdup(item_labels_const);
    tok=strtok(item_labels,",");
    char **retarr=malloc(sizeof(char*)*count);
    int i=0;
    while(tok)
    {
        asprintf(&(retarr[i]),"%s",tok);
        i++;
        tok=strtok(NULL,",");
        
    }
    free(item_labels);
    *stringarr=retarr;
    return count;
}
void free_item_labels_array(char **stringarr,int num)
{
    int i;
    for(i=0;i<num;i++)
        free(stringarr[i]);    
    free(stringarr);
    
}
char* get_tag_string(char *tags[],int tagnum)
{
     
    char* tagstr=NULL;
    
    int length=2;
    int i;
    for(i=0;i<tagnum;i++)
    {
        if(is_predef_tag(tags[i]))
            length+=strlen(get_predef_tag_display_name(tags[i]))+2;
        else
            length+=strlen(tags[i])+2;
        
    }
    length--;
    
    tagstr=malloc(length*sizeof(char));
    tagstr[0]='[';
    tagstr[1]='\0';
    for(i=0;i<tagnum;i++)
    {
        if(is_predef_tag(tags[i]))
            strcat(tagstr,get_predef_tag_display_name(tags[i]));
        else
            strcat(tagstr,tags[i]);
        if(i<(tagnum-1))
            strcat(tagstr,", ");
        else
            strcat(tagstr,"]");
    }
    
    return tagstr;
}
void reset_file_position()
{
    current_index=0;
    nav_sel=0;
    
}
void update_list()
{
    int count=0;
    char tempname[20];
    char *pointptr;
    const char *tempstr2;
    Ewl_Widget **labelsbox;
    Ewl_Widget **titlelabel;
    Ewl_Widget **seriesbox;
    Ewl_Widget **serieslabel;
    Ewl_Widget **seriesnumlabel;
    Ewl_Widget **authorlabel;
    Ewl_Widget **infobox;
    Ewl_Widget **infolabel;
    Ewl_Widget **taglabel;
    Ewl_Widget **bookbox;
    Ewl_Widget **separator;
    Ewl_Widget **typeicon;
    Ewl_Widget *arrow_widget;
    Ewl_Widget *statuslabel;
    int *showflag;
    int *seriesshowflag;
    const char *extracted_title = NULL;
    count=0;

    labelsbox=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    titlelabel=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    seriesbox=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    serieslabel=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    seriesnumlabel=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    authorlabel=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    infobox=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    infolabel=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    taglabel=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    bookbox=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    separator=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));
    typeicon=(Ewl_Widget**)alloca(num_books*sizeof(Ewl_Widget*));

    showflag = alloca(num_books * sizeof(int));
    seriesshowflag = alloca(num_books * sizeof(int));

    for(count=0;count<num_books;count++)
    {
        sprintf (tempname, "titlelabel%d",count);
        titlelabel[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "seriesbox%d",count);
        seriesbox[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "serieslabel%d",count);
        serieslabel[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "seriesnumlabel%d",count);
        seriesnumlabel[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "authorlabel%d",count);
        authorlabel[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "infobox%d",count);
        infobox[count] = ewl_widget_name_find(tempname);
        sprintf (tempname, "taglabel%d",count);
        taglabel[count] = ewl_widget_name_find(tempname);
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
    arrow_widget = ewl_widget_name_find("arrow_widget");
    statuslabel=ewl_widget_name_find("statuslabel");

    for(count=0;count<num_books;count++)
    {
        ewl_widget_hide(bookbox[count]);
        ewl_widget_hide(labelsbox[count]);
        ewl_widget_hide(separator[count]);
        ewl_widget_hide(authorlabel[count]);
        ewl_widget_hide(titlelabel[count]);
        ewl_widget_hide(seriesbox[count]);
        ewl_widget_hide(serieslabel[count]);
        ewl_widget_hide(seriesnumlabel[count]);
        ewl_widget_hide(infobox[count]);
        ewl_widget_hide(taglabel[count]);
        ewl_widget_hide(infolabel[count]);
        ewl_widget_hide(typeicon[count]);
    }
    ewl_widget_hide(arrow_widget);
    for(count = 0; count < num_books; count++)
    {
        if(current_index + count >= g_nfileslist)
        {
            showflag[count]=0;
            seriesshowflag[count]=0;
            continue;
        }

        if(get_nav_mode()==1)
        {
            if(count==nav_sel)
                ewl_widget_state_set(bookbox[count],"select",EWL_STATE_PERSISTENT);
            else
                ewl_widget_state_set(bookbox[count],"unselect",EWL_STATE_PERSISTENT);
        }
            
        //char* file = ((struct dirent*) g_fileslist[current_index + count]->filestr)->d_name;
        char* file = get_mad_file(current_index+count)->filestr->d_name;
        char *rel_file;
        
        asprintf(&rel_file, "%s/%s", get_mad_file(current_index+count)->path,file);
        
        
        
        
        struct stat stat_p;
        char* time_str;
        char *extension = strrchr(file, '.');

        stat(rel_file, &stat_p);
        time_str = format_time(stat_p.st_mtime);

        if(ecore_file_is_dir(rel_file))
        {
            ewl_label_text_set(EWL_LABEL(titlelabel[count]),ecore_file_strip_ext(file));

            ewl_label_text_set(EWL_LABEL(infolabel[count]),time_str);
            ewl_label_text_set(EWL_LABEL(authorlabel[count]),"");
            ewl_label_text_set(EWL_LABEL(taglabel[count]),"");
            ewl_image_file_path_set(EWL_IMAGE(typeicon[count]),"/usr/share/madshelf/folder.png");
            seriesshowflag[count]=0;
        }
        else
        {
            char* size_str = format_size(stat_p.st_size);
            char* infostr;
            char* imagefile;
            
            char **titlearr=NULL;
            int titlenum=get_titles(rel_file,&titlearr);


            if(titlenum>0)
                ewl_label_text_set(EWL_LABEL(titlelabel[count]),titlearr[0]);
            else
                ewl_label_text_set(EWL_LABEL(titlelabel[count]),ecore_file_strip_ext(file));
            int i;
            for(i=0;i<titlenum;i++)
                free(titlearr[i]);
            if(titlearr)
                free(titlearr);
            
            char **seriesarr=NULL;
            int *seriesindex=NULL;
            int seriesnum=get_series(rel_file,&seriesarr,&seriesindex);
            

            if(seriesnum>0 && seriesarr && seriesarr[0])
            {
                //char *series_str=get_series_string(seriesarr,seriesindex,seriesnum);
                ewl_label_text_set(EWL_LABEL(serieslabel[count]),seriesarr[0]);//series_str);
                
                if(seriesindex[0]>0)
                {
                    char *tempstr;
                    asprintf(&tempstr,"#%d",seriesindex[0]);
                    ewl_label_text_set(EWL_LABEL(seriesnumlabel[count]),tempstr);
                    free(tempstr);
                }
                else
                    ewl_label_text_set(EWL_LABEL(seriesnumlabel[count]),"");
                seriesshowflag[count]=1;
                //free(series_str);
            }
            else
            {
                ewl_label_text_set(EWL_LABEL(serieslabel[count]),"");
                ewl_label_text_set(EWL_LABEL(seriesnumlabel[count]),"");
                seriesshowflag[count]=0;
            }
            for(i=0;i<seriesnum;i++)
                free(seriesarr[i]);
            if(seriesarr)
                free(seriesarr);
            if(seriesindex)
                free(seriesindex);

            
            extension = strrchr(file, '.');

            asprintf(&infostr, "%s%s%s   %s",
                     extension ? extension : "",
                     extension ? "   " : "",
                     time_str,
                     size_str);

            ewl_label_text_set(EWL_LABEL(infolabel[count]),infostr);
            
            char **authorarr=NULL;
            int numauthors=get_authors(rel_file,&authorarr);
            
            
            if(numauthors>0)
            {
                char* authors = get_authors_string(authorarr,numauthors);
                ewl_label_text_set(EWL_LABEL(authorlabel[count]), authors);
                
                free(authors);
            }
            else
                ewl_label_text_set(EWL_LABEL(authorlabel[count]),"");
            
            for(i=0;i<numauthors;i++)
                free(authorarr[i]);
            
            if(authorarr)
                free(authorarr);
            
            
            
            char **tagarr=NULL;
            int numtags=get_tags(rel_file,&tagarr);
            
            
            if(numtags>0)
            {
                char* tags = get_tag_string(tagarr,numtags);
                ewl_label_text_set(EWL_LABEL(taglabel[count]), tags);
                
                free(tags);
            }
            else
                ewl_label_text_set(EWL_LABEL(taglabel[count]),"");
            
            for(i=0;i<numtags;i++)
                free(tagarr[i]);
            
            if(tagarr)
                free(tagarr);
            
            
            
            
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
        free(rel_file);
        
    }

    if(next_page_exists() && prev_page_exists())
        ewl_widget_state_set(arrow_widget,"both_on",EWL_STATE_PERSISTENT);
    else if(!next_page_exists() && !prev_page_exists())
    	ewl_widget_state_set(arrow_widget,"both_off",EWL_STATE_PERSISTENT);
    else if(next_page_exists())
        ewl_widget_state_set(arrow_widget,"right_only",EWL_STATE_PERSISTENT);
    else if(prev_page_exists())
        ewl_widget_state_set(arrow_widget,"left_only",EWL_STATE_PERSISTENT);

    for(count=0;count<num_books;count++)
    {
        if(showflag[count])
        {
            ewl_widget_show(typeicon[count]);
            ewl_widget_configure(typeicon[count]);
            ewl_widget_show(authorlabel[count]);
            ewl_widget_configure(authorlabel[count]);
            ewl_widget_show(titlelabel[count]);
            ewl_widget_configure(titlelabel[count]);
            if(seriesshowflag[count])
            {
                ewl_widget_show(serieslabel[count]);
                ewl_widget_configure(serieslabel[count]);
                ewl_widget_reveal(serieslabel[count]);
                ewl_widget_show(seriesnumlabel[count]);
                ewl_widget_configure(seriesnumlabel[count]);
                ewl_widget_show(seriesbox[count]);
                ewl_widget_configure(seriesbox[count]);
            }
            ewl_widget_show(taglabel[count]);
            ewl_widget_configure(taglabel[count]);
            ewl_widget_show(infolabel[count]);
            ewl_widget_configure(infolabel[count]);
            ewl_widget_show(infobox[count]);
            ewl_widget_configure(infobox[count]);
            ewl_widget_show(separator[count]);
            ewl_widget_configure(separator[count]);
            ewl_widget_show(labelsbox[count]);
            ewl_widget_configure(labelsbox[count]);
            ewl_widget_show(bookbox[count]);
            ewl_widget_configure(bookbox[count]);
        }
    }
    ewl_widget_show(arrow_widget);
    ewl_widget_configure(arrow_widget);
    //show pages in statuslabel
    char *statstr;
    if(g_nfileslist==0)
        ewl_label_text_set(EWL_LABEL(statuslabel),gettext("no books"));
    else
    {
        asprintf(&statstr,"%d/%d",current_index/num_books+1,(g_nfileslist-(g_nfileslist%num_books))/num_books+((g_nfileslist%num_books)?1:0));
        ewl_label_text_set(EWL_LABEL(statuslabel),statstr);
        free(statstr);
    }
}

void update_title()
{
    char* titletext;
    char* cwd = get_current_dir_name();

    int notroot;
    if(file_list_mode==FILE_LIST_FOLDER_MODE || file_list_mode==FILE_LIST_LOCATION_MODE)
    {
        if(file_list_mode==FILE_LIST_FOLDER_MODE)
            notroot= strcmp(g_roots->roots[current_root].path, cwd);
        else if(file_list_mode==FILE_LIST_LOCATION_MODE)
            notroot=0;
        
        
        asprintf(&titletext, "Madshelf | %s%s%s",
                 g_roots->roots[current_root].name,
                 notroot ? "://" : "",
                 notroot ? cwd + strlen(g_roots->roots[current_root].path) : "");
    }
    else if(file_list_mode==FILE_LIST_ALL_MODE)
    {
        asprintf(&titletext, "Madshelf | %s",gettext("All Locations"));
    }
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
    char *tempstrings[]={gettext("File Filters..."),gettext("Edit"),gettext("Go to"),gettext("File Mode"),gettext("Sorting Options"),gettext("Languages"),gettext("Scripts")};
    char tempname[30];
    char *temptext;
    int i=0;
    int realwidth=0;
    Ewl_Widget *curwidget;
    curwidget = ewl_widget_name_find("okmenu");
    ewl_button_label_set(EWL_BUTTON(curwidget),gettext("Menu"));
    char **item_labels;
    int nitem_labels=get_item_labels_array(&item_labels);
    for(i=0;i<7;i++)
    {
        sprintf(tempname,"menuitem%d",i+1);
        curwidget = ewl_widget_name_find(tempname);
        if(get_nav_mode()==0)
        {
            if(!nitem_labels)
                asprintf(&temptext,"%d. %s",i+1,tempstrings[i]);
            else if(i<nitem_labels)
                asprintf(&temptext,"%s. %s",item_labels[i],tempstrings[i]);
            else
                asprintf(&temptext,"%s",tempstrings[i]);
        }
        else
            asprintf(&temptext,"%s",tempstrings[i]);
        ewl_button_label_set(EWL_BUTTON(curwidget),temptext);
        free(temptext);
    }
    
    
    char *tempstrings2[]={gettext("Paste")};
    for(i=0;i<1;i++)
    {
        sprintf(tempname,"fileopsmenuitem%d",i+1);
        curwidget = ewl_widget_name_find(tempname);
        if(get_nav_mode()==0)
        {
            if(!nitem_labels)
                asprintf(&temptext,"%d. %s",i+1,tempstrings2[i]);
            else if(i<nitem_labels)
                asprintf(&temptext,"%s. %s",item_labels[i],tempstrings2[i]);
            else
                asprintf(&temptext,"%s",tempstrings2[i]);
        }
        else
            asprintf(&temptext,"%s",tempstrings2[i]);
        ewl_button_label_set(EWL_BUTTON(curwidget),temptext);
        free(temptext);
    }
    
    char *tempstrings3[]={gettext("Sort by Name"),gettext("Sort by Time"),gettext("Reverse Sort Order")};
    for(i=0;i<3;i++)
    {
        sprintf(tempname,"sortmenuitem%d",i+1);
        curwidget = ewl_widget_name_find(tempname);
        if(get_nav_mode()==0)
        {
            if(!nitem_labels)
                asprintf(&temptext,"%d. %s",i+1,tempstrings3[i]);
            else if(i<nitem_labels)
                asprintf(&temptext,"%s. %s",item_labels[i],tempstrings3[i]);
            else
                asprintf(&temptext,"%s",tempstrings3[i]);
        }
        else
            asprintf(&temptext,"%s",tempstrings3[i]);
        ewl_button_label_set(EWL_BUTTON(curwidget),temptext);
        free(temptext);
    }
    
    char *tempstrings4[]={gettext("Folder Mode"),gettext("Location Mode"),gettext("All Locations Mode")};
    for(i=0;i<3;i++)
    {
        sprintf(tempname,"filemodemenuitem%d",i+1);
        curwidget = ewl_widget_name_find(tempname);
        if(get_nav_mode()==0)
        {
            if(!nitem_labels)
                asprintf(&temptext,"%d. %s",i+1,tempstrings4[i]);
            else if(i<nitem_labels)
                asprintf(&temptext,"%s. %s",item_labels[i],tempstrings4[i]);
            else
                asprintf(&temptext,"%s",tempstrings4[i]);
        }
        else
            sprintf(&temptext,"%s",tempstrings4[i]);
        ewl_button_label_set(EWL_BUTTON(curwidget),temptext);
        free(temptext);
    }
    free_item_labels_array(item_labels,nitem_labels);
}

void update_context_menu()
{
    char *tempstrings[]={gettext("Cut"),gettext("Copy"),gettext("Delete"),gettext("Tags...")};
    char tempname[30];
    char *temptext;
    char **item_labels;
    int nitem_labels=get_item_labels_array(&item_labels);
    int i=0;
    Ewl_Widget *curwidget;
    curwidget = ewl_widget_name_find("main_context");
    for(i=0;i<4;i++)
    {
        sprintf(tempname,"mc_menuitem%d",i+1);
        curwidget = ewl_widget_name_find(tempname);
        if(get_nav_mode()==0)
        {
            if(!nitem_labels)
                asprintf(&temptext,"%d. %s",i+1,tempstrings[i]);
            else if(i<nitem_labels)
                asprintf(&temptext,"%s. %s",item_labels[i],tempstrings[i]);
            else
                asprintf(&temptext,"%s",tempstrings[i]);
        }
        else
            asprintf(&temptext,"%s",tempstrings[i]);
        ewl_button_label_set(EWL_BUTTON(curwidget),temptext);
        free(temptext);
    }
    free_item_labels_array(item_labels,nitem_labels);
}

/*static char is_dirent_dir(const struct dirent* e)
{
    if(e->d_type != DT_UNKNOWN)
        return e->d_type == DT_DIR;

    struct stat st;
    if(stat(e->d_name, &st))
        return 0;

    return S_ISDIR(st.st_mode);
}*/

static int mad_scandir(const char *dir, mad_file ***namelist, int (*selector) (const struct dirent *), int (*cmp) (const void *, const void *), int dorecurse)
{
    struct dirent** curnamelist = NULL;
    mad_file** mad_namelist;

    int numfiles = scandir(dir, &curnamelist, selector, alphasort);
    if(numfiles == -1)
    {
        *namelist=NULL;
        return -1;
    }

    int total = numfiles;

    mad_namelist = (mad_file **)malloc(sizeof(mad_file*) * numfiles);
    

    int i;
    for(i = 0; i < numfiles; i++)
    {
        mad_namelist[i] = (mad_file *)malloc(sizeof(mad_file));
        mad_namelist[i]->filestr = curnamelist[i];
        mad_namelist[i]->path = strdup(dir);
    }
    free(curnamelist);

    if(dorecurse)
    {
        for(i=0;i<numfiles;i++)
        {
            mad_file *curr_mad_file = mad_namelist[i];
            char *curr_file;
            asprintf(&curr_file,"%s/%s", dir, curr_mad_file->filestr->d_name);
            if(ecore_file_is_dir(curr_file))
            {
                mad_file **subdir_filelist;
                int subdir_files = mad_scandir(curr_file, &subdir_filelist, selector, cmp, 1);
                if(subdir_files == -1)
                    goto err_subdir;

                mad_namelist = (mad_file**)realloc(mad_namelist, sizeof(mad_file*) * (total + subdir_files));
                if(!mad_namelist)
                {
                    perror("realloc");
                    exit(17);
                }

                memcpy(mad_namelist + total, subdir_filelist, sizeof(mad_file*) * subdir_files);

                total += subdir_files;

                free(subdir_filelist);
            }
        err_subdir:
            free(curr_file);
        }
    }

    qsort(mad_namelist, total, sizeof(mad_file*), cmp);
    *namelist = mad_namelist;
    return total;
}

static int dir_alphasort(const void* lhs, const void* rhs)
{
    char *lhs_filename,*rhs_filename;
    asprintf(&(lhs_filename),"%s/%s",(*(mad_file**)lhs)->path,(*(mad_file**)lhs)->filestr->d_name);
    asprintf(&(rhs_filename),"%s/%s",(*(mad_file**)rhs)->path,(*(mad_file**)rhs)->filestr->d_name);
    
    int lhsdir = ecore_file_is_dir(lhs_filename);
    int rhsdir = ecore_file_is_dir(rhs_filename);
        
    free(lhs_filename);
    free(rhs_filename);
    
    if(lhsdir == rhsdir)
    {
        
        return alphasort(&((*(mad_file**)lhs)->filestr),&((*(mad_file**)rhs)->filestr));

    }
    return rhsdir - lhsdir;
}

static int rev_dir_alphasort(const void* lhs, const void* rhs)
{
    char *lhs_filename,*rhs_filename;
    asprintf(&(lhs_filename),"%s/%s",(*(mad_file**)lhs)->path,(*(mad_file**)lhs)->filestr->d_name);
    asprintf(&(rhs_filename),"%s/%s",(*(mad_file**)rhs)->path,(*(mad_file**)rhs)->filestr->d_name);
    
    int lhsdir = ecore_file_is_dir(lhs_filename);
    int rhsdir = ecore_file_is_dir(rhs_filename);

    free(lhs_filename);
    free(rhs_filename);
    
    if(lhsdir == rhsdir)
    {
        
        return alphasort(&((*(mad_file**)rhs)->filestr),&((*(mad_file**)lhs)->filestr));

    }
    return rhsdir - lhsdir;
}

static long long rel_file_mtime(const char* f)
{
    struct stat st;
    stat(f, &st);
    return st.st_mtime;
}

static int date_cmp(const struct dirent** lhs, const struct dirent** rhs)
{
    char *lhs_filename,*rhs_filename;
    asprintf(&(lhs_filename),"%s/%s",(*(mad_file**)lhs)->path,(*(mad_file**)lhs)->filestr->d_name);
    asprintf(&(rhs_filename),"%s/%s",(*(mad_file**)rhs)->path,(*(mad_file**)rhs)->filestr->d_name);
    
    long long lhs_mtime = rel_file_mtime(lhs_filename);
    long long rhs_mtime = rel_file_mtime(rhs_filename);
    
    free(lhs_filename);
    free(rhs_filename);
    
    

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
    {
        free(get_mad_file(i)->filestr);
        free(get_mad_file(i)->path);
        free(get_mad_file(i));
    }
    free(g_fileslist);
}

void init_filelist(int reset)
{
    compar_t cmp;

    fini_filelist();

    if(sort_type == SORT_BY_NAME)
        cmp = (compar_t)(sort_order == ECORE_SORT_MIN ? dir_alphasort : rev_dir_alphasort);
    else
        cmp = (compar_t)(sort_order == ECORE_SORT_MIN ? date_cmp : rev_date_cmp);

    /*g_nfileslist = scandir(".", &g_fileslist,
                           &filter_dotfiles,
                           cmp);*/
    if(file_list_mode==FILE_LIST_FOLDER_MODE)
        g_nfileslist=mad_scandir(current_dir,&g_fileslist,&filter_dotfiles,cmp,0);
    else if(file_list_mode==FILE_LIST_LOCATION_MODE)
        g_nfileslist=mad_scandir(g_roots->roots[current_root].path,&g_fileslist,&filter_dotfiles,cmp,1);
    else if(file_list_mode==FILE_LIST_ALL_MODE)
    {
        g_nfileslist=0;
        int i;
        g_fileslist=NULL;
        for(i=0;i<count_roots();i++)
        {
            mad_file **templist,**templist2;
            int ntemplist=mad_scandir(g_roots->roots[i].path,&templist,&filter_dotfiles,cmp,1);
            if(ntemplist<=0)
                continue;
            templist2=g_fileslist;
            g_fileslist=(mad_file**)malloc((g_nfileslist+ntemplist)*sizeof(mad_file *));
            int j;
            for(j=0;j<g_nfileslist;j++)
                g_fileslist[j]=templist2[j];
            for(j=g_nfileslist;j<(g_nfileslist+ntemplist);j++)
                g_fileslist[j]=templist[j-g_nfileslist];
                
            if(templist2)
                free(templist2);
            free(templist);
            g_nfileslist+=ntemplist;
        }
        qsort ((void *)g_fileslist,g_nfileslist,sizeof(mad_file*),cmp);
        
        
        
        
    }
    
    
    
    
    
    
    
    if(g_nfileslist == -1)
    {
        /* FIXME: handle somehow */

        g_nfileslist = 0;
    }
    if(g_nfileslist>0)
    {
        update_file_database();
        if(file_list_mode==FILE_LIST_FOLDER_MODE)
            filter_filelist(0);
        else if(file_list_mode==FILE_LIST_LOCATION_MODE || file_list_mode==FILE_LIST_ALL_MODE)
            filter_filelist(1);
    }

    
    if(reset)
    {
        current_index = 0;
        nav_sel = 0;
    }
}
void update_file_database()
{
    int i;
    for(i=0;i<g_nfileslist;i++)
    {
        char *rel_file;
        asprintf(&rel_file, "%s/%s",get_mad_file(i)->path,get_mad_file(i)->filestr->d_name);
        if(!ecore_file_is_dir(rel_file))
        {
            
            
            
            int recstatus=get_file_record_status(rel_file);
            if(recstatus==RECORD_STATUS_ERROR || recstatus==RECORD_STATUS_OK || recstatus==RECORD_STATUS_EXISTS_BUT_UNKNOWN) //will have to deal with some of these cases differently later
            {
                
            }
            else if(recstatus==RECORD_STATUS_OUT_OF_DATE)
            {
                clear_file_extractor_data(rel_file);
            
                update_file_mod_time(rel_file);
            
                extract_and_cache(rel_file);
            
            }
            else if(recstatus==RECORD_STATUS_ABSENT)
            {
                if(!extract_and_cache(rel_file))
                    create_empty_record(rel_file);
       
            }
            
        }
        free(rel_file);
       
    }
}
int extract_and_cache(char *filename)
{
    int retval=0;
    EXTRACTOR_KeywordList* mykeys;
    mykeys = extractor_get_keywords(extractors, filename);
    
    
    
    
    //process titles    
    char *extracted_title = extractor_get_last(EXTRACTOR_TITLE, mykeys);
    if(extracted_title && extracted_title[0])
    {
        const char *titlearr[]={extracted_title};
        set_titles(filename,titlearr,1);
        retval=1;
    }
    //process series
    char *extracted_series=extractor_get_last(EXTRACTOR_ALBUM,mykeys);
    char *extracted_seriesnum=extractor_get_last(EXTRACTOR_TRACK_NUMBER,mykeys);
    if(extracted_series && extracted_series[0])
    {
        const char *seriesarr[]={extracted_series};
        int seriesnum=-1;
        if(extracted_seriesnum && extracted_seriesnum[0])
        {
            seriesnum=(int)strtol(extracted_seriesnum,NULL,10);
            if(seriesnum<=0)
                seriesnum=-1;
        }
        
        set_series(filename,seriesarr,&seriesnum,1);
        retval=1;
    }
    //process authors
    EXTRACTOR_KeywordList* keypt=mykeys;
    int authorcount=0;
    while(keypt)
    {
        if(keypt->keywordType == EXTRACTOR_AUTHOR && keypt->keyword && keypt->keyword[0])
            authorcount++;    
        keypt = keypt->next;
    }
    keypt=mykeys;
    char **authorarr=(char**)malloc(sizeof(char*)*authorcount);
    int count=0;
    while(keypt)
    {
        if(keypt->keywordType == EXTRACTOR_AUTHOR && keypt->keyword && keypt->keyword[0])
        {
            authorarr[count]=keypt->keyword;
            count++;
        }
        keypt = keypt->next;
    }
    if(authorcount>0)
    {
        set_authors(filename,authorarr,authorcount);
        retval=1;
    }
    free(authorarr);
    return retval;
    
}
int filter_filelist(int removedirs)
{
    mad_file** new_g_fileslist=(struct dirent**)malloc(sizeof(mad_file*)*g_nfileslist);
    int i,j;
    int count=0;
    int retval=0;
    for(i=0;i<g_nfileslist;i++)
    {
        int flag=1;
        char *rel_file;
        asprintf(&rel_file, "%s/%s",get_mad_file(i)->path,get_mad_file(i)->filestr->d_name);

        if(!ecore_file_is_dir(rel_file))
        {
            for(j=0;j<getNumFilters();j++)
            {
                if(isFilterActive(j))
                {
                    
                    
                    if(!evaluateFilter(j,rel_file))
                    {
                        flag=0;
                        retval=1;
                        break;
                    }

                }
            }
        }
        else if(removedirs)
        {
            flag=0;
            retval=1;
        }
        if(flag)
        {
            new_g_fileslist[count]=g_fileslist[i];
            count++;
        }
        else
        {
            free(get_mad_file(i)->filestr);
            free(get_mad_file(i)->path);
            free(get_mad_file(i));
        }
        free(rel_file);
    }
    g_nfileslist=count;
    free(g_fileslist);
    g_fileslist=new_g_fileslist;    
    return retval;
}
void update_filters()
{
    int i;
    for(i=0;i<getNumFilters();i++)
        filterstatus[i]=isFilterActive(i);
    
}
void destroy_cb ( Ewl_Widget *w, void *event, void *data )
{
    ewl_widget_destroy ( w );
    ewl_main_quit();
}


int sighup_signal_handler(void *data, int type, void *event)
{
    int old_ci=current_index;
    int old_ns=nav_sel;

    /*
     * This chdir/chdir stanza is required to make sure madshelf has freed the
     * handle to the directory of just unmounted filesystem.
     */
    chdir("/");
    if(-1 == chdir_to(current_dir))
    {
        /* Ok, let's try top directory */
        if(-1 == chdir_to(g_roots->roots[current_root].path))
        {
            /* Achtung! */
            emergency_chdir();
        }
    }

    init_filelist(1);
    if(old_ci<g_nfileslist)
        current_index=old_ci;
    if((old_ci+old_ns)<g_nfileslist)
        nav_sel=old_ns;
    update_filelist_in_gui();
    
    return 1;
}


/* Confirm dialog stuff */
#define CONFIRM_DIALOG_NO 0
#define CONFIRM_DIALOG_YES 1
typedef void (*confirm_handler)(void);
confirm_handler confirm_dialog_yes_handler=NULL;
confirm_handler confirm_dialog_no_handler=NULL;
int confirm_dialog_current_choice=0;
void show_confirm_dialog(confirm_handler nohandler,confirm_handler yeshandler,char *message)
{
    Ewl_Widget *wwin=ewl_widget_name_find("mainwindow");
    Ewl_Widget *wdialog=ewl_widget_name_find("confirm_dialog");
    Ewl_Widget *wmessage=ewl_widget_name_find("confirm_dialog_message");
    Ewl_Widget *yeslabel=ewl_widget_name_find("confirm_dialog_yeslabel");
    Ewl_Widget *nolabel=ewl_widget_name_find("confirm_dialog_nolabel");
    char yestext[20],notext[20];
    
    confirm_dialog_no_handler=nohandler;
    confirm_dialog_yes_handler=yeshandler;
    
    ewl_label_text_set(EWL_LABEL(wmessage),message);
    
    if(get_nav_mode()==0)
    {
        sprintf(notext,"1. %s",gettext("No"));
        sprintf(yestext,"2. %s",gettext("Yes"));
        ewl_label_text_set(EWL_LABEL(nolabel),notext);
        ewl_label_text_set(EWL_LABEL(yeslabel),yestext);
    }
    else
    {
        ewl_label_text_set(EWL_LABEL(nolabel),gettext("No"));
        ewl_label_text_set(EWL_LABEL(yeslabel),gettext("Yes"));
    }
    
    
    confirm_dialog_choice_set(CONFIRM_DIALOG_NO);
    
    ewl_widget_show(wdialog);
    ewl_widget_configure(wdialog);
    
}
int confirm_dialog_choice_get(void)
{
    return confirm_dialog_current_choice;
}
void confirm_dialog_choice_set(int choice)
{
    Ewl_Widget *yeslabel=ewl_widget_name_find("confirm_dialog_yeslabel");
    Ewl_Widget *nolabel=ewl_widget_name_find("confirm_dialog_nolabel");
    confirm_dialog_current_choice=choice;
    if(choice==CONFIRM_DIALOG_NO)
    {
        ewl_widget_state_set(nolabel,"select",EWL_STATE_PERSISTENT);
        ewl_widget_state_set(yeslabel,"unselect",EWL_STATE_PERSISTENT);
    }
    else if(choice==CONFIRM_DIALOG_YES)
    {
        ewl_widget_state_set(yeslabel,"select",EWL_STATE_PERSISTENT);
        ewl_widget_state_set(nolabel,"unselect",EWL_STATE_PERSISTENT);
        
    }
}
void confirm_dialog_action_perform(void)
{
    Ewl_Widget *wdialog=ewl_widget_name_find("confirm_dialog");
    if(confirm_dialog_choice_get()==CONFIRM_DIALOG_NO)
    {
        ewl_widget_hide(wdialog);    
        (confirm_dialog_no_handler)();
    }
    else if(confirm_dialog_choice_get()==CONFIRM_DIALOG_YES)
    {
        ewl_widget_hide(wdialog);    
        (confirm_dialog_yes_handler)();
    }
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
    char* file_path_copy = strdup(file_path);
    const char* file_name = basename(file_path_copy);

    const char* extension = strrchr(file_name, '.');
    if (!extension)
    {
        free(file_path_copy);
        return NULL;
    }

    const char* res = ReadString("apps", extension, NULL);
    free(file_path_copy);
    return res;
}

void update_filelist_in_gui()
{
    update_list();
    update_title();
}

void change_dir_in_gui()
{
    init_filelist(1);
    update_filelist_in_gui();
}

void doActionForNum(unsigned int num, unsigned char lp)
{
    char *file;
    int file_index = current_index + num - 1;

    if(file_index >= g_nfileslist)
        return;

    //file = g_fileslist[file_index]->d_name;
    asprintf(&file,"%s/%s",get_mad_file(file_index)->path,get_mad_file(file_index)->filestr->d_name);
    if(!ecore_file_is_dir(file))
    {
        const char* handler = lookup_handler(file);
        if (handler)
        {
            /* Sin */
            g_file = strdup(file);
            if(!strrchr(handler,':'))
            {
                g_handler = strdup(handler);
                ewl_main_quit();
            }
            else if(lp)
                HandlerDialog(handler);
            else
            {
                char *copystring;
                copystring=strdup(handler);
                g_handler = strdup (strtok(copystring, ":"));
                free(copystring);
                ewl_main_quit();
            }
            
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
    free(file);
}

void popupContext(unsigned int num)
{
    char tempname[30];
    Ewl_Widget *curwidget,*selected;
 
    toggle_key_shifted();//=0;
    
    sprintf(tempname,"bookbox%d",num-1);
    selected = ewl_widget_name_find(tempname);
    if(selected==NULL)
        return;
    if(!ewl_widget_onscreen_is(selected))
        return;
    curwidget=ewl_widget_name_find("main_context");
    ewl_popup_follow_set(EWL_POPUP(curwidget),selected);
    
    ewl_popup_mouse_position_set(EWL_POPUP(curwidget),ewl_object_current_x_get(EWL_OBJECT(selected))+ewl_object_current_w_get(EWL_OBJECT(selected))-PREFERRED_W(curwidget),ewl_object_current_y_get(EWL_OBJECT(selected)));
    context_index=num-1;
    
    //Hide or show tags option, based on whether directory.
    Ewl_Widget *tags_item=ewl_widget_name_find("mc_menuitem4");
    char* file;
    asprintf(&file,"%s/%s",get_mad_file(current_index+num-1)->path,get_mad_file(current_index+num-1)->filestr->d_name);
    
    if(ecore_file_is_dir(file))
    {
        ewl_widget_hide(tags_item);    
        
    }
    else
    {
        ewl_widget_show(tags_item);
        ewl_widget_configure(tags_item);
    }
    
    
    ewl_widget_show(curwidget);
    ewl_widget_configure(curwidget);
    ewl_window_raise(EWL_WINDOW(curwidget));
    ewl_widget_focus_send(curwidget);
    free(file);
}
void toggle_key_shifted()
{
    Ewl_Widget *curwidget;
    key_shifted=!key_shifted;
    curwidget = ewl_widget_name_find("keystatelabel");
    if(key_shifted)
        ewl_label_text_set(EWL_LABEL(curwidget),"↑");
    else
        ewl_label_text_set(EWL_LABEL(curwidget),"");
    
    
}
void change_root(int item)
{
    if(chdir_to_in_root(g_roots->roots[item].path, item) == 0)
        current_root = item;
}

/* GUI */

/* Main key handler */

void main_back(Ewl_Widget *widget, unsigned char lp)
{
    if(file_list_mode!=FILE_LIST_FOLDER_MODE)
        return;
    int i;
    char* cwd = get_current_dir_name();
    char* cur_name = basename(cwd);

    chdir_to("..");
    init_filelist(1);

    for(i = 0; i < g_nfileslist; i++)
        if(!strcmp(cur_name,get_mad_file(i)->filestr->d_name))
        {
            nav_sel = i%num_books;
            current_index = i-nav_sel;
            break;
        }

    free(cwd);

    update_filelist_in_gui();
}

void main_menu(Ewl_Widget *widget, unsigned char lp)
{
    show_main_menu();
}

void main_mod(Ewl_Widget *widget, unsigned char lp)
{
    toggle_key_shifted();
}

void main_nav_up(Ewl_Widget *widget, unsigned char lp)
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

void main_nav_down(Ewl_Widget *widget, unsigned char lp)
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

void main_nav_left(Ewl_Widget *widget, unsigned char lp)
{
    nav_sel=0;
    prev_page();
}

void main_nav_right(Ewl_Widget *widget, unsigned char lp)
{
    nav_sel=0;
    next_page();
}

void main_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    if(key_shifted)
        popupContext(nav_sel+1);
    else
        doActionForNum(nav_sel+1,lp);
    
}
/*void main_nav_menubtn(Ewl_Widget *widget, unsigned char lp)
{
    
    show_main_menu();
    
}*/
void main_next(Ewl_Widget *widget, unsigned char lp)
{
    next_page();
}
void main_previous(Ewl_Widget *widget, unsigned char lp)
{
    prev_page();
}
void main_item(Ewl_Widget *widget,int item, unsigned char lp)
{
    if(item<1 || item>num_books)
        return;
    
    else if(key_shifted)
        popupContext(item);
    else
        doActionForNum(item,lp);
    
}


static key_handler_info_t main_info =
{
    .menu_handler = &main_menu,
    .back_handler = &main_back,
    .mod_handler = &main_mod,
    .nav_up_handler=&main_nav_up,
    .nav_down_handler=&main_nav_down,
    .nav_left_handler=&main_nav_left,
    .nav_right_handler=&main_nav_right,
    .nav_sel_handler=&main_nav_sel,
    .next_handler=&main_next,
    .previous_handler=&main_previous,
    .item_handler = &main_item,
};

/* Main menu */

void show_main_menu()
{
    Ewl_Widget *curwidget=ewl_widget_name_find("okmenu");
    ewl_menu_cb_expand(curwidget,NULL,NULL);
    ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
}

void hide_main_menu()
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("okmenu")));
}

void main_menu_nav_up(Ewl_Widget *widget, unsigned char lp)
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

void main_menu_nav_down(Ewl_Widget *widget, unsigned char lp)
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

void main_menu_back(Ewl_Widget *widget, unsigned char lp)
{
    hide_main_menu();
}

void main_menu_item(Ewl_Widget *widget,int item, unsigned char lp)
{
    Ewl_Widget* curwidget;

    switch(item)
    {
    case 1:
        hide_main_menu();
        FiltersDialog();
        break;

    case 2:
        curwidget = ewl_widget_name_find("menuitem2");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
        break;
    case 3:
        curwidget = ewl_widget_name_find("menuitem3");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
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
    case 7:
        curwidget = ewl_widget_name_find("menuitem7");
        ewl_menu_cb_expand(curwidget,NULL,NULL);
        ewl_widget_focus_send(EWL_WIDGET(EWL_MENU(curwidget)->popup));
        break;
    }
}

void main_menu_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    main_menu_item(widget,nav_menu_sel+1,lp);
}

static key_handler_info_t main_menu_info =
{
    .menu_handler = &main_menu_back,
    .back_handler = &main_menu_back,
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
    { "Беларуская", "be" },
    { "Español", "es" },
    { "Polish", "pl" },
    { "Tiếng Việt", "vi"},

/* Temporarily disabled until some Chinese font is added in
    { "简体中文", "zh_CN" }
 */
};

static const int g_nlanguages = sizeof(g_languages)/sizeof(language_t);

void lang_menu_back(Ewl_Widget *widget, unsigned char lp)
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem6")));
    hide_main_menu();
}
void lang_menu_nav_up(Ewl_Widget *widget, unsigned char lp)
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

void lang_menu_nav_down(Ewl_Widget *widget, unsigned char lp)
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

void lang_menu_item(Ewl_Widget *widget,int item, unsigned char lp)
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

    curwidget = ewl_widget_name_find("menuitem6");
    ewl_menu_collapse(EWL_MENU(curwidget));
    hide_main_menu();
    update_title();
    update_sort_label();
    update_menu();
}

void lang_menu_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    lang_menu_item(widget,nav_lang_menu_sel+1,lp);
}

static key_handler_info_t lang_menu_info =
{
    .menu_handler = &lang_menu_back,
    .back_handler = &lang_menu_back,
    .nav_up_handler=&lang_menu_nav_up,
    .nav_down_handler=&lang_menu_nav_down,
    .nav_sel_handler=&lang_menu_nav_sel,
    .item_handler = &lang_menu_item,
};

/* "Go to" menu */

void goto_menu_back(Ewl_Widget *widget, unsigned char lp)
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem3")));
    hide_main_menu();
}

void goto_menu_nav_up(Ewl_Widget *widget, unsigned char lp)
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

void goto_menu_nav_down(Ewl_Widget *widget, unsigned char lp)
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


void goto_menu_item(Ewl_Widget *widget,int item, unsigned char lp)
{
    if(item == 0)
        item = 10;

    item--;

    if(item < 0 || item >= g_roots->nroots)
        return;

    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem3")));
    hide_main_menu();

    change_root(item);
    change_dir_in_gui();
}

void goto_menu_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    goto_menu_item(widget,nav_goto_menu_sel+1,lp);
}

static key_handler_info_t goto_menu_info =
{
    .menu_handler = &goto_menu_back,
    .back_handler = &goto_menu_back,
    .nav_up_handler=&goto_menu_nav_up,
    .nav_down_handler=&goto_menu_nav_down,
    .nav_sel_handler=&goto_menu_nav_sel,
    .item_handler = &goto_menu_item,
};

/* "Scripts" menu */

void scripts_menu_back(Ewl_Widget *widget, unsigned char lp)
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem7")));
    hide_main_menu();
}

void scripts_menu_nav_up(Ewl_Widget *widget, unsigned char lp)
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

void scripts_menu_nav_down(Ewl_Widget *widget, unsigned char lp)
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


void scripts_menu_item(Ewl_Widget *widget,int item, unsigned char lp)
{
    const char* tempstr;
    char* handler_path;

    item--;

    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem7")));
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

void scripts_menu_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    scripts_menu_item(widget,nav_scripts_menu_sel+1,lp);
}

static key_handler_info_t scripts_menu_info =
{
    .menu_handler = &scripts_menu_back,
    .back_handler = &scripts_menu_back,
    .nav_up_handler=&scripts_menu_nav_up,
    .nav_down_handler=&scripts_menu_nav_down,
    .nav_sel_handler=&scripts_menu_nav_sel,
    .item_handler = &scripts_menu_item,
};

/* Main Context menu */

void mc_menu_back(Ewl_Widget *widget, unsigned char lp)
{
    ewl_widget_hide(ewl_widget_name_find("main_context"));
}

void mc_menu_nav_up(Ewl_Widget *widget, unsigned char lp)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    if((nav_mc_menu_sel-1)>=0)
    {
        
        sprintf (tempname, "mc_menuitem%d",nav_mc_menu_sel+1);
        oldselwid = ewl_widget_name_find(tempname);
        sprintf (tempname, "mc_menuitem%d",nav_mc_menu_sel);
        newselwid = ewl_widget_name_find(tempname);
        if(!oldselwid||!newselwid)
            return;
        ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
        nav_mc_menu_sel--;
        ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
    }       
}

void mc_menu_nav_down(Ewl_Widget *widget, unsigned char lp)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    sprintf (tempname, "mc_menuitem%d",nav_mc_menu_sel+1);
    oldselwid = ewl_widget_name_find(tempname);
    sprintf (tempname, "mc_menuitem%d",nav_mc_menu_sel+2);
    newselwid = ewl_widget_name_find(tempname);
    if(!oldselwid||!newselwid)
        return;
    ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
    nav_mc_menu_sel++;
    ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
}

void mc_menu_delete_confirm_yes(void)
{
    if(action_filename)
    {
        unlink (action_filename);
        free(action_filename);
        action_filename=NULL;
        init_filelist(1);
        update_filelist_in_gui();
    }
    
}
void mc_menu_delete_confirm_no(void)
{
    if(action_filename)
    { 
        free(action_filename);
        action_filename=NULL;
    }
        
        
}

void mc_menu_item(Ewl_Widget *widget,int item, unsigned char lp)
{
    Ewl_Widget *curwidget;
    if(item <= 0 || item>4)
        return;

    action_filename=(char *)malloc((strlen(get_mad_file(current_index+context_index)->filestr->d_name)+2+strlen(get_mad_file(current_index+context_index)->path))*sizeof(char));
    sprintf(action_filename,"%s/%s",get_mad_file(current_index+context_index)->path,get_mad_file(current_index+context_index)->filestr->d_name);

    ewl_widget_hide(ewl_widget_name_find("main_context"));
    if(item==1)
        file_action=FILE_CUT;
    else if(item==2)
        file_action=FILE_COPY;
    else if(item==3)
    {
        
        show_confirm_dialog(mc_menu_delete_confirm_no,mc_menu_delete_confirm_yes,gettext("Delete file?"));
        file_action=FILE_NO_ACTION;
        
    }
    else if(item==4)
    {
        TagsDialog(action_filename);
        free(action_filename);
        action_filename=NULL;
    }
    
}

void mc_menu_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    mc_menu_item(widget,nav_mc_menu_sel+1,lp);
}




static key_handler_info_t mc_menu_info =
{
    .menu_handler = &mc_menu_back,
    .back_handler = &mc_menu_back,
    .nav_up_handler=&mc_menu_nav_up,
    .nav_down_handler=&mc_menu_nav_down,
    .nav_sel_handler=&mc_menu_nav_sel,
    .item_handler = &mc_menu_item,
};

/* FileOps menu */
void fileops_menu_back(Ewl_Widget *widget, unsigned char lp)
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem2")));
    hide_main_menu();
}

void fileops_menu_nav_up(Ewl_Widget *widget, unsigned char lp)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    if((nav_fileops_menu_sel-1)>=0)
    {
        
        sprintf (tempname, "fileopsmenuitem%d",nav_fileops_menu_sel+1);
        oldselwid = ewl_widget_name_find(tempname);
        sprintf (tempname, "fileopsmenuitem%d",nav_fileops_menu_sel);
        newselwid = ewl_widget_name_find(tempname);
        if(!oldselwid||!newselwid)
            return;
        ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
        nav_fileops_menu_sel--;
        ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
    }       
}

void fileops_menu_nav_down(Ewl_Widget *widget, unsigned char lp)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    sprintf (tempname, "fileopsmenuitem%d",nav_fileops_menu_sel+1);
    oldselwid = ewl_widget_name_find(tempname);
    sprintf (tempname, "fileopsmenuitem%d",nav_fileops_menu_sel+2);
    newselwid = ewl_widget_name_find(tempname);
    if(!oldselwid||!newselwid)
        return;
    ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
    nav_fileops_menu_sel++;
    ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
}


void fileops_menu_item(Ewl_Widget *widget,int item, unsigned char lp)
{
    if(item < 0 || item >1)
        return;

    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem2")));
    hide_main_menu();
    
    if(item==1)
    {
        if(file_action==FILE_NO_ACTION)
            return;
        char* filename_copy = strdup(action_filename);
        char* target_basename = basename(filename_copy);
        char* cwd = get_current_dir_name();

        char* target_filename;
        asprintf(&target_filename, "%s/%s", cwd, target_basename);

        free(filename_copy);
        free(cwd);
        
        if(file_action==FILE_CUT)
            move_file(action_filename,target_filename);            
        else if(file_action==FILE_COPY)
            copy_file(action_filename,target_filename);
        
        file_action=FILE_NO_ACTION;
        free(target_filename);
        free(action_filename);
        action_filename=NULL;
        init_filelist(1);
        update_filelist_in_gui();
    }
}

void fileops_menu_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    fileops_menu_item(widget,nav_fileops_menu_sel+1,lp);
}

static key_handler_info_t fileops_menu_info =
{
    .menu_handler = &fileops_menu_back,
    .back_handler = &fileops_menu_back,
    .nav_up_handler=&fileops_menu_nav_up,
    .nav_down_handler=&fileops_menu_nav_down,
    .nav_sel_handler=&fileops_menu_nav_sel,
    .item_handler = &fileops_menu_item,
};

/* Sort menu */
void sort_menu_back(Ewl_Widget *widget, unsigned char lp)
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem5")));
    hide_main_menu();
}

void sort_menu_nav_up(Ewl_Widget *widget, unsigned char lp)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    if((nav_sort_menu_sel-1)>=0)
    {
        
        sprintf (tempname, "sortmenuitem%d",nav_sort_menu_sel+1);
        oldselwid = ewl_widget_name_find(tempname);
        sprintf (tempname, "sortmenuitem%d",nav_sort_menu_sel);
        newselwid = ewl_widget_name_find(tempname);
        if(!oldselwid||!newselwid)
            return;
        ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
        nav_sort_menu_sel--;
        ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
    }       
}

void sort_menu_nav_down(Ewl_Widget *widget, unsigned char lp)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    sprintf (tempname, "sortmenuitem%d",nav_sort_menu_sel+1);
    oldselwid = ewl_widget_name_find(tempname);
    sprintf (tempname, "sortmenuitem%d",nav_sort_menu_sel+2);
    newselwid = ewl_widget_name_find(tempname);
    if(!oldselwid||!newselwid)
        return;
    ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
    nav_sort_menu_sel++;
    ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
}


void sort_menu_item(Ewl_Widget *widget,int item, unsigned char lp)
{
    if(item < 0 || item >3)
        return;

    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem5")));
    hide_main_menu();
    
    if(item==1)
    {
    
        sort_order=ECORE_SORT_MIN;
        sort_type=SORT_BY_NAME;

        init_filelist(1);

        update_list();
        update_sort_label();
        
    }
    else if(item==2)
    {
        sort_order=ECORE_SORT_MIN;
        sort_type=SORT_BY_TIME;

        init_filelist(1);

        update_list();
        update_sort_label();
        
     }
     else if(item==3)
     {
        if(sort_order==ECORE_SORT_MIN)
            sort_order=ECORE_SORT_MAX;
        else
            sort_order=ECORE_SORT_MIN;

        init_filelist(1);

        update_list();
        update_sort_label();
        
     }
}

void sort_menu_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    sort_menu_item(widget,nav_sort_menu_sel+1,lp);
}

static key_handler_info_t sort_menu_info =
{
    .menu_handler = &sort_menu_back,
    .back_handler = &sort_menu_back,
    .nav_up_handler=&sort_menu_nav_up,
    .nav_down_handler=&sort_menu_nav_down,
    .nav_sel_handler=&sort_menu_nav_sel,
    .item_handler = &sort_menu_item,
};

/* File Mode menu */
void filemode_menu_back(Ewl_Widget *widget, unsigned char lp)
{
    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem4")));
    hide_main_menu();
}

void filemode_menu_nav_up(Ewl_Widget *widget, unsigned char lp)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    if((nav_filemode_menu_sel-1)>=0)
    {
        
        sprintf (tempname, "filemodemenuitem%d",nav_filemode_menu_sel+1);
        oldselwid = ewl_widget_name_find(tempname);
        sprintf (tempname, "filemodemenuitem%d",nav_filemode_menu_sel);
        newselwid = ewl_widget_name_find(tempname);
        if(!oldselwid||!newselwid)
            return;
        ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
        nav_filemode_menu_sel--;
        ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
    }       
}

void filemode_menu_nav_down(Ewl_Widget *widget, unsigned char lp)
{
    char tempname[30];
    Ewl_Widget *oldselwid=NULL;
    Ewl_Widget *newselwid=NULL;
    sprintf (tempname, "filemodemenuitem%d",nav_filemode_menu_sel+1);
    oldselwid = ewl_widget_name_find(tempname);
    sprintf (tempname, "filemodemenuitem%d",nav_filemode_menu_sel+2);
    newselwid = ewl_widget_name_find(tempname);
    if(!oldselwid||!newselwid)
        return;
    ewl_widget_state_set((EWL_MENU_ITEM(oldselwid)->button).label_object,"unselect",EWL_STATE_PERSISTENT);
    nav_filemode_menu_sel++;
    ewl_widget_state_set((EWL_MENU_ITEM(newselwid)->button).label_object,"select",EWL_STATE_PERSISTENT);
}


void filemode_menu_item(Ewl_Widget *widget,int item, unsigned char lp)
{
    if(item < 0 || item >3)
        return;

    ewl_menu_collapse(EWL_MENU(ewl_widget_name_find("menuitem4")));
    hide_main_menu();
    
    if(item==1)
    {
        
        file_list_mode=FILE_LIST_FOLDER_MODE;
        init_filelist(1);

        update_list();
        update_title();
        
    }
    else if(item==2)
    {
        file_list_mode=FILE_LIST_LOCATION_MODE;
        
        init_filelist(1);

        update_list();
        update_title();
        
     }
     else if(item==3)
     {
        file_list_mode=FILE_LIST_ALL_MODE;
        
        init_filelist(1);

        update_list();
        update_title();
        
     }
     
}

void filemode_menu_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    sort_menu_item(widget,nav_filemode_menu_sel+1,lp);
}

static key_handler_info_t filemode_menu_info =
{
    .menu_handler = &filemode_menu_back,
    .back_handler = &filemode_menu_back,
    .nav_up_handler=&filemode_menu_nav_up,
    .nav_down_handler=&filemode_menu_nav_down,
    .nav_sel_handler=&filemode_menu_nav_sel,
    .item_handler = &filemode_menu_item,
};
/* Confirm dialog key handlers */

void confirm_dialog_nav_sel(Ewl_Widget *widget, unsigned char lp)
{
    confirm_dialog_action_perform();
}

void confirm_dialog_nav_right(Ewl_Widget *widget, unsigned char lp)
{
    if(confirm_dialog_choice_get()!=CONFIRM_DIALOG_YES)
        confirm_dialog_choice_set(CONFIRM_DIALOG_YES);
}

void confirm_dialog_nav_left(Ewl_Widget *widget, unsigned char lp)
{
    if(confirm_dialog_choice_get()!=CONFIRM_DIALOG_NO)
        confirm_dialog_choice_set(CONFIRM_DIALOG_NO);
}

void confirm_dialog_back(Ewl_Widget *widget, unsigned char lp)
{
    if(confirm_dialog_choice_get()!=CONFIRM_DIALOG_NO)
        confirm_dialog_choice_set(CONFIRM_DIALOG_NO);
    confirm_dialog_action_perform();
}

void confirm_dialog_item(Ewl_Widget *widget,int item, unsigned char lp)
{
    if(item <1 || item >2)
        return;
    
    if(item==1)
    {
        if(confirm_dialog_choice_get()!=CONFIRM_DIALOG_NO)
            confirm_dialog_choice_set(CONFIRM_DIALOG_NO);
        confirm_dialog_action_perform();
    }
    else if(item==2)
    {
        if(confirm_dialog_choice_get()!=CONFIRM_DIALOG_YES)
            confirm_dialog_choice_set(CONFIRM_DIALOG_YES);
        confirm_dialog_action_perform();
    }
}

static key_handler_info_t confirm_dialog_info =
{
    .menu_handler = &confirm_dialog_back,
    .back_handler = &confirm_dialog_back,
    .nav_left_handler=&confirm_dialog_nav_left,
    .nav_right_handler=&confirm_dialog_nav_right,
    .nav_sel_handler=&confirm_dialog_nav_sel,
    .item_handler = &confirm_dialog_item,
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
    eet_write(state,"file_list_mode",(void *)&file_list_mode,sizeof(int),0);
    eet_write(state,"filters_modtime",(void *)&filters_modtime,sizeof(long),0);
    eet_write(state,"filterstatus",(void *)filterstatus,sizeof(int)*getNumFilters(),0);
    
    eet_close(state);

    free(cwd);
}

void refresh_state()
{
    change_root(0);

    int size;
    Eet_File* state = eet_open(statefilename, EET_FILE_MODE_READ);
    if(!state || !eet_read(state, "statesaved", &size))
    {
        eet_close(state);
        //init_filelist(1);
        return;
    }

    char* temp = (char*)eet_read(state, "rootname", &size);
    int i;
    for(i = 1; i < g_roots->nroots; ++i)
        if (!strcmp(temp, g_roots->roots[i].name))
    {
        change_root(i);
        break;
    }

    chdir_to((char*)eet_read(state, "curdir", &size));
    //init_filelist(1);
    int *temppt=(int*)eet_read(state,"curindex", &size);
 
    if(temppt)
        current_index = *temppt;
    else
        current_index = 0;
    if(current_index < 0)// || current_index > g_nfileslist)
        current_index = 0;
    
    temppt=(int*)eet_read(state, "sort_type", &size);
    if(temppt)
        sort_type=*temppt;
    else
        sort_type=SORT_BY_NAME;
    temppt=(int*)eet_read(state, "sort_order", &size);
    if(temppt)
        sort_order=*temppt;
    else
        sort_order=ECORE_SORT_MIN;
    
    temppt=(int*)eet_read(state, "file_list_mode", &size);
    if(temppt)
        file_list_mode=*temppt;
    else
        file_list_mode=FILE_LIST_FOLDER_MODE;
    long *temppt2;
    temppt2=(long*)eet_read(state, "filters_modtime", &size);
    if(temppt2)
        filters_modtime=*temppt2;
    else
        filters_modtime=0;
    
    filterstatus=(int *)eet_read(state, "filterstatus", &size);
    eet_close(state);
}

int valid_dir(const char* dir)
{
    struct stat st;

    int res = stat(dir, &st);
    return res == 0 && S_ISDIR(st.st_mode) && ((S_IRUSR | S_IXUSR) & st.st_mode);
}

static void idialog_reveal(Ewl_Widget *w, void *ev, void *data) {
	Ewl_Widget *win;
	win = ewl_widget_name_find("mainwindow");
	ewl_window_move(EWL_WINDOW(w), CURRENT_X(win) + (CURRENT_W(win) - CURRENT_W(w)) / 2, CURRENT_Y(win) + (CURRENT_H(win) - CURRENT_H(w)) / 2);
	ewl_window_keyboard_grab_set(EWL_WINDOW(w), 1);
}

static void idialog_unrealize(Ewl_Widget *w, void *ev, void *data) {
	Ewl_Widget *win;
	win = ewl_widget_name_find("mainwindow");
	if(win)
		ewl_window_keyboard_grab_set(EWL_WINDOW(win), 1);
}

int main ( int argc, char ** argv )
{
    int file_desc;
    char tempname1[20];
    char tempname2[20];
    char tempname3[20];
    char tempname4[20];
    char *tempname5;
    char tempname6[20];
    Ewl_Widget *win = NULL;
    Ewl_Widget *border = NULL;

    Ewl_Widget *box = NULL;
    Ewl_Widget *box2=NULL;
    Ewl_Widget *box3=NULL;
    Ewl_Widget *box5=NULL;
    Ewl_Widget *box6=NULL;
    Ewl_Widget *box7=NULL;
    Ewl_Widget *authorlabel;
    Ewl_Widget *titlelabel;
    Ewl_Widget *serieslabel;
    Ewl_Widget *seriesnumlabel;
    Ewl_Widget *infolabel;
    Ewl_Widget *taglabel;
    Ewl_Widget *iconimage;
    Ewl_Widget *menubar=NULL;
    Ewl_Widget *arrow_widget=NULL;
    Ewl_Widget *sorttypetext;
    Ewl_Widget *dividewidget;
    Ewl_Widget *statuslabel;
    Ewl_Widget *keystatelabel;
    char *homedir;
    char *configfile;
    char *filterfile;
    char *dbfile;
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

    
    
    
    //end database testing
    homedir=getenv("HOME");
    
    
    filterfile=(char *)calloc(strlen(homedir) + 23, sizeof(char));
    strcat(filterfile,homedir);
    strcat(filterfile,"/.madshelf/");
    if(!ecore_file_path_dir_exists(filterfile))
    {
        ecore_file_mkpath(filterfile);
    }
    strcat(filterfile,"filters.xml");

    struct stat filterstat;
    int filtexists;
    filtexists = stat(filterfile, &filterstat);
    if(filtexists>=0)
    {
        load_filters(filterfile);
    }
    
    free(filterfile);
    
    
    
    
   
    
    dbfile=(char *)calloc(strlen(homedir) + 23, sizeof(char));
    strcat(dbfile,homedir);
    strcat(dbfile,"/.madshelf/");
    strcat(dbfile,"madshelf.db");
    init_database(dbfile);
    free(dbfile);
    
    
    configfile=(char *)calloc(strlen(homedir) + 1+18 + 1, sizeof(char));
    strcat(configfile,homedir);
    strcat(configfile,"/.madshelf/");
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
    
    set_nav_mode(ReadInt("general","nav_mode",0));
    num_books=ReadInt("general","num_books",8);
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

    if(filtexists>=0)
    {
        if(filters_modtime!=filterstat.st_mtime)
        {
            if(filterstatus!=NULL)
                free(filterstatus);
            filterstatus=(int *)malloc(sizeof(int)*getNumFilters());
            int i;
            for(i=0;i<getNumFilters();i++)
                filterstatus[i]=0;
            current_index=0;
            nav_sel=0;
        }
        filters_modtime=filterstat.st_mtime;
        
        
        
    }
    int i;
    for(i=0;i<getNumFilters();i++)
        setFilterActive(i,filterstatus[i]);
    
    
    init_filelist(0);
    
    
    win = ewl_window_new();
    ewl_window_title_set ( EWL_WINDOW ( win ), "EWL_WINDOW" );
    ewl_window_name_set ( EWL_WINDOW ( win ), "EWL_WINDOW" );
    ewl_window_class_set ( EWL_WINDOW ( win ), "EWLWindow" );
    ewl_object_size_request ( EWL_OBJECT ( win ), 600, 800 );
    ewl_object_maximum_w_set(EWL_OBJECT(win),600);
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
    //ewl_object_maximum_w_set(EWL_OBJECT(EWL_BORDER(border)->label),500);
    ewl_object_fill_policy_set(EWL_OBJECT(EWL_BORDER(border)->label), EWL_FLAG_FILL_HSHRINK);//EWL_FLAG_FILL_VSHRINK|EWL_FLAG_FILL_HFILL);
    ewl_widget_show(border);

    update_title();

    box3 = ewl_vbox_new();
    ewl_container_child_append(EWL_CONTAINER(border),box3);
    ewl_object_fill_policy_set(EWL_OBJECT(box3), EWL_FLAG_FILL_FILL);//EWL_FLAG_FILL_VSHRINK|EWL_FLAG_FILL_HFILL);
    ewl_widget_show(box3);

    char **item_labels;
    int nitem_labels=get_item_labels_array(&item_labels);
    
    menubar=ewl_hmenubar_new();

    {
        int i;
        Ewl_Widget *temp=NULL;
        Ewl_Widget *temp2=NULL;
        Ewl_Widget *temp3=NULL;
        temp=ewl_menu_new();

        ewl_container_child_append(EWL_CONTAINER(menubar),temp);
        ewl_widget_name_set(temp,"okmenu");
        ewl_object_fill_policy_set(EWL_OBJECT(temp),EWL_FLAG_FILL_HSHRINKABLE);
        set_key_handler(EWL_MENU(temp)->popup, &main_menu_info);

        ewl_widget_show(temp);


        temp2=ewl_menu_item_new();
        ewl_widget_name_set(temp2,"menuitem1");

        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        if(get_nav_mode()==1)
            ewl_widget_state_set((EWL_MENU_ITEM(temp2)->button).label_object,"select",EWL_STATE_PERSISTENT);
        ewl_widget_show(temp2);


        temp2=ewl_menu_new();
        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        ewl_widget_name_set(temp2,"menuitem2");
        set_key_handler(EWL_MENU(temp2)->popup, &fileops_menu_info);
        ewl_widget_show(temp2);

        temp3=ewl_menu_item_new();
        ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
        if(get_nav_mode()==1)
            ewl_widget_state_set((EWL_MENU_ITEM(temp3)->button).label_object,"select",EWL_STATE_PERSISTENT);
        ewl_widget_name_set(temp3,"fileopsmenuitem1");
        ewl_widget_show(temp3);

        temp2=ewl_menu_new();
        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        ewl_widget_name_set(temp2,"menuitem3");

        set_key_handler(EWL_MENU(temp2)->popup, &goto_menu_info);
        ewl_widget_show(temp2);

        for(i = 0; i < MIN(g_roots->nroots, 8); ++i)
        {
            temp3=ewl_menu_item_new();
            //tempstr4=(char *)calloc(strlen(g_roots->roots[i].name)+3+1,sizeof(char));
            if(get_nav_mode()==0)
            {
                if(!nitem_labels)    
                    asprintf(&tempstr4,"%d. %s",i+1, g_roots->roots[i].name);
                else if(i<nitem_labels)
                    asprintf(&tempstr4,"%s. %s",item_labels[i], g_roots->roots[i].name);
                else
                    asprintf(&tempstr4,"%s",g_roots->roots[i].name);
            }
            else
                asprintf(&tempstr4,"%s",g_roots->roots[i].name);
            ewl_button_label_set(EWL_BUTTON(temp3),tempstr4);
            free(tempstr4);
            ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
            if(get_nav_mode()==1 && i==0)
                ewl_widget_state_set((EWL_MENU_ITEM(temp3)->button).label_object,"select",EWL_STATE_PERSISTENT);
            sprintf(tempname6,"gotomenuitem%d",i+1);
            ewl_widget_name_set(EWL_WIDGET(temp3),tempname6);
            ewl_widget_show(temp3);
            count++;
        }

        temp2=ewl_menu_new();
        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        ewl_widget_name_set(temp2,"menuitem4");
        set_key_handler(EWL_MENU(temp2)->popup, &filemode_menu_info);
        ewl_widget_show(temp2);

        temp3=ewl_menu_item_new();
        ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
        if(get_nav_mode()==1)
            ewl_widget_state_set((EWL_MENU_ITEM(temp3)->button).label_object,"select",EWL_STATE_PERSISTENT);
        ewl_widget_name_set(temp3,"filemodemenuitem1");
        ewl_widget_show(temp3);
        
        temp3=ewl_menu_item_new();
        ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
        ewl_widget_name_set(temp3,"filemodemenuitem2");
        ewl_widget_show(temp3);
        
        temp3=ewl_menu_item_new();
        ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
        ewl_widget_name_set(temp3,"filemodemenuitem3");
        ewl_widget_show(temp3);
        

        temp2=ewl_menu_new();
        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        ewl_widget_name_set(temp2,"menuitem5");
        set_key_handler(EWL_MENU(temp2)->popup, &sort_menu_info);
        ewl_widget_show(temp2);

        temp3=ewl_menu_item_new();
        ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
        if(get_nav_mode()==1)
            ewl_widget_state_set((EWL_MENU_ITEM(temp3)->button).label_object,"select",EWL_STATE_PERSISTENT);
        ewl_widget_name_set(temp3,"sortmenuitem1");
        ewl_widget_show(temp3);
        
        temp3=ewl_menu_item_new();
        ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
        ewl_widget_name_set(temp3,"sortmenuitem2");
        ewl_widget_show(temp3);        
        
        temp3=ewl_menu_item_new();
        ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
        ewl_widget_name_set(temp3,"sortmenuitem3");
        ewl_widget_show(temp3);        
        
        
        temp2=ewl_menu_new();

        ewl_container_child_append(EWL_CONTAINER(temp),temp2);

        ewl_widget_name_set(temp2,"menuitem6");

        set_key_handler(EWL_MENU(temp2)->popup, &lang_menu_info);

        ewl_widget_show(temp2);

        for(i = 0; i < g_nlanguages; ++i)
        {
            Ewl_Widget* lang_menu_item = ewl_menu_item_new();
            //tempstr4=(char *)calloc(strlen(g_languages[i].name)+3+1,sizeof(char));
            if(get_nav_mode()==0)
            {
                if(!nitem_labels)
                    asprintf(&tempstr4,"%d. %s",i+1, g_languages[i].name);
                else if(i<nitem_labels)
                    asprintf(&tempstr4,"%s. %s",item_labels[i], g_languages[i].name);
                else
                    asprintf(&tempstr4,"%s",g_languages[i].name);
            }
            else
                asprintf(&tempstr4,"%s",g_languages[i].name);
            ewl_button_label_set(EWL_BUTTON(lang_menu_item),tempstr4);
            free(tempstr4);
            ewl_container_child_append(EWL_CONTAINER(temp2), lang_menu_item);
            if(get_nav_mode()==1 && i==0)
                ewl_widget_state_set((EWL_MENU_ITEM(lang_menu_item)->button).label_object,"select",EWL_STATE_PERSISTENT);
            sprintf(tempname6,"langmenuitem%d",i+1);
            ewl_widget_name_set(lang_menu_item,tempname6);
            ewl_widget_show(lang_menu_item);
        }

        temp2=ewl_menu_new();
        ewl_container_child_append(EWL_CONTAINER(temp),temp2);
        ewl_widget_name_set(temp2,"menuitem7");

        set_key_handler(EWL_MENU(temp2)->popup, &scripts_menu_info);
        ewl_widget_show(temp2);

        count=0;
        while(scriptstrlist[count]!=NULL)
        {
            temp3=ewl_menu_item_new();
            tempstr4=(char *)calloc(strlen(scriptstrlist[count])+3+1,sizeof(char));
            if(get_nav_mode()==0)
            {
                if(!nitem_labels)
                    asprintf(&tempstr4,"%d. %s",count+1,scriptstrlist[count]);
                else if(count<nitem_labels)
                    asprintf(&tempstr4,"%s. %s",item_labels[i],scriptstrlist[count]);
                else
                    asprintf(&tempstr4,"%s",scriptstrlist[count]);
            }
            else
                asprintf(&tempstr4,"%s",scriptstrlist[count]);
            ewl_button_label_set(EWL_BUTTON(temp3),tempstr4);
            free(tempstr4);
            ewl_container_child_append(EWL_CONTAINER(temp2),temp3);
            if(get_nav_mode()==1 && count==0)
                ewl_widget_state_set((EWL_MENU_ITEM(temp3)->button).label_object,"select",EWL_STATE_PERSISTENT);
            sprintf(tempname6,"scriptsmenuitem%d",count+1);
            ewl_widget_name_set(temp3,tempname6);
            ewl_widget_show(temp3);
            count++;
        }

        
        
        
        


    }
    statuslabel = ewl_label_new();
    ewl_container_child_append(EWL_CONTAINER(menubar), statuslabel);
    ewl_widget_name_set(statuslabel,"statuslabel");
    ewl_object_fill_policy_set(EWL_OBJECT(statuslabel),EWL_FLAG_FILL_HFILL);
    ewl_label_text_set(EWL_LABEL(statuslabel),"");
    ewl_widget_show(statuslabel);
    
    keystatelabel= ewl_label_new();
    ewl_container_child_append(EWL_CONTAINER(menubar), keystatelabel);
    ewl_widget_name_set(keystatelabel,"keystatelabel");
    ewl_object_fill_policy_set(EWL_OBJECT(keystatelabel),EWL_FLAG_FILL_HSHRINKABLE);
    ewl_object_minimum_w_set(EWL_OBJECT(keystatelabel),30);
    ewl_label_text_set(EWL_LABEL(keystatelabel),"");
    ewl_widget_show(keystatelabel);
    
    
    ewl_container_child_append(EWL_CONTAINER(box2),menubar);
    
    update_menu();
    ewl_widget_show(menubar);

    sorttypetext=ewl_label_new();
    ewl_container_child_append(EWL_CONTAINER(box3), sorttypetext);
    ewl_widget_name_set(sorttypetext,"sortlabel");
    ewl_theme_data_str_set(EWL_WIDGET(sorttypetext),"/label/group","ewl/oi_label/sorttext");
    ewl_theme_data_str_set(EWL_WIDGET(sorttypetext),"/label/textpart","ewl/oi_label/sorttext/text");
    ewl_object_alignment_set(EWL_OBJECT(sorttypetext),EWL_FLAG_ALIGN_RIGHT);
    update_sort_label();
    ewl_widget_show(sorttypetext);
    
    for(count=0;count<num_books;count++)
    {
        sprintf(tempname1,"bookbox%d",count);
        box = ewl_hbox_new();
        ewl_container_child_append(EWL_CONTAINER(box3), box);
        if(!nitem_labels)
            asprintf(&tempname5,"%d",count+1);
        else if(count<nitem_labels)
        {
            asprintf(&tempname5,"%s",item_labels[count]);
            
        }
        else
            asprintf(&tempname5,"");
        ewl_theme_data_str_set(EWL_WIDGET(box),"/hbox/group","ewl/box/oi_bookbox");//tempname5);
        if(get_nav_mode()==0)
            ewl_widget_appearance_part_text_set(box,"ewl/box/oi_bookbox/text",tempname5);
        free(tempname5);
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
        ewl_label_text_set(EWL_LABEL(authorlabel), "");
        //ewl_object_padding_set(EWL_OBJECT(authorlabel),3,0,0,0);
        ewl_theme_data_str_set(EWL_WIDGET(authorlabel),"/label/group","ewl/oi_label/authortext");
        ewl_theme_data_str_set(EWL_WIDGET(authorlabel),"/label/textpart","ewl/oi_label/authortext/text");
        ewl_object_fill_policy_set(EWL_OBJECT(authorlabel), EWL_FLAG_FILL_VSHRINK| EWL_FLAG_FILL_HFILL);

        sprintf (tempname3, "titlelabel%d",count);
        titlelabel = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box5), titlelabel);
        ewl_widget_name_set(titlelabel,tempname3 );
        ewl_label_text_set(EWL_LABEL(titlelabel), "");
        //ewl_object_padding_set(EWL_OBJECT(titlelabel),3,0,0,0);
        ewl_theme_data_str_set(EWL_WIDGET(titlelabel),"/label/group","ewl/oi_label/titletext");
        ewl_theme_data_str_set(EWL_WIDGET(titlelabel),"/label/textpart","ewl/oi_label/titletext/text");
        ewl_object_fill_policy_set(EWL_OBJECT(titlelabel), EWL_FLAG_FILL_VSHRINK| EWL_FLAG_FILL_HFILL);

        sprintf (tempname3, "seriesbox%d",count);
        box7 = ewl_hbox_new();
        ewl_container_child_append(EWL_CONTAINER(box5),box7);
        ewl_widget_name_set(box7,tempname3 );
        ewl_theme_data_str_set(EWL_WIDGET(box7),"/hbox/group","ewl/blank");
        ewl_object_fill_policy_set(EWL_OBJECT(box7),EWL_FLAG_FILL_HFILL|EWL_FLAG_FILL_VSHRINKABLE);
        
        sprintf (tempname3, "serieslabel%d",count);
        serieslabel = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box7), serieslabel);
        ewl_widget_name_set(serieslabel,tempname3 );
        ewl_theme_data_str_set(EWL_WIDGET(serieslabel),"/label/group","ewl/oi_label/seriestext");
        ewl_theme_data_str_set(EWL_WIDGET(serieslabel),"/label/textpart","ewl/oi_label/seriestext/text");
        ewl_object_fill_policy_set(EWL_OBJECT(serieslabel),EWL_FLAG_FILL_HSHRINK);
        
        
        sprintf (tempname3, "seriesnumlabel%d",count);
        seriesnumlabel = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box7), seriesnumlabel);
        ewl_widget_name_set(seriesnumlabel,tempname3 );
        ewl_theme_data_str_set(EWL_WIDGET(seriesnumlabel),"/label/group","ewl/oi_label/seriesnumtext");
        ewl_theme_data_str_set(EWL_WIDGET(seriesnumlabel),"/label/textpart","ewl/oi_label/seriesnumtext/text");
        
        
        
        
        sprintf (tempname3, "infobox%d",count);
        box6 = ewl_hbox_new();
        ewl_container_child_append(EWL_CONTAINER(box5),box6);
        ewl_widget_name_set(box6,tempname3 );
        ewl_theme_data_str_set(EWL_WIDGET(box6),"/hbox/group","ewl/blank");
        ewl_object_fill_policy_set(EWL_OBJECT(box6),EWL_FLAG_FILL_HFILL|EWL_FLAG_FILL_VSHRINKABLE);
        
        
        
        sprintf (tempname3, "taglabel%d",count);
        taglabel = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box6), taglabel);
        ewl_widget_name_set(taglabel,tempname3 );
        ewl_theme_data_str_set(EWL_WIDGET(taglabel),"/label/group","ewl/oi_label/tagtext");
        ewl_theme_data_str_set(EWL_WIDGET(taglabel),"/label/textpart","ewl/oi_label/tagtext/text");
        ewl_object_fill_policy_set(EWL_OBJECT(taglabel), EWL_FLAG_FILL_HFILL);

        
        sprintf (tempname3, "infolabel%d",count);
        infolabel = ewl_label_new();
        ewl_container_child_append(EWL_CONTAINER(box6), infolabel);
        ewl_widget_name_set(infolabel,tempname3 );
        ewl_theme_data_str_set(EWL_WIDGET(infolabel),"/label/group","ewl/oi_label/infotext");
        ewl_theme_data_str_set(EWL_WIDGET(infolabel),"/label/textpart","ewl/oi_label/infotext/text");
        ewl_object_fill_policy_set(EWL_OBJECT(infolabel), EWL_FLAG_FILL_HFILL);

        sprintf(tempname4,"separator%d",count);
        dividewidget = ewl_hseparator_new();
        ewl_container_child_append(EWL_CONTAINER(box3), dividewidget);

        ewl_widget_name_set(dividewidget,tempname4 );
    }
    free_item_labels_array(item_labels,nitem_labels);
    
    arrow_widget = ewl_widget_new();
    ewl_container_child_append(EWL_CONTAINER(box3), arrow_widget);
    ewl_widget_name_set(arrow_widget,"arrow_widget");
    ewl_object_alignment_set(EWL_OBJECT(arrow_widget),EWL_FLAG_ALIGN_RIGHT|EWL_FLAG_ALIGN_TOP);
    ewl_theme_data_str_set(EWL_WIDGET(arrow_widget),"/group","ewl/widget/oi_arrows");
    ewl_widget_show(arrow_widget);
    
    {
        Ewl_Widget *context;
        Ewl_Widget *cont_item;
        
        context=ewl_context_menu_new();
        ewl_widget_name_set(context,"main_context");
        ewl_popup_type_set(EWL_POPUP(context),EWL_POPUP_TYPE_MOUSE);
        set_key_handler(context, &mc_menu_info);
        ewl_context_menu_attach(EWL_CONTEXT_MENU(context), EWL_WIDGET(win));
        
        
        cont_item=ewl_menu_item_new();
        ewl_widget_name_set(cont_item,"mc_menuitem1");
        
        ewl_container_child_append(EWL_CONTAINER(context),cont_item);
        if(get_nav_mode()==1)
            ewl_widget_state_set((EWL_MENU_ITEM(cont_item)->button).label_object,"select",EWL_STATE_PERSISTENT);
        ewl_widget_show(cont_item);
        
        cont_item=ewl_menu_item_new();
        ewl_widget_name_set(cont_item,"mc_menuitem2");
        
        ewl_container_child_append(EWL_CONTAINER(context),cont_item);
        ewl_widget_show(cont_item);
        
        cont_item=ewl_menu_item_new();
        ewl_widget_name_set(cont_item,"mc_menuitem3");
        
        ewl_container_child_append(EWL_CONTAINER(context),cont_item);
        ewl_widget_show(cont_item);
        
        cont_item=ewl_menu_item_new();
        ewl_widget_name_set(cont_item,"mc_menuitem4");
        
        ewl_container_child_append(EWL_CONTAINER(context),cont_item);
        ewl_widget_show(cont_item);
        
        update_context_menu();
        ewl_widget_realize(context);
        update_context_menu();
        //ewl_object_init(EWL_OBJECT(context));
        //ewl_widget_configure(context);
    }
    
    {
        Ewl_Widget *idialog;
        Ewl_Widget *confirmlabel;
        Ewl_Widget *yesnobox;
        Ewl_Widget *nolabel;
        Ewl_Widget *yeslabel;
        
        idialog=ewl_dialog_new();
        ewl_window_title_set ( EWL_WINDOW ( idialog ), "EWL_DIALOG" );
        ewl_window_name_set ( EWL_WINDOW (  idialog ), "EWL_DIALOG" );
        ewl_window_class_set ( EWL_WINDOW ( idialog ), "EWLDialog" );
        ewl_theme_data_str_set(EWL_WIDGET(idialog),"/dialog/group","ewl/dialog/oi_confirmdialog");
        ewl_theme_data_str_set(EWL_WIDGET(idialog),"/dialog/vbox/hseparator/group","ewl/dialog/oi_confirmdialog/spacer");
        ewl_theme_data_str_set(EWL_WIDGET(idialog),"/dialog/vbox/actionarea/group","ewl/dialog/oi_confirmdialog/actionarea");
        ewl_callback_append(idialog, EWL_CALLBACK_REVEAL, idialog_reveal, NULL);
        ewl_callback_append(idialog, EWL_CALLBACK_UNREALIZE, idialog_unrealize, NULL);
        set_key_handler(idialog, &confirm_dialog_info);
        ewl_widget_name_set(idialog,"confirm_dialog");
        ewl_window_dialog_set(EWL_WINDOW(idialog),1);
        ewl_window_transient_for(EWL_WINDOW(idialog),EWL_WINDOW(win));
        
        ewl_object_fill_policy_set(EWL_OBJECT(EWL_DIALOG(idialog)->action_area),EWL_FLAG_FILL_HFILL);
        ewl_object_fill_policy_set(EWL_OBJECT(EWL_DIALOG(idialog)->action_box),EWL_FLAG_FILL_HFILL);
        
        
        confirmlabel=ewl_label_new();
        
        ewl_object_fill_policy_set(EWL_OBJECT(confirmlabel),EWL_FLAG_FILL_HFILL);
        ewl_container_child_append(EWL_CONTAINER(EWL_DIALOG(idialog)->vbox),confirmlabel);
        ewl_widget_name_set(confirmlabel,"confirm_dialog_message");
        ewl_widget_show(confirmlabel);
        
        nolabel=ewl_label_new();
        ewl_label_text_set(EWL_LABEL(nolabel),"No");
        ewl_object_fill_policy_set(EWL_OBJECT(nolabel),EWL_FLAG_FILL_HFILL);
        ewl_container_child_append(EWL_CONTAINER(EWL_DIALOG(idialog)->action_box),nolabel);
        ewl_theme_data_str_set(EWL_WIDGET(nolabel),"/label/group","ewl/dialog/oi_confirmdialog/reverse_label");
        ewl_theme_data_str_set(EWL_WIDGET(nolabel),"/label/textpart","ewl/dialog/oi_confirmdialog/reverse_label/text");
        ewl_widget_name_set(nolabel,"confirm_dialog_nolabel");
        ewl_widget_show(nolabel);
                
        yeslabel=ewl_label_new();
        ewl_label_text_set(EWL_LABEL(yeslabel),"Yes");
        ewl_object_fill_policy_set(EWL_OBJECT(yeslabel),EWL_FLAG_FILL_HFILL);
        ewl_container_child_append(EWL_CONTAINER(EWL_DIALOG(idialog)->action_box),yeslabel);
        ewl_theme_data_str_set(EWL_WIDGET(yeslabel),"/label/group","ewl/dialog/oi_confirmdialog/reverse_label");
        ewl_theme_data_str_set(EWL_WIDGET(yeslabel),"/label/textpart","ewl/dialog/oi_confirmdialog/reverse_label/text");
        ewl_widget_name_set(yeslabel,"confirm_dialog_yeslabel");
        ewl_widget_show(yeslabel);
        
    }
    free(theme_file);

    update_list();
    ewl_widget_focus_send(EWL_WIDGET(border));

    ecore_event_handler_add(ECORE_EVENT_SIGNAL_HUP,sighup_signal_handler,NULL);
    ewl_main();

    save_state();
    free(statefilename);
    
    free_filters();
    fini_database();
    eet_shutdown();
    CloseIniFile ();
    fini_filelist();

    roots_destroy(g_roots);
    
    free(scriptstrlist);
    unload_extractors(extractors);
    if(action_filename)
        free(action_filename);
    free(filterstatus);
    
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
