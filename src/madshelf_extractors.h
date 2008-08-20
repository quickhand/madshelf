#ifndef MADSHELF_EXTRACTORS_H
#define MADSHELF_EXTRACTORS_H

/*
 * libextractor-compatible extractors handling.
 */

#include <extractor.h>

typedef struct extractors_t
{
    void* handle;
    ExtractMethod method;
    struct extractors_t* next;
} extractors_t;

/*
 * Loads extractors from hardcoded directory (/usr/lib/madshelf/extractors) or
 * (if set) from ENV{EXTRACTORS_DIR}
 */
extractors_t* load_extractors();

void unload_extractors(extractors_t* extractors);

EXTRACTOR_KeywordList* extractor_get_keywords(extractors_t* extractors,
                                              const char* filename);


const char* extractor_get_last(const EXTRACTOR_KeywordType type,
                               const EXTRACTOR_KeywordList* keywords);

const char* extractor_get_first(const EXTRACTOR_KeywordType type,
                                const EXTRACTOR_KeywordList* keywords);

#endif
