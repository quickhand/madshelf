#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sqlite.h>
#include <sys/stat.h>
#include "database.h"
static struct sqlite *madshelf_database=NULL;
long get_file_index(char *filename,int create_entry_if_missing);
long get_table_index(char *value,char *tablename,char *idcolname,char *valcolname);
long get_author_index(char *authorname);
long get_title_index(char *title);
long get_series_index(char *seriesname);
long get_tag_index(char *tagname);


int init_database(char *filename)
{
    char **errmsg=NULL;
    madshelf_database=sqlite_open(filename,O_RDWR,errmsg);
    if(errmsg!=NULL)
    {
        free(errmsg);
        return -1;
    }
    sqlite_exec(madshelf_database,"CREATE TABLE files(fileid INTEGER PRIMARY KEY,filename TEXT UNIQUE,mod_time INTEGER)",NULL,NULL,NULL);
    sqlite_exec(madshelf_database,"CREATE TABLE titles(titleid INTEGER PRIMARY KEY,title TEXT UNIQUE)",NULL,NULL,NULL);
    sqlite_exec(madshelf_database,"CREATE TABLE authors(authorid INTEGER PRIMARY KEY,authorname TEXT UNIQUE)",NULL,NULL,NULL);
    sqlite_exec(madshelf_database,"CREATE TABLE series(seriesid INTEGER PRIMARY KEY,seriesname TEXT UNIQUE)",NULL,NULL,NULL);
    sqlite_exec(madshelf_database,"CREATE TABLE tags(tagid INTEGER PRIMARY KEY,tagname TEXT UNIQUE)",NULL,NULL,NULL);
    sqlite_exec(madshelf_database,"CREATE TABLE booktitles(fileid INTEGER,titleid INTEGER)",NULL,NULL,NULL);
    sqlite_exec(madshelf_database,"CREATE TABLE bookauthors(fileid INTEGER,authorid INTEGER)",NULL,NULL,NULL);
    sqlite_exec(madshelf_database,"CREATE TABLE bookseries(fileid INTEGER,seriesid INTEGER,seriesnum INTEGER)",NULL,NULL,NULL);
    sqlite_exec(madshelf_database,"CREATE TABLE booktags(fileid INTEGER,tagid INTEGER)",NULL,NULL,NULL);
    return 0;
}

int get_file_record_status(char *filename)
{
    

    char **resultp=NULL;
    int rows,cols;
    
    

    if(sqlite_get_table_printf(madshelf_database,"SELECT mod_time FROM files WHERE filename = \'%q\'",&resultp,&rows,&cols,NULL,filename)!=SQLITE_OK)
    {
        return RECORD_STATUS_ERROR;
    }
    else if(rows==0)
    {
        sqlite_free_table(resultp);    
        return RECORD_STATUS_ABSENT;
        
    }
    /*else if(rows>1)
    {
        sqlite_free_table(resultp);    
        return RECORD_STATUS_MULTIPLE_RECORDS;
        
    }*/
    long table_modtime=strtol(resultp[cols],NULL,10);
    sqlite_free_table(resultp);
    
    struct stat filestat;
    int fileexists;
    fileexists = stat(filename, &filestat);
    if(fileexists<0)
        return RECORD_STATUS_EXISTS_BUT_UNKNOWN;
    
    
    
    if(table_modtime==filestat.st_mtime)
        return RECORD_STATUS_OK;    
    else
        return RECORD_STATUS_OUT_OF_DATE;
    
    
}

int update_file_mod_time(char *filename)
{
    struct stat filestat;
    int fileexists;
    fileexists = stat(filename, &filestat);
    if(fileexists<0)
    {
        return -1;
    }
    
    char **resultp=NULL;
    int rows,cols;
    

    if(sqlite_get_table_printf(madshelf_database,"SELECT mod_time FROM files WHERE filename = \'%q\'",&resultp,&rows,&cols,NULL,filename)!=SQLITE_OK)
    {
        sqlite_free_table(resultp);    
        return -1;
    }
    /*if(rows>1)
    {
        sqlite_free_table(resultp);
        return -1;
    }*/
    else if(rows==0)
        sqlite_exec_printf(madshelf_database,"INSERT INTO files (filename,mod_time) VALUES(\'%q\',%d)",NULL,NULL,NULL,filename,filestat.st_mtime);
    else
        sqlite_exec_printf(madshelf_database,"UPDATE files SET mod_time=%d WHERE filename=\'%q\'",NULL,NULL,NULL,filestat.st_mtime,filename);
    sqlite_free_table(resultp);
    return 0;
}
int clear_file_extractor_data(char *filename)
{
    long fileindex=get_file_index(filename,0);
    if(fileindex==-1)
        return -1;
    //erase titles
    sqlite_exec_printf(madshelf_database,"DELETE FROM titles WHERE titles.titleid NOT IN (SELECT titleid FROM booktitles WHERE fileid != %d) AND titles.titleid IN (SELECT titleid FROM booktitles WHERE fileid=%d)",NULL,NULL,NULL,fileindex,fileindex);
    sqlite_exec_printf(madshelf_database,"DELETE FROM booktitles WHERE fileid = %d",NULL,NULL,NULL,fileindex);
    //erase authors        
    sqlite_exec_printf(madshelf_database,"DELETE FROM authors WHERE authors.authorid NOT IN (SELECT authorid FROM bookauthors WHERE fileid != %d) AND authors.authorid IN (SELECT authorid FROM bookauthors WHERE fileid=%d)",NULL,NULL,NULL,fileindex,fileindex);
    sqlite_exec_printf(madshelf_database,"DELETE FROM bookauthors WHERE fileid = %d",NULL,NULL,NULL,fileindex);
    //erase series
    sqlite_exec_printf(madshelf_database,"DELETE FROM series WHERE series.seriesid NOT IN (SELECT seriesid FROM bookseries WHERE fileid != %d) AND series.seriesid IN (SELECT seriesid FROM bookseries WHERE fileid=%d)",NULL,NULL,NULL,fileindex,fileindex);
    sqlite_exec_printf(madshelf_database,"DELETE FROM bookseries WHERE fileid = %d",NULL,NULL,NULL,fileindex);
    return 0;
    
}
int clear_tags(char *filename)
{
    long fileindex=get_file_index(filename,0);
    if(fileindex==-1)
        return -1;
    //erase titles
    sqlite_exec_printf(madshelf_database,"DELETE FROM tags WHERE tags.tagid NOT IN (SELECT tagid FROM booktags WHERE fileid != %d) AND tags.tagid IN (SELECT tagid FROM booktags WHERE fileid=%d)",NULL,NULL,NULL,fileindex,fileindex);
    sqlite_exec_printf(madshelf_database,"DELETE FROM booktags WHERE fileid = %d",NULL,NULL,NULL,fileindex);
    return 0;
    
}
long get_file_index(char *filename,int create_entry_if_missing)
{
    
    char **resultp=NULL;
    int rows,cols;
    int result= sqlite_get_table_printf(madshelf_database,"SELECT fileid FROM files WHERE filename = \'%q\'",&resultp,&rows,&cols,NULL,filename);

    if(rows<=0 && create_entry_if_missing)
    {
        struct stat filestat;
        int fileexists;
        fileexists = stat(filename, &filestat);
        if(fileexists<0)
        {
            return -1;
        }
        
        sqlite_free_table(resultp);
        sqlite_exec_printf(madshelf_database,"INSERT INTO files (filename,mod_time) VALUES(\'%q\',%d)",NULL,NULL,NULL,filename,filestat.st_mtime);
        result= sqlite_get_table_printf(madshelf_database,"SELECT fileid FROM files WHERE filename = \'%q\'",&resultp,&rows,&cols,NULL,filename);


    }
    if(rows<=0)
    {
        sqlite_free_table(resultp);
        return -1;
    }
    
    long retval=strtol(resultp[cols],NULL,10);
    sqlite_free_table(resultp);
    return retval;
}
long get_table_index(char *value,char *tablename,char *idcolname,char *valcolname)
{
    
    char **resultp=NULL;
    int rows,cols;
    int result= sqlite_get_table_printf(madshelf_database,"SELECT %s FROM %s WHERE %s = \'%q\'",&resultp,&rows,&cols,NULL,idcolname,tablename,valcolname,value);
    if(rows<=0)
    {
        sqlite_free_table(resultp);
        sqlite_exec_printf(madshelf_database,"INSERT INTO %s (%s) VALUES(\'%q\')",NULL,NULL,NULL,tablename,valcolname,value);
        result= sqlite_get_table_printf(madshelf_database,"SELECT %s FROM %s WHERE %s = \'%q\'",&resultp,&rows,&cols,NULL,idcolname,tablename,valcolname,value);
        
    }
    if(rows<=0)
    {
        sqlite_free_table(resultp);
        return -1;
    }
    long retval=strtol(resultp[cols],NULL,10);
    sqlite_free_table(resultp);
    return retval;
}
long get_author_index(char *authorname)
{
    return get_table_index(authorname,"authors","authorid","authorname");    
}
long get_title_index(char *title)
{
    return get_table_index(title,"titles","titleid","title");
}
long get_series_index(char *seriesname)
{
    return get_table_index(seriesname,"series","seriesid","seriesname");    
}
long get_tag_index(char *tagname)
{
    return get_table_index(tagname,"tags","tagid","tagname");
}

void set_authors(char *filename,const char *authors[],int numauthors)
{
    long fileindex=get_file_index(filename,1);
    long authorindex;

    if(fileindex==(-1))
        return;
    
    int i;
    for(i=0;i<numauthors;i++)
    {
        authorindex=get_author_index(authors[i]);
        if(authorindex!=-1)
        {
            sqlite_exec_printf(madshelf_database,"INSERT INTO bookauthors (fileid,authorid) VALUES(%d,%d)",NULL,NULL,NULL,fileindex,authorindex);
 
        }
    }
}
void set_titles(char *filename,const char *titles[],int numtitles)
{
    long fileindex=get_file_index(filename,1);
    long titleindex;
    char **errmsg=NULL;
    if(fileindex==(-1))
        return;
    
    int i;
    for(i=0;i<numtitles;i++)
    {
        titleindex=get_title_index(titles[i]);
        if(titleindex!=-1)
        {
            sqlite_exec_printf(madshelf_database,"INSERT INTO booktitles (fileid,titleid) VALUES(%d,%d)",NULL,NULL,errmsg,fileindex,titleindex);
            if(errmsg)
            {
                free(errmsg);
                errmsg=NULL;
            }
        }
    }
}
void set_series(char *filename,const char *series[],int *seriesnum,int numseries)
{
    long fileindex=get_file_index(filename,1);
    long seriesindex;

    if(fileindex==(-1))
        return;
    
    int i;
    for(i=0;i<numseries;i++)
    {
        seriesindex=get_series_index(series[i]);
        if(seriesindex!=-1)
        {
            
            sqlite_exec_printf(madshelf_database,"INSERT INTO bookseries (fileid,seriesid,seriesnum) VALUES(%d,%d,%d)",NULL,NULL,NULL,fileindex,seriesindex,*(seriesnum+i));

        }
    }
    
}
void set_tags(char *filename,const char *tags[],int numtags)
{
    long fileindex=get_file_index(filename,1);
    long tagindex;

    if(fileindex==(-1))
        return;
    
    int i;
    for(i=0;i<numtags;i++)
    {
        tagindex=get_tag_index(tags[i]);
        if(tagindex!=-1)
        {
            sqlite_exec_printf(madshelf_database,"INSERT INTO booktags (fileid,tagid) VALUES(%d,%d)",NULL,NULL,NULL,fileindex,tagindex);

        }
    }
}
int get_authors(char *filename,char ***authors)
{
    char **resultp;
    int rows,cols;
     
    int result= sqlite_get_table_printf(madshelf_database,"SELECT authorname FROM authors WHERE authorid IN (SELECT authorid FROM bookauthors WHERE bookauthors.fileid = (SELECT fileid FROM files WHERE \'%q\' = filename ))",&resultp,&rows,&cols,NULL,filename);
    if(rows<=0)
    {
        *authors=NULL;
        return 0;
    }
    else
        *authors=(char **)malloc(rows*sizeof(char*));
    int i;
    for(i=0;i<rows;i++)
    {
        asprintf((char **)(&(**authors)+i),resultp[i+cols]);
    }
    
    sqlite_free_table(resultp);
    return rows;    
}
int get_titles(char *filename,char ***titles)
{
    
    
    char **resultp;
    int rows,cols;
    
    int result= sqlite_get_table_printf(madshelf_database,"SELECT title FROM titles WHERE titleid IN (SELECT titleid FROM booktitles WHERE booktitles.fileid = (SELECT fileid FROM files WHERE \'%q\' = filename))",&resultp,&rows,&cols,NULL,filename);
    if(rows<=0)
    {
        *titles=NULL;
        return 0;
    }
    else
        *titles=(char **)malloc(rows*sizeof(char*));
    int i;
    for(i=0;i<rows;i++)
    {
        
        asprintf((char **)(&(**titles)+i),resultp[i+cols]);
    }
    
    sqlite_free_table(resultp);
 
    return rows;    
}
int get_series(char *filename,char ***seriesname,int **seriesnum)
{
    
    char **resultp;
    int rows,cols;
    
    int result= sqlite_get_table_printf(madshelf_database,"SELECT seriesname FROM series WHERE seriedid IN (SELECT seriesid FROM bookseries WHERE bookseries.fileid = (SELECT fileid FROM files WHERE \'%q\' = filename)) UNION SELECT seriesnum FROM bookseries WHERE bookseries.fileid IN (SELECT fileid FROM files WHERE \'%q\' = filename)",&resultp,&rows,&cols,NULL,filename,filename);
    if(rows<=0)
    {
        *seriesname=NULL;
        *seriesnum=NULL;
        return 0;
    }
    else
    {
        *seriesname=(char **)malloc(rows/2*sizeof(char*));
        *seriesnum=(char *)malloc(rows/2*sizeof(int));
    }
    
    int i;
    for(i=0;i<rows/2;i++)
    {
        
        asprintf((char **)(&(**seriesname)+i),resultp[i+rows/2+cols]);
        *(*seriesnum+i)=(int)strtol(resultp[i+cols],NULL,10);
    }
    
    sqlite_free_table(resultp);
    return rows/2;    
}
int get_tags(char *filename,char ***tagnames)
{
    
    
    char **resultp;
    int rows,cols;
     
    
    int result= sqlite_get_table_printf(madshelf_database,"SELECT tagname FROM tags WHERE tagid IN (SELECT tagid FROM booktags WHERE booktags.fileid = (SELECT fileid FROM files WHERE \'%q\' = filename))",&resultp,&rows,&cols,NULL,filename);
    if(rows<=0)
    {
        *tagnames=NULL;
        return 0;    
    }
    else
        *tagnames=(char **)malloc(rows*sizeof(char*));
    int i;
    for(i=0;i<rows;i++)
    {
        
        asprintf((char **)(&(**tagnames)+i),resultp[i+cols]);
    }
    
    sqlite_free_table(resultp);
    
    return rows;    
}
void fini_database()
{
    sqlite_close(madshelf_database);
    madshelf_database=NULL;
}
