#ifndef MADSHELF_DATABASE_H
#define MADSHELF_DATABASE_H

#define RECORD_STATUS_ERROR -1
#define RECORD_STATUS_OK 0
#define RECORD_STATUS_OUT_OF_DATE 1
#define RECORD_STATUS_ABSENT 2
#define RECORD_STATUS_EXISTS_BUT_UNKNOWN 3

int init_database(char *filename);
void fini_database();


int get_file_record_status(char *filename);
int update_file_mod_time(char *filename);
int clear_file_extractor_data(char *filename);
int clear_tags(char *filename);
void set_authors(char *filename,const char *authors[],int numauthors);
void set_titles(char *filename,const char *titles[],int numtitles);
void set_series(char *filename,const char *series[],int *seriesnum,int numseries);
void set_tags(char *filename,const char *tags[],int numtags);
int get_authors(char *filename,char ***authors);
int get_titles(char *filename,char ***titles);
int get_series(char *filename,char ***seriesname,int **seriesnum);
int get_tags(char *filename,char ***tagnames);
int create_empty_record(char *filename);
int remove_tag(char *filename,char *tagname);
#endif
