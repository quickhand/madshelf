#define _GNU_SOURCE
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "madshelf_extractors.h"

#define EXTRACTORS_DIR "/usr/lib/libextractor"

/*
 * Returns full path, not need to be freeed.
 */
const char* get_extractors_dir()
{
    return getenv("EXTRACTORS_DIR")
        ? getenv("EXTRACTORS_DIR")
        : EXTRACTORS_DIR;
}

/*
 * Returns full path, to be free(3)ed later.
 */
char* get_full_path(const char* extractor_file)
{
    char* full_path;
    asprintf(&full_path, "%s/%s", get_extractors_dir(), extractor_file);
    return full_path;
}

int filter_files(const struct dirent* d)
{
    int len = strlen(d->d_name);

    if(d->d_type == DT_LNK)
    {
        char *libname = get_full_path(d->d_name);
        struct stat s;
        if(-1 == stat(libname, &s)
           || !S_ISREG(s.st_mode))
        {
            free(libname);
            return 0;
        }
        free(libname);
    }
    else if(d->d_type != DT_REG)
        return 0;
    
    return (len > 2) && !strcmp(d->d_name + len - 3, ".so");
}

extractors_t* load_extractors()
{
    struct dirent** files;
    int nfiles = scandir(get_extractors_dir(),
                         &files, &filter_files, &versionsort);
    int i;

    if(nfiles == -1)
    {
        perror("Unable to load extractors");
        exit(1);
    }
    
    extractors_t* head = NULL;

    for(i = 0; i != nfiles; ++i)
    {
        extractors_t* nhead;
        void* libhandle;
        void* extract_handle;
        char* extract_name;
        char* libname = get_full_path(files[0]->d_name);
        if(!libname)
        {
            fprintf(stderr, "Out of memory while loading extractor %s",
                    files[i]->d_name);
            exit(1);
        }
        
        libhandle = dlopen(libname, RTLD_LAZY);
        if(!libhandle)
        {
            fprintf(stderr, "Unable to load %s: %s\n", libname, dlerror());
            free(libname);
            continue;
        }

        free(libname);

        /* Remove '.so' from filename */
        files[i]->d_name[strlen(files[i]->d_name)-3] = 0;
        /* libextractor_foo_extract */
        asprintf(&extract_name, "%s_extract", files[i]->d_name);
        if(!extract_name)
        {
            fprintf(stderr, "Out of memory while loading extractor %s",
                    files[i]->d_name);
            exit(1);
        }

        extract_handle = dlsym(libhandle, extract_name);
        if(!extract_handle)
        {
            fprintf(stderr, "Unable to get entry point in %s: %s\n",
                    libname, dlerror());
            free(extract_name);
            dlclose(libhandle);
            continue;
        }

        free(extract_name);

        nhead = malloc(sizeof(extractors_t));
        if(!nhead)
        {
            fprintf(stderr, "Out of memory while loading extractor %s",
                    files[i]->d_name);
            exit(1);
        }

        nhead->handle = libhandle;
        nhead->method = (ExtractMethod)extract_handle;
        nhead->next = head;

        head = nhead;
    }

    free(files);

    return head;
}

void unload_extractors(extractors_t* extractors)
{
    while(extractors)
    {
        extractors_t* c = extractors;
        dlclose(c->handle);
        extractors = c->next;
        free(c);
    }
}

EXTRACTOR_KeywordList* extractor_get_keywords(extractors_t* extractors,
                                              const char* filename)
{
    EXTRACTOR_KeywordList* list = NULL;
    int fd = open(filename, O_RDONLY);
    struct stat s;
    void* m;
    if(fd == -1)
    {
        fprintf(stderr, "Unable to open %s for obtaining keywords: %s",
                filename, strerror(errno));
        goto err1;
    }

    if(-1 == fstat(fd, &s))
    {
        fprintf(stderr, "Unable to stat %s for obtaining keywords: %s",
                filename, strerror(errno));
        goto err2;
    }

    m = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE | MAP_NONBLOCK, fd, 0);
    if(m == (void*)-1)
    {
        fprintf(stderr, "Unable to mmap %s for obtaining keywords: %s",
                filename, strerror(errno));
        goto err2;
    }
    
    while(extractors)
    {
        list = (*extractors->method)(filename, m, s.st_size, list, "");
        extractors = extractors->next;
    }

    munmap(m, s.st_size);
err2:
    close(fd);
err1:
    return list;
}

const char* extractor_get_last(const EXTRACTOR_KeywordType type,
                               const EXTRACTOR_KeywordList* keywords)
{
    const char* result = NULL;
    while(keywords)
    {
        if(keywords->keywordType == type)
            result = keywords->keyword;
        keywords = keywords->next;
    }
    return result;
}

const char* extractor_get_first(const EXTRACTOR_KeywordType type,
                                const EXTRACTOR_KeywordList* keywords)
{
    while(keywords)
    {
        if(keywords->keywordType == type)
            return keywords->keyword;
        keywords = keywords->next;
    }
    return NULL;
}
