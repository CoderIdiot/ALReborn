
/*
 * Header file generation.
 *
 * Generate a publicly accessible RenderWare header file from the
 * internal module headers. This utility parses all files + dependants
 * from the command line and extracts all data bracketed by
 * RWPUBLIC and RWPUBLICEND keywords held in comments.
 *
 * Usage: incgen [options]
 *
 * Options are:
 *
 * -i<dirspec>
 * -I<dirspec>
 *     Specify include directory to find dependency files, eg -I./src
 * -o<filespec>
 *     Specify output filename, eg -orwcore.h
 * -s<filespec>
 *     Specify system filename to #include, eg -sstdio.h
 * -g<filespec>
 *     Write guard that generated file is included before this one, eg -grwcore.h.
 * -v
 *     Verbose mode on - prints out extra debug info, like dependencies found.
 * -l<filespec>
 *     Specify list output file - produces a list of the files included in the
 *     generated file.
 * -x<filespec>
 *     Specify exclude input file - rejects files listed in this file - should be
 *     early in the options order.
 * -r<filespec>
 *     Remove single file from the reject list.  Should appear in options list
 *     after the file has been added to the reject list.
 * <filespec>
 *     Other non -options are files to start with - add *.h files you want to parse
 *     using.
 * -p<rpefile>
 *     Specify name of rpe file - produces a '#include <rpefile>.rpe' at the top
 *     of the produced file.
 *
 * This comment is generated by the RCS co command:
 * $Id: //RenderWare/RW37Active/rwsdk/buildtools/incgen/main.c#1 $
 * <file name> <rev> <date & time> <author> <state> <currently locked by>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#if (defined(_MSC_VER))
#if ((_MSC_VER>=1000) && defined(_DEBUG))
#include <windows.h>
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif /* ((_MSC_VER>=1000) && defined(_DEBUG)) */
#endif /* (defined(_MSC_VER)) */

#include "main.h"

#ifdef __ELATE__
#define stricmp(s1, s2) strcmpi(s1, s2)
#endif

#ifdef __linux__
#define stricmp(s1, s2) strcasecmp(s1, s2)
#endif

extern int          yylex(void);

extern FILE        *yyin;       /* Lexical analyser input file */

/*--- Structure Definitions ---*/

/* Simple Linked List structure. This structure along with supporting
 * functions provides generic linked list management for lists of filenames
 */

typedef struct stLink TyLink;
struct stLink
{
    char               *name;
    TyLink             *next;
    TyLink             *prev;
};

/* Extended linked list structure to support dependancy lists.
 * Note: the link member must be the first member in order for the generic
 * linked list functions to operate on this structure.
 */

typedef struct stFileLink TyFileLink;
struct stFileLink
{
    TyLink              link;
    TyLink             *child;
    int                 childCount;
};

/*--- Global Variables ---*/

static TyFileLink  *FileList = NULL; /* list of all files from command

                                      *
                                      * * line and dependencies
                                      */
static TyFileLink  *RejectFileList = NULL; /* List of all the files that must

                                            *
                                            * * not be added to the process list
                                            */
static TyFileLink  *CurrentFile = NULL; /* current file being processed */
static TyLink      *PathList = NULL; /* Search path for included files */
static TyLink      *SortedList = NULL; /* all files sorted in dependancy order */
static TyLink      *GuardList = NULL; /* List of files that must be included

                                       *
                                       * * before this one
                                       */
static TyLink      *SysList = NULL; /* list of system files to be added */
static int          Verbose = 0; /* print verbose information */
static char        *OutFileName = NULL; /* Output filename */
static char        *RpeFileName = NULL; /* Name of RPE file to include */

/******************************************************************************
 *
 * Add a new element to a linked list.
 *
 *****************************************************************************/
static TyLink      *
NewLink(TyLink * start, char *name, int size)
{
    TyLink             *newLink = malloc(size);

    newLink->name = name;
    newLink->next = start;
    newLink->prev = NULL;
    if (start)
    {
        start->prev = newLink;
    }
    return (newLink);
}

/******************************************************************************
 *
 * Find a named element in a linked list.
 *
 *****************************************************************************/
static TyLink      *
FindLink(TyLink * start, char *name)
{
    TyLink             *cur;

    for (cur = start; cur; cur = cur->next)
    {
        if (!stricmp(cur->name, name))
        {
            return (cur);
        }
    }
    return (NULL);
}

/******************************************************************************
 *
 * Remove an element from a linked list
 *
 *****************************************************************************/
static TyLink      *
RemoveLink(TyLink * start, TyLink * link)
{
    TyLink             *next = link->next;

    if (link->prev)
    {
        link->prev->next = link->next;
    }
    if (link->next)
    {
        link->next->prev = link->prev;
    }

    free(link);

    if (start == link)
    {
        /* start link removed */
        return (next);
    }
    else
    {
        return (start);
    }
}

/*************************************************************************
 *
 * Print the linked list
 *
 *
 ************************************************************************/
static void
PrintLink(FILE * fp, TyLink * link, int depth)
{
    TyLink             *curLink;

    for (curLink = link; curLink; curLink = curLink->next)
    {
        int                 i;

        for (i = 0; i < depth; i++)
        {
            fprintf(fp, "\t");
        }
        fprintf(fp, "%s\n", curLink->name);
    }
}

/*************************************************************************
 *
 * Look for the specified filename on the current path. If it exists then
 * return the fully qualified filename.
 * Note: The name returned is allocated with malloc and should be freed
 *
 *************************************************************************/

char               *
ExpandPath(char *filename)
{
    TyLink             *curPath;
    char               *fullName;
    FILE               *fp;

    /* go to end of path list */
    for (curPath = PathList; curPath && curPath->next;
         curPath = curPath->next)
        ;

    while (curPath)
    {
        fullName = malloc(strlen(filename) + strlen(curPath->name) + 2);
        sprintf(fullName, "%s/%s", curPath->name, filename);

#ifdef RWDEBUG
        printf("Trying <%s>\n", fullName);
#endif
        /* test if file can be opened */
        fp = fopen(fullName, "r");
        if (fp)
        {
            fclose(fp);
#ifdef RWDEBUG
            printf("Found <%s>\n", fullName);
#endif
            return (fullName);
        }
        free(fullName);

        /* it can't so try next directory on the path */
        curPath = curPath->prev;
    }
    fprintf(stderr, "Unable to Open: %s\n", filename);
    return (NULL);
}

/*************************************************************************
 *
 * Add a new filename to the list of all dependencies.
 *
 *************************************************************************/

static void
NewFile(char *name)
{
#ifdef RWDEBUG
    printf("New File: %s\n", name);
#endif

    FileList =
        (TyFileLink *) NewLink((TyLink *) FileList, name, sizeof(TyFileLink));
    FileList->child = NULL;
    FileList->childCount = 0;
}

/*************************************************************************
 *
 * Check if the supplied filename exists already. If not then add a new
 * filename to the list of all dependencies.
 *
 *************************************************************************/
void
AddFile(char *name)
{
    if (name)
    {
        if (FindLink((TyLink *) RejectFileList, name))
        {
            /* Reject the filename, 'cos it's on the reject list */
            free(name);
            return;
        }
        if (FindLink((TyLink *) FileList, name))
        {
            /* Filename already exists on this list - we don't need it again */
            free(name);
            return;
        }
        NewFile(name);
    }
}

/*************************************************************************
 *
 * Add a new filename to the list of files to reject.
 *
 *************************************************************************/

static void
NewRejectFile(char *name)
{
#ifdef RWDEBUG
    printf("New Reject File: %s\n", name);
#endif

    RejectFileList =
        (TyFileLink *) NewLink((TyLink *) RejectFileList, name,
                               sizeof(TyFileLink));
    RejectFileList->child = NULL;
    RejectFileList->childCount = 0;
}

/*************************************************************************
 *
 * Check if the supplied filename exists already. If not then add a new
 * filename to the list of all files to reject.
 *
 *************************************************************************/
void
AddRejectFile(char *name)
{
    if (name)
    {
        if (FindLink((TyLink *) RejectFileList, name))
        {
            /* Filename already exists */
            free(name);
            return;
        }
        NewRejectFile(name);
    }
}

/*************************************************************************
 *
 * Add a filename dependency to the file being currently parsed
 *
 ************************************************************************/
void
AddChild(char *name)
{
#ifdef RWDEBUG
    printf("Adding Child <%s> to <%s>\n", name, CurrentFile->link.name);
#endif

    if (name && CurrentFile)
    {
        if (FindLink((TyLink *) RejectFileList, name))
        {
            /* Reject the filename, 'cos it's on the reject list */
            return;
        }

        CurrentFile->child =
            NewLink(CurrentFile->child, name, sizeof(TyLink));
        CurrentFile->childCount++;
    }
}

/*************************************************************************
 *
 * Add a new directory to the search path for include files
 *
 ************************************************************************/
static void
AddPath(char *name)
{
    char               *newName = malloc(strlen(name) + 1);

    strcpy(newName, name);

    PathList = NewLink(PathList, newName, sizeof(TyLink));
}

/*************************************************************************
 *
 * Remove the specified filename from the dependancy list of all other files
 *
 ************************************************************************/
static void
RemoveDependancy(char *name)
{
    TyFileLink         *curFile;

    for (curFile = FileList;
         curFile; curFile = (TyFileLink *) curFile->link.next)
    {
        TyLink             *found = FindLink(curFile->child, name);

        if (found)
        {
            free(found->name);
            curFile->child = RemoveLink(curFile->child, found);
            curFile->childCount--;
#ifdef RWDEBUG
            printf("Removed <%s> from <%s>\n", name, curFile->link.name);
#endif
        }
    }
}

/*************************************************************************
 *
 * Search for a file with no dependancies. Purge it from the
 * dependancy lists of all other files and then return it.
 * If no zero dependancy files are found then return NULL.
 *
 ************************************************************************/
static char        *
GetZeroDepFile(void)
{
    TyFileLink         *curFile;

    for (curFile = FileList;
         curFile; curFile = (TyFileLink *) curFile->link.next)
    {
        if (curFile->childCount == 0)
        {
            /* found a file with no dependencies */
            char               *name = curFile->link.name;

            RemoveDependancy(curFile->link.name);

            FileList =
                (TyFileLink *) RemoveLink((TyLink *) FileList,
                                          (TyLink *) curFile);
            return (name);
        }
    }
    return (NULL);
}


/*************************************************************************
 *
 * Print the list of all files found in reverse order  and their dependents
 *
 ************************************************************************/
static void
ReverseFileList(FILE * fp, TyFileLink * curFile)
{
    if (curFile)
    {
        ReverseFileList( fp, (TyFileLink *) curFile->link.next);

        fprintf(fp, "%s has %d dependants\n",
                curFile->link.name, curFile->childCount);
        PrintLink(fp, curFile->child, 1);
    }
}
/*************************************************************************
 *
 * Print the list of all files found and their dependents
 *
 ************************************************************************/
static void
PrintFileList(FILE * fp)
{

    ReverseFileList(fp, FileList);

    printf("\n");
}

/*************************************************************************
 *
 * Copy all public lines from input file to output file.
 * Public lines are delimited by RWPUBLIC and RWPUBLICEND embedded in comments.
 *
 ************************************************************************/
static void
ExtractPublic(FILE * in, FILE * out)
{
    char                lineBuf[256];
    int                 publicState = 0;
    int                 publicStart = 0;
    char               *substr;

    /* Read a line at a time from the file */

    while (fgets(lineBuf, sizeof(lineBuf), in))
    {
        /* look for one of the statechange strings */

        substr = strstr(lineBuf, "RWPUBLIC");
        if (substr)
        {
            if (!strncmp(substr, "RWPUBLICEND", 11))
            {
                publicState = 0;
            }
            else
            {
                if (publicState)
                {
                    fprintf(stderr, "Mismatched RWPUBLIC - RWPUBLICEND\n");
                    exit(1);
                }
                publicStart = 1;
            }
        }
        if (publicState)
        {
            fprintf(out, "%s", lineBuf);
        }
        if (publicStart)
        {
            publicState = 1;
            publicStart = 0;
        }
    }
}

/*************************************************************************
 *
 * Get the base of the name
 *
 ************************************************************************/

static char        *
ExtractBase(char *inName)
{
    char               *processBuf, *outName;
    int                 i;

    processBuf = malloc(strlen(inName) + 1);
    strcpy(processBuf, inName);

    /* extract base of filename */
    for (i = strlen(processBuf); i >= 0; i--)
    {
        if (processBuf[i] == '.')
        {
            processBuf[i] = '\0';
        }
        if ((processBuf[i] == '\\') || (processBuf[i] == '/'))
        {
            break;
        }
        processBuf[i] = toupper(processBuf[i]);
    }
    i++;

    outName = malloc(strlen(&processBuf[i]) + 1);
    strcpy(outName, &processBuf[i]);
    free(processBuf);

    return (outName);
}

/*************************************************************************
 *
 * Print output file header
 *
 ************************************************************************/
static void
PrintHeader(FILE * fp)
{
    time_t              ltime;

    time(&ltime);

    fprintf(fp, "/******************************************/\n");
    fprintf(fp, "/*                                        */\n");
    fprintf(fp, "/*    RenderWare(TM) Graphics Library     */\n");
    fprintf(fp, "/*                                        */\n");
    fprintf(fp, "/******************************************/\n");
    fprintf(fp, "\n");
    fprintf(fp, "/*\n");
    fprintf(fp, " * This file is a product of Criterion Software Ltd.\n");
    fprintf(fp, " *\n");
    fprintf(fp,
            " * This file is provided as is with no warranties of any kind and is\n");
    fprintf(fp,
            " * provided without any obligation on Criterion Software Ltd.\n");
    fprintf(fp, " * or Canon Inc. to assist in its use or modification.\n");
    fprintf(fp, " *\n");
    fprintf(fp,
            " * Criterion Software Ltd. and Canon Inc. will not, under any\n");
    fprintf(fp,
            " * circumstances, be liable for any lost revenue or other damages\n");
    fprintf(fp, " * arising from the use of this file.\n");
    fprintf(fp, " *\n");
    fprintf(fp, " * Copyright (c) 1999. Criterion Software Ltd.\n");
    fprintf(fp, " * All Rights Reserved.\n");
    fprintf(fp, " */\n");
    fprintf(fp, "\n");

    fprintf(fp,
            "/*************************************************************************\n");
    fprintf(fp, " *\n");
    if (OutFileName)
    {
        fprintf(fp, " * Filename: <%s>\n", OutFileName);
    }
    else
    {
        fprintf(fp, " * Filename: <unknown>\n");
    }
    fprintf(fp, " * Automatically Generated on: %s", ctime(&ltime));
    fprintf(fp, " *\n");
    fprintf(fp,
            " ************************************************************************/\n");
    fprintf(fp, "\n");

    if (OutFileName)
    {
        char               *base;

        base = ExtractBase(OutFileName);
        fprintf(fp, "#ifndef %s_H\n", base);
        fprintf(fp, "#define %s_H\n", base);
        fprintf(fp, "\n");
        free(base);
    }
    if (GuardList)
    {
        fprintf(fp, "/*--- Check For Previous Required Includes ---*/\n");
        while (GuardList)
        {
            char               *base;

            base = ExtractBase(GuardList->name);

            fprintf(fp, "#ifndef %s_H\n", base);
            fprintf(fp,
                    "#error \"Include %s.H before including this file\"\n",
                    base);
            fprintf(fp, "#endif /* %s_H */\n", base);
            fprintf(fp, "\n");
            GuardList = RemoveLink(GuardList, GuardList);

            free(base);
        }
    }
    if (SysList)
    {
        fprintf(fp, "/*--- System Header Files ---*/\n");
        while (SysList)
        {
            fprintf(fp, "#include <%s.h>\n", SysList->name);
            SysList = RemoveLink(SysList, SysList);
        }
        fprintf(fp, "\n");
    }
    if (RpeFileName)
    {
        fprintf(fp, "/*--- Error enumerations ---*/\n");
        fprintf(fp, "#include \"%s.rpe\"\n", RpeFileName);
        fprintf(fp, "\n");
    }
}

/*************************************************************************
 *
 * Print output file footer
 *
 ************************************************************************/
static void
PrintFooter(FILE * fp)
{
    if (OutFileName)
    {
        char               *base;

        base = ExtractBase(OutFileName);
        fprintf(fp, "#endif /* %s_H */\n", base);
    }
}

/*************************************************************************
 *
 * Main entry point
 *
 * The following command line options are supported
 *
 *     -v            Verbose mode
 *     -o<filename>  Output file
 *     -i<path>      Path to be added to include search path
 *
 * All other parameters are assumed to be input files
 *
 ************************************************************************/
int
main(int argc, char **argv)
{
    TyLink             *tmpLink = NULL;
    FILE               *outfp = NULL;
    FILE               *outlistfp = NULL;
    char               *name;
    TyLink             *link;
    FILE               *fp;

    ++argv, --argc;            /* skip over program name */

    while (argc--)
    {
        if (argv[0][0] == '-')
        {
            /* Option */
            switch (argv[0][1])
            {
                case 'i':
                case 'I':
                    /* Include path to look for files at */
                    AddPath(&(argv[0][2]));
                    break;

                case 'o':
                    /* Generated include file name */
                    outfp = fopen(&argv[0][2], "w");
                    if (!outfp)
                    {
                        fprintf(stderr,
                                "Unable to open output file <%s>\n",
                                &argv[0][2]);
                        if (outlistfp)
                        {
                            fclose(outlistfp);
                            outlistfp = NULL;
                        }
                        exit(1);
                    }
                    OutFileName = malloc(strlen(&argv[0][2]) + 1);
                    strcpy(OutFileName, &argv[0][2]);
                    break;

                case 's':
                    /* System file to include */
                    name = malloc(strlen(&argv[0][2]) + 1);
                    strcpy(name, &argv[0][2]);
                    SysList = NewLink(SysList, name, sizeof(TyLink));
                    break;

                case 'g':
                    /* Guard that this file is included first */
                    name = malloc(strlen(&argv[0][2]) + 1);
                    strcpy(name, &argv[0][2]);
                    GuardList = NewLink(GuardList, name, sizeof(TyLink));
                    break;

                case 'v':
                    /* Verbose enable */
                    Verbose = 1;
                    break;

                case 'l':
                    /* List file */
                    outlistfp = fopen(&argv[0][2], "w");
                    if (!outlistfp)
                    {
                        fprintf(stderr,
                                "Unable to open output list file <%s>\n",
                                &argv[0][2]);
                        if (outfp)
                        {
                            fclose(outfp);
                            outfp = NULL;
                        }
                        exit(1);
                    }
                    break;

                case 'x':
                    /* Exclude file - specifies file containing list of files not to add to process list */
                    fp = fopen(&argv[0][2], "r");
                    if (fp)
                    {
                        char                filename[80];

                        while (fgets(filename, 80, fp)
                               && strlen(filename) > 0)
                        {
                            char               *newName;
                            int                 i;

                            i = 0;
                            while (isprint(filename[i]))
                            {
                                i++;
                            }
                            filename[i] = 0;

                            newName = malloc(strlen(filename) + 1);
                            strcpy(newName, filename);

                            AddRejectFile(newName);
                        }

                        fclose(fp);
                    }
                    break;
                case 'r':
                    name = &(argv[0][2]);
                    link = FindLink((TyLink *) RejectFileList, name);
                    if (link)
                    {
                        RejectFileList =
                            (TyFileLink *) RemoveLink((TyLink *)
                                                      RejectFileList, link);
                    }
                    break;
                case 'p':
                    /* Name of rpe file */
                    RpeFileName = malloc(strlen(&argv[0][2]) + 1);
                    strcpy(RpeFileName, &argv[0][2]);
                    break;
                default:
                    /* Ignore unknown options */
                    break;
            }
            argv++;
        }
        else
        {
            /*
             * not an option therefore must be a filename.
             * Add it to the global list of filenames
             */
            fp = fopen(*argv, "r");
            if (fp)
            {
                name = malloc(strlen(*argv) + 1);
                strcpy(name, *argv);
                AddFile(name);
                fclose(fp);
            }
            else
            {
                fprintf(stderr, "Unable to Open: %s\n", *argv);
            }
            argv++;
        }
    }

    if (!outfp)
    {
        /* No output filename specified - use stdout */
        outfp = stdout;
    }

    if (!FileList)
    {
        fprintf(stderr, "No Files to process\n");
        exit(1);
    }

    /* Parse all files in global file list to extract dependancies.
     * This uses a simple FLEX lexical analyser for parsing
     * Note: We start at the end of the list and work backwards. This is
     * in order that new files can be added to the start of the list in the
     * usual manner and will be processed when encountered.
     */

    /* go to end of list */
    for (CurrentFile = FileList;
         CurrentFile->link.next;
         CurrentFile = (TyFileLink *) CurrentFile->link.next)
        ;

    for (; CurrentFile; CurrentFile = (TyFileLink *) CurrentFile->link.prev)
    {
#ifdef RWDEBUG
        printf("Processing : %s\n", CurrentFile->link.name);
#endif
        yyin = fopen(CurrentFile->link.name, "r");
        yylex();
        fclose(yyin);
    }

    if (Verbose)
    {
        /* Print the list of files and dependancies on stdout */
        PrintFileList(stdout);
    }

    /*
     * Sort the files into dependancy order. This is done by recursively
     * removing the files with zero dependancies until there are no files
     * remaining to be processed. If it is impossible to process the list
     * until it is empty then a cyclic dependancy must exist. This cannot
     * be resolved.
     */

    while ( (name = GetZeroDepFile()) )
    {
        SortedList = NewLink(SortedList, name, sizeof(TyLink));
    }
    if (FileList)
    {
        fprintf(stderr, "Possible Cyclic Dependancy\n");
        PrintFileList(stderr);
        exit(1);
    }

    if (Verbose)
    {
        fprintf(stdout, "Sorted List of headers\n\n");
        PrintLink(stdout, SortedList, 1);
    }

    /* Do we need to print to a file (for future exclusion)? */
    if (outlistfp)
    {
        PrintLink(outlistfp, SortedList, 0);
    }

    /* Print header on output file */
    PrintHeader(outfp);

    /*
     * List is currently sorted in reverse order so go to the end
     * and work back
     */

    for (tmpLink = SortedList; tmpLink->next; tmpLink = tmpLink->next)
        ;

    while (tmpLink)
    {
        FILE               *fp = fopen(tmpLink->name, "r");

        fprintf(outfp, "\n/*--- Automatically derived from: %s ---*/\n",
                tmpLink->name);

        ExtractPublic(fp, outfp);

        tmpLink = tmpLink->prev;
    }

    /* Print footer on output file */
    PrintFooter(outfp);

    /*
     * Cheesy exit without freeing allocated memory. The OS
     * will handle this.
     */

    /* ROB - But we are going to close the files we opened at least */
    if (outfp != stdout)
    {
        fclose(outfp);
    }
    if (outlistfp)
    {
        fclose(outlistfp);
    }

    /* ROB - And free up some memory that's easy */
    if (OutFileName)
    {
        free(OutFileName);
    }
    if (RpeFileName)
    {
        free(RpeFileName);
    }

    return (0);
}
