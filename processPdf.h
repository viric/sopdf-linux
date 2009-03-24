#pragma once

#define TRUE 1
#define FALSE 0

// soPdf file
typedef struct _soPdfFile
{
    char    fileName[1024];
    char    title[128];
    char    author[128];
    char    category[128];
    char    password[128];
    char    publisher[128];
    char    subject[128];

    // pdf document state
    pdf_xref        *xref;
    pdf_pagetree    *pageTree;
    pdf_page        *page;
    pdf_outline     *outline;

    // for editing
    fz_obj          *pagelist;
    fz_obj          *editobjs;

} soPdfFile, *soPdfFilePtr;


soPdfFile* 
initSoPdfFile(
    soPdfFile* pdfFile);

int 
processPdfFile(
    soPdfFile* inFile, 
    soPdfFile* outFile);

