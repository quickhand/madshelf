INCLUDES = @EWL_CFLAGS@ @EET_CFLAGS@ @EXTRACT_CFLAGS@ @SQLITE_CFLAGS@
AM_CFLAGS=-Wall

bin_PROGRAMS = madshelf

madshelf_SOURCES = IniFile.c madshelf_main.c madshelf_extractors.c Keyhandler.c filefilter.c filtertree.c Dialogs.c Choicebox.c database.c tags.c
madshelf_LDADD = @EWL_LIBS@ @EET_LIBS@ @SQLITE_LIBS@
