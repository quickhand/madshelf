#ifndef BOOK_META_H
#define BOOK_META_H


typedef struct
{
    char* title; /* Must be not-NULL */
    char* author; /* NULL means "N/A" */
    char* series; /* NULL means "N/A" */
    int series_n; /* irrelevant if series == NULL */
} book_meta_t;







#endif
