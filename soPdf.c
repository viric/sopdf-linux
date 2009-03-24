// soPdf.cpp : Defines the entry point for the console application.
//

#include <getopt.h>
#include "fitz.h"
#include "mupdf.h"
#include "soPdf.h"
#include "processPdf.h"

//
// Definitions
//
int    p_proceedWithErrors = FALSE;
int    p_reverseLandscape = FALSE;
int    p_cropWhiteSpace = TRUE;
double  p_overlap = 2;
enum EMode   p_mode = Fit2xWidth;

// Pdf files
soPdfFile   inPdfFile;
soPdfFile   outPdfFile;




int
soPdfError(
    fz_error *error
    )
{
    int ctr = 0;
    fz_error *e = error;
    while (error)
    {
        printf("%*sError: %s(%d) : %s() - %s\n", 
            ctr++, "", error->file, error->line, 
            error->func, error->msg);

        error = error->cause;
    }

    fz_droperror(e);
    return 1;
}


int
soPdfUsage(void)
{
    fprintf(stderr,
        "about: soPdf\n"
        "   author: Navin Pai, soPdf ver " SO_PDF_VER "\n"
        "usage: \n"
        "   soPdf -i file_name [options]\n"
        "   -i file_name    input file name\n"
        "   -p password     password for input file\n"
        "   -o file_name    output file name\n"
        "   -w              turn off white space cropping\n"
        "                       default is on\n"
        "   -m nn           mode of operation\n"
        "                       0 = fit 2xWidth *\n"
        "                       1 = fit 2xHeight\n"
        "                       2 = fit Width\n"
        "                       3 = fit Height\n"
        "                       4 = smart fit Width\n"
        "                       5 = smart fit Height\n"
        "   -v nn           overlap percentage\n"
        "                       nn = 2 percent overlap *\n"
        "   -t title        set the file title\n"
        "   -a author       set the file author\n"
        "   -b publisher    set the publisher\n"
        "   -c category     set the category\n"
        "   -s subject      set the subject\n"
        "   -e              proceed with errors\n"
        "   -r              reverse landscape\n"
        "\n"
        "   * = default values\n");

    return 1;
}

int 
main(int argc, char* argv[])
{
    int c;

    if (argc < 2)
        return soPdfUsage();

    initSoPdfFile(&inPdfFile);
    initSoPdfFile(&outPdfFile);


    // parse the command line arguments
    while ((c = getopt(argc, argv, "i:p:o:t:a:b:c:s:ewm:v:r")) != -1)
    {
        switch(c)
        {
        case 'i':   strcpy(inPdfFile.fileName, optarg);   break;
        case 'p':   strcpy(inPdfFile.password, optarg);   break;
        case 'o':   strcpy(outPdfFile.fileName, optarg);  break;

        case 't':   strcpy(outPdfFile.title, optarg);     break;
        case 'a':   strcpy(outPdfFile.author, optarg);    break;
        case 'b':   strcpy(outPdfFile.publisher, optarg); break;
        case 'c':   strcpy(outPdfFile.category, optarg);  break;
        case 's':   strcpy(outPdfFile.subject, optarg);   break;

        case 'e':   p_proceedWithErrors = TRUE;             break;
        case 'w':   p_cropWhiteSpace = FALSE;               break;
        case 'm':   p_mode = atoi(optarg);           break;
        case 'v':   p_overlap = atof(optarg);               break;
        case 'r':   p_reverseLandscape = TRUE;              break;
        default:    return soPdfUsage();                    break;
        }
    }
    
    // Check if input file is specified at the minimum
    if (inPdfFile.fileName[0] == 0)
        return soPdfUsage();

    // If output file is not specified
    if (outPdfFile.fileName[0] == 0)
    {
        strcpy(outPdfFile.fileName, inPdfFile.fileName);
        strcat(outPdfFile.fileName, "out.pdf");
    }

    // The reverseLandscape works with landscape mode only
    if (! ((p_mode == FitWidth) || 
           (p_mode == Fit2xWidth) || 
           (p_mode == SmartFitWidth)))
        p_reverseLandscape = FALSE;

    printf("\nsoPdf ver " SO_PDF_VER "\n");
    printf("\tA program to reformat pdf file for sony reader\n");
    printf("\nInput : %s\n", inPdfFile.fileName);
    printf("Output: %s\n\n", outPdfFile.fileName);

    return processPdfFile(&inPdfFile, &outPdfFile);
}

