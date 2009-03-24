#include <stdio.h>
#include "fitz.h"
#include "mupdf.h"
#include "soPdf.h"
#include "processPdf.h"

#define MAX_ERRORS_BEFORE_STOP      512
int g_errorCount = 0;
fz_error *g_errorList[MAX_ERRORS_BEFORE_STOP];

void
displayPageNumber(
    int pageNo,
    int first
    )
{
   printf("%d", pageNo);
}


fz_error*
soPdfErrorList(
    fz_error *error
    )
{
  int ctr;
    for (ctr = g_errorCount - 1; ctr >= 0; ctr--)
        soPdfError(g_errorList[ctr]);

    return fz_throw("\nToo many errors to proceed...");
}


soPdfFile*
initSoPdfFile(
    soPdfFile* pdfFile 
    )
{
    memset(pdfFile, 0, sizeof(soPdfFile));
    return pdfFile;
}


int
openPdfFile(
    soPdfFile* pdfFile
    )
{
    fz_error    *error;
    fz_obj      *obj;

    //
    // open pdf and load xref table
    error = pdf_newxref(&pdfFile->xref);
    if (error)
        return soPdfError(error);

    error = pdf_loadxref(pdfFile->xref, pdfFile->fileName);
    if (error)
        return soPdfError(error);

    //
    // Handle encrypted file
    error = pdf_decryptxref(pdfFile->xref);
    if (error)
        return soPdfError(error);

    if (pdfFile->xref->crypt)
    {
        int ret = pdf_setpassword(pdfFile->xref->crypt, 
            pdfFile->password);
        if (! ret)
            return soPdfError(fz_throw("invalid password"));
    }

    //
    // load the page tree and other objects
    error = pdf_loadpagetree(&pdfFile->pageTree, pdfFile->xref);
    if (error)
        return soPdfError(error);

    //
    // load meta information
    obj = fz_dictgets(pdfFile->xref->trailer, "Root");
    if (! obj)
        return soPdfError(fz_throw("mising root object"));

    error = pdf_loadindirect(&pdfFile->xref->root, pdfFile->xref, obj);
    if (error)
        return soPdfError(error);


    obj = fz_dictgets(pdfFile->xref->trailer, "Info");
    if (obj)
    {
        error = pdf_loadindirect(&pdfFile->xref->info, pdfFile->xref, obj);
        if (error)
            return soPdfError(error);
    }

    error = pdf_loadnametrees(pdfFile->xref);
    if (error)
        return soPdfError(error);

    error = pdf_loadoutline(&pdfFile->outline, pdfFile->xref);
    if (error)
        return soPdfError(error);

    return 0;
}

int
closePdfFile(
    soPdfFile* pdfFile
    )
{
    assert(pdfFile != NULL);

    if (pdfFile->pageTree)
    {
        pdf_droppagetree(pdfFile->pageTree);
        pdfFile->pageTree = NULL;
    }

    if (pdfFile->xref)
    {
        if (pdfFile->xref->store)
        {
            pdf_dropstore(pdfFile->xref->store);
            pdfFile->xref->store = NULL;
        }

        pdf_closexref(pdfFile->xref);
    }

    return 0;
}

int
newPdfFile(
    soPdfFile* pdfFile
    )
{
    fz_error    *error;

    assert(pdfFile != NULL);

    error = pdf_newxref(&pdfFile->xref);
    if (error)
        return soPdfError(error);

    error = pdf_initxref(pdfFile->xref);
    if (error)
        return soPdfError(error);

    error = fz_newarray(&pdfFile->pagelist, 100);
    if (error)
        return soPdfError(error);

    error = fz_newarray(&pdfFile->editobjs, 100);
    if (error)
        return soPdfError(error);

    return 0;
}


void
indent(int level)
{
    while(level--) putchar(' ');
}

void 
bbdump(fz_node *node, int level)
{
	fz_node *child;
    char*   kind;
    
    if (! node) return;

    switch(node->kind) 
    {
	case FZ_NOVER:      kind = "over"; break;
	case FZ_NMASK:      kind = "mask"; break;
	case FZ_NBLEND:     kind = "blend"; break;
	case FZ_NTRANSFORM: kind = "transform"; break;
	case FZ_NCOLOR:     kind = "color"; break;
	case FZ_NPATH:      kind = "path"; break;
	case FZ_NTEXT:      kind = "text"; break;
	case FZ_NIMAGE:     kind = "image"; break;
	case FZ_NSHADE:     kind = "shade"; break;
	case FZ_NLINK:      kind = "link"; break;
    default:            kind = "UNK"; break;
    }

    indent(level);
    printf("<%s : bbox = %.2f,%.2f - %.2f,%.2f>\n", 
        kind, node->bbox.x0, node->bbox.y0, 
        node->bbox.x1, node->bbox.y1);

    for (child = node->first; child; child = child->next)
		bbdump(child, level + 1);
}

#define PUT_INFO(_N, _F)    if (_F[0] != 0) { \
                                error = fz_newstring(&outObj, _F, strlen(_F)); \
                                if (error) return fz_rethrow(error, "unable to allocate"); \
                                error = fz_dictputs(outFile->xref->info, _N, outObj); \
                                if (error) return fz_rethrow(error, "unable to put : %s", _N); \
                                fz_dropobj(outObj); }
                                            
fz_error*
setPageInfo(
    soPdfFile*  inFile,
    soPdfFile*  outFile)
{
    fz_error    *error = NULL;
    fz_obj      *outObj;
    char        creator[] = "soPdf ver " SO_PDF_VER;
    char        szTime[128] = "";
    /*(
	struct tm   tmTemp;
    __time32_t  time;

    _time32(&time);
    _localtime32_s(&tmTemp, &time);
    strftime(szTime, sizeof(szTime), "%Y/%m/%d %H:%M", &tmTemp);
    */

    PUT_INFO("Title", outFile->title);
    PUT_INFO("Author", outFile->author);
    PUT_INFO("Category", outFile->category);
    PUT_INFO("Publisher", outFile->publisher);
    PUT_INFO("Subject", outFile->subject);
    PUT_INFO("Creator", creator);
    PUT_INFO("Producer", creator);
    PUT_INFO("CreationDate", szTime);
    PUT_INFO("ModDate", szTime);

    return error;
}

fz_error*
setPageMediaBox(
    pdf_xref*   pdfXRef,
    fz_obj*     pageObj,
    fz_rect     mediaBox
    )
{
    fz_error    *error;
    fz_obj      *objMedia;
    fz_irect    mRect;
    fz_obj      *objInt;

    // Delete the CropBox. This is done because we are reducing
    // the size of the media box and CropBox is of no use to us
    fz_dictdels(pageObj, "CropBox");
    //objMedia = fz_dictgets(pageObj, "CropBox");
    //if (objMedia == NULL) return fz_throw("no CropBox entry");
    //error = pdf_resolve(&objMedia, pdfXRef);
    //if (error) return fz_rethrow(error, "cannot resolve page bounds");
    //if (! fz_isarray(objMedia)) return fz_throw("cannot find page bounds");
    //fz_rect cRect = pdf_torect(objMedia);

    // Get the media box
    objMedia = fz_dictgets(pageObj, "MediaBox");
    if (objMedia == NULL) return fz_throw("no MediaBox entry");

    error = pdf_resolve(&objMedia, pdfXRef);
    if (error) return fz_rethrow(error, "cannot resolve page bounds");

    if (! fz_isarray(objMedia)) return fz_throw("cannot find page bounds");


    // We have the MediaBox array here
    mRect = fz_roundrect(mediaBox);

    error = fz_newint(&objInt, mRect.x0);
    if (error) return fz_rethrow(error, "cannot allocate int"); 
    fz_arrayput(objMedia, 0, objInt);
    fz_dropobj(objInt);

    error = fz_newint(&objInt, mRect.y0);
    if (error) return fz_rethrow(error, "cannot allocate int"); 
    fz_arrayput(objMedia, 1, objInt);
    fz_dropobj(objInt);

    error = fz_newint(&objInt, mRect.x1);
    if (error) return fz_rethrow(error, "cannot allocate int"); 
    fz_arrayput(objMedia, 2, objInt);
    fz_dropobj(objInt);

    error = fz_newint(&objInt, mRect.y1);
    if (error) return fz_rethrow(error, "cannot allocate int"); 
    fz_arrayput(objMedia, 3, objInt);
    fz_dropobj(objInt);

    return NULL;
}

fz_error*
setPageRotate(
    fz_obj*     pageObj,
    int         rotate
    )
{
    fz_obj      *objRotate;
    fz_error    *error = NULL;

    // Get the media box
    objRotate = fz_dictgets(pageObj, "Rotate");
    if (objRotate == NULL)
    {
        fz_obj  *objInt;
        // This entry does not have media box. Create a new
        // media box entry
        error = fz_newint(&objInt, rotate);
        if (error) return fz_rethrow(error, "cannot allocate rotate int");
        error = fz_dictputs(pageObj, "Rotate", objInt);
        if (error) return fz_rethrow(error, "cannot add rotate entry");
        fz_dropobj(objInt);
    }
    else
        objRotate->u.i = rotate;

    return error;
}


int
isInsideRect(
    fz_rect maxRect,
    fz_rect checkRect
    )
{
    if (fz_isinfiniterect(checkRect) || fz_isemptyrect(checkRect))
        return TRUE;

    return ((maxRect.x0 <= checkRect.x0) && 
            (maxRect.y0 <= checkRect.y0) &&
            (maxRect.x1 >= checkRect.x1) &&
            (maxRect.y1 >= checkRect.y1));
}

fz_rect
getContainingRect(
    fz_node *node, 
    fz_rect maxRect
    )
{
    fz_rect rect = fz_emptyrect;

    if (node)
    {
      fz_node *child;
        switch(node->kind)
        {
        case FZ_NTEXT:
        case FZ_NIMAGE:
        case FZ_NPATH:
            if (isInsideRect(maxRect, node->bbox))
                rect = fz_mergerects(rect, node->bbox);
            break;

        default:
            break;
        }

        // Recurse
        for (child = node->first; child; child = child->next)
            rect = fz_mergerects(rect, getContainingRect(child, maxRect));
    }

    return rect;
}


fz_error*
processErrorPage(
    soPdfFile* inFile,
    fz_obj *pageObj,
    int pageNo,
    fz_rect *bbRect,
    fz_error *error
    )
{
    // Wrap the error
    error = fz_rethrow(error, "Cannot process page %d", pageNo + 1);

    // If we are not supposed to proceed with error then it ends
    if (p_proceedWithErrors == FALSE)
            return error;

    // Save the error in the list
    if (g_errorCount >= MAX_ERRORS_BEFORE_STOP)
        return soPdfErrorList(error);
                
    g_errorList[g_errorCount++] = error;


    // Get the box for the page
    fz_obj *obj = fz_dictgets(pageObj, "CropBox");
    if (!obj)
        obj = fz_dictgets(pageObj, "MediaBox");

    error = pdf_resolve(&obj, inFile->xref);
    if (error)
        return fz_rethrow(error, "Cannot proceed with error %d", pageNo + 1);

    if (!fz_isarray(obj))
        return fz_throw("Cannot find page bounds : %d", pageNo + 1);

    fz_rect bbox = pdf_torect(obj);
    fz_rect mediaBox;

	mediaBox.x0 = MIN(bbox.x0, bbox.x1);
	mediaBox.y0 = MIN(bbox.y0, bbox.y1);
	mediaBox.x1 = MAX(bbox.x0, bbox.x1);
	mediaBox.y1 = MAX(bbox.y0, bbox.y1);
    float mbHeight = mediaBox.y1 - mediaBox.y0;


    switch(p_mode)
    {
    case FitHeight:
    case FitWidth:
        bbRect[0] = mediaBox;
        break;

    case Fit2xHeight:
    case Fit2xWidth:
        {
            float overlap = (mbHeight * (float)(p_overlap / 100)) / 2;
            bbRect[0] = mediaBox;
            bbRect[0].y0 = bbRect[0].y0 + (float)(0.5 * mbHeight) - overlap;
            bbRect[1] = mediaBox;
            bbRect[1].y1 = bbRect[1].y1 - (float)(0.5 * mbHeight) + overlap;
        }
        break;

    case SmartFitHeight:
    case SmartFitWidth:
    default:
        return fz_throw("Mode(%d) not yet implemented.", p_mode);
        break;
    }

    return NULL;
}


fz_error*
processPage(
    soPdfFile* inFile,
    int pageNo,
    fz_rect *bbRect,
    int rectCount
    )
{
    fz_error    *error;
    fz_obj      *pageRef;
    pdf_page    *pdfPage;
    fz_rect     contentBox;
    fz_rect     mediaBox;
    int ctr;


    // Initialize 
    for (ctr = 0; ctr < rectCount; ctr++)
        bbRect[ctr] = fz_emptyrect;

    // Get the page reference and load the page contents
    pageRef = pdf_getpageobject(inFile->pageTree, pageNo);
    error = pdf_loadpage(&pdfPage, inFile->xref, pageRef);
    if (error != NULL)
    {
        // Ideally pdf_loadpage should render all the pages
        // and this should never happen
        return processErrorPage(inFile, pageRef, pageNo, bbRect, error);
    }

    // Get the bounding box for the page
    mediaBox = pdfPage->mediabox;
    float mbHeight = mediaBox.y1 - mediaBox.y0;
    
    // calculate the bounding box for all the elements in the page
    contentBox = fz_boundnode(pdfPage->tree->root, fz_identity());
    float cbHeight = contentBox.y1 - contentBox.y0;

    // If there is nothing on the page we return nothing.
    // should we return an empty page instead ???
    if (fz_isemptyrect(contentBox))
        goto Cleanup;

    // if contentBox is bigger than mediaBox then there are some
    // elements that should not be display and hence we reset the
    // content box to media box
    if ((cbHeight > mbHeight) || ((contentBox.x1 - contentBox.x0) > (mediaBox.x1 - mediaBox.x0)))
    {
        // Calculate the new content box based on the content that is 
        // inside the the media box and recalculate cbHeight
        contentBox = getContainingRect(pdfPage->tree->root, mediaBox);
        cbHeight = contentBox.y1 - contentBox.y0;
    }


#ifdef _blahblah
    printf("-->Page %d\n", pageNo);
    bbdump(pdfPage->tree->root, 1);
#endif

    // The rotation takes place when we insert the page into destination
    // If only the mupdf renderer could give accurate values of the 
    // bounding box of all the elements in a page, the splitting would
    // be so much more easier without overlapping, cutting text, etc
    switch(p_mode)
    {
    case FitHeight:
    case FitWidth:
        bbRect[0] = contentBox;
        goto Cleanup;

    case Fit2xHeight:
    case Fit2xWidth:
        // Let the processing happen.
        break;

    case SmartFitHeight:
    case SmartFitWidth:
    default:
        return fz_throw("Mode(%d) not yet implemented.", p_mode);
        break;
    }

    // If the contentBox is 60% of mediaBox then do not split 
    if (((cbHeight / mbHeight) * 100) <= 55)
    {
        bbRect[0] = contentBox;
        goto Cleanup;
    }

    // Get the first split contents from top. The box we specify is 
    // top 55% (bottom + 45) of the contents
    bbRect[0] = contentBox;
    bbRect[0].y0 = bbRect[0].y0 + (float)(0.45 * cbHeight);
    bbRect[0] = getContainingRect(pdfPage->tree->root, bbRect[0]);

    // Check if the contents we got in first split is more than 40%
    // of the total contents
    float bbRect0Height = bbRect[0].y1 - bbRect[0].y0;
    if (((bbRect0Height / cbHeight) * 100) >= 40)
    {
        // The content is more than 40%. Put the rest of the 
        // content in the second split and exit
        bbRect[1] = contentBox;
        bbRect[1].y1 = bbRect[1].y1 - bbRect0Height;

        // Adjust the split box height by X points to make sure
        // we get everything and dont have annoying tail cuts
        bbRect[0].y0 -= 2;
        goto Cleanup;
    }

    // Since the contents we got in first split is less than 40%
    // of the total contents, split the content in half with overlap
    float overlap = (cbHeight * (float)(p_overlap / 100)) / 2;
    bbRect[0] = contentBox;
    bbRect[0].y0 = bbRect[0].y0 + (float)(0.5 * cbHeight) - overlap;
    bbRect[1] = contentBox;
    bbRect[1].y1 = bbRect[1].y1 - (float)(0.5 * cbHeight) + overlap;


Cleanup:

    // This function can overflow the stack when the pdf page
    // to be rendered is very complex. I had to increase the 
    // stack size reserved for exe using compiler option
    pdf_droppage(pdfPage);

    return error;
}

int
copyPdfFile(
    soPdfFile* inFile,
    soPdfFile* outFile
    )
{
    fz_error    *error;
    int         pageTreeNum, pageTreeGen;

    assert(inFile != NULL);
    assert(outFile != NULL);

    //
    // Process every page in the source file
    //
    {
      int pageNo;
        printf("\nProcessing input page : ");
        for (pageNo = 0; pageNo < pdf_getpagecount(inFile->pageTree); pageNo++)
        {
            displayPageNumber(pageNo + 1, !pageNo);

            // Get the page object from the source
            fz_obj  *pageRef = inFile->pageTree->pref[pageNo];
            fz_obj  *pageObj = pdf_getpageobject(inFile->pageTree, pageNo);

            //
            // Process the page. Each page can be split into up-to 3 pages
            //
            fz_rect    bbRect[3];
            error = processPage(inFile, pageNo, bbRect, 3);
            if (error)
                return soPdfError(error);

            int ctr;
            for (ctr = 0; ctr < 3; ctr++)
            {
                // Check if this was a blank page
                if (fz_isemptyrect(bbRect[ctr]))
                    break;

                //
                // copy the source page dictionary entry. The way this is done is basically
                // by making a copy of the page dict object in the source file, and adding
                // the copy in the source file. Then the copied page dict object is 
                // referenced and added to the destination file.
                //
                // This convoluted procedure is done because the copy is done by pdf_transplant
                // function that accepts a source and destination. Whatever is referenced by
                // destination object is deep copied
                //
                

                // allocate an object id and generation id in source file
                //
                // There is a bug in mupdf where the object allocation returns
                // 0 oid and 0 gid when the input pdf file has iref stream
                // so to work around the issue, we wrap the pdf_allocojbect
                // in a for loop 10 times to get the number
                //
                int sNum, sGen, tries;

                for (tries = 0; tries < 10; tries++)
                {
                    error = pdf_allocobject(inFile->xref, &sNum, &sGen);
                    if (error)
                        return soPdfError(error);

                    // If sNum is non zero then the allocation was successful
                    if (sNum != 0)
                        break;  
                    pdf_updateobject(inFile->xref, sNum, sGen, pageObj);
                }

                // If we didn't succeed even after 10 tries then this file 
                // is not going to work.
                if (tries >= 10)
                    return soPdfError(fz_throw("cannot allocate object because of mupdf bug"));

                // make a deep copy of the original page dict
                fz_obj  *pageObj2;
                error = fz_deepcopydict(&pageObj2, pageObj);
                if (error)
                    return soPdfError(error);

                // update the source file with the duplicate page object
                pdf_updateobject(inFile->xref, sNum, sGen, pageObj2);

                fz_dropobj(pageObj2);

                // create an indirect reference to the page object
                fz_obj  *pageRef2;
                error = fz_newindirect(&pageRef2, sNum, sGen);
                if (error)
                    return soPdfError(error);

                // delete the parent dictionary entry
                // Do we need to delete any other dictionary entry 
                // like annot, tabs, metadata, etc
                fz_dictdels(pageObj2, "Parent");

                // Set the media box
                setPageMediaBox(inFile->xref, pageObj2, bbRect[ctr]);

                // Set the rotation based on input
                switch(p_mode)
                {
                    // no rotation if fit height
                case FitHeight:
                case Fit2xHeight:
                    break;

                    // rotate -90 deg if fit width
                case Fit2xWidth:
                case FitWidth:
                    setPageRotate(pageObj2, p_reverseLandscape ? 90 : -90);
                    break;

                case SmartFitHeight:
                case SmartFitWidth:
                default:
                    return soPdfError(fz_throw("Mode(%d) not yet implemented.", p_mode));
                    break;
                }


                // push the indirect reference to the destination list for copy by pdf_transplant
                error = fz_arraypush(outFile->editobjs, pageRef2);
                if (error)
                    return soPdfError(error);
            }
        }
    }

    // flush the objects into destination from source
    {
        fz_obj      *results;
        int         outPages;

        printf("\nCopying output page : ");
        error = pdf_transplant(outFile->xref, inFile->xref, &results, outFile->editobjs);
        if (error)
            return soPdfError(error);

        outPages = fz_arraylen(results);
        int ctr;
        for (ctr = 0; ctr < outPages; ctr++)
        {
            displayPageNumber(ctr + 1, !ctr);
            error = fz_arraypush(outFile->pagelist, fz_arrayget(results, 
                p_reverseLandscape ? outPages - 1 - ctr : ctr));
            if (error)
                return soPdfError(error);
        }

        fz_dropobj(results);
    }

    // flush page tree

    // Create page tree and add back-links
    {
        fz_obj  *pageTreeObj;
        fz_obj  *pageTreeRef;

        // allocate a new object in out file for pageTree object
        error = pdf_allocobject(outFile->xref, &pageTreeNum, &pageTreeGen);
        if (error)
            return soPdfError(error);

        // Create a page tree object
        error = fz_packobj(&pageTreeObj, "<</Type/Pages/Count %i/Kids %o>>",
            fz_arraylen(outFile->pagelist), outFile->pagelist);
        if (error)
            return soPdfError(error);

        // Update the xref entry with the pageTree object
        pdf_updateobject(outFile->xref, pageTreeNum, pageTreeGen, pageTreeObj);

        fz_dropobj(pageTreeObj);

        // Create a reference to the pageTree object
        error = fz_newindirect(&pageTreeRef, pageTreeNum, pageTreeGen);
        if (error)
            return soPdfError(error);

        //
        // For every page in the output file, update the parent entry
        //
        int ctr;
        for (ctr = 0; ctr < fz_arraylen(outFile->pagelist); ctr++)
        {
            fz_obj  *pageObj;

            int num = fz_tonum(fz_arrayget(outFile->pagelist, ctr));
            int gen = fz_togen(fz_arrayget(outFile->pagelist, ctr));

            // Get the page object from xreft
            error = pdf_loadobject(&pageObj, outFile->xref, num, gen);
            if (error)
                return soPdfError(error);

            // Update the parent entry in the page dictionary
            error = fz_dictputs(pageObj, "Parent", pageTreeRef);
            if (error)
                return soPdfError(error);

            // Update the entry with the updated page object
            pdf_updateobject(outFile->xref, num, gen, pageObj);

            fz_dropobj(pageObj);
        }
    }

    // Create catalog and root entries
    {
        fz_obj  *catObj, *infoObj;
        int     rootNum, rootGen;
        int     infoNum, infoGen;

        //
        // Copy the info catalog to the destination

        // alloc an object id and gen id in destination file
        error = pdf_allocobject(outFile->xref, &infoNum, &infoGen);
        if (error)
            return soPdfError(error);

        // make a deep copy of the original page dict
        error = fz_deepcopydict(&infoObj, inFile->xref->info);
        if (error)
            return soPdfError(error);

        // update the dest file with object
        pdf_updateobject(outFile->xref, infoNum, infoGen, infoObj);
        outFile->xref->info = infoObj;

        fz_dropobj(infoObj);

        //
        // root/catalog object creation
        error = pdf_allocobject(outFile->xref, &rootNum, &rootGen);
        if (error)
            return soPdfError(error);

        error = fz_packobj(&catObj, "<</Type/Catalog /Pages %r>>", pageTreeNum, pageTreeGen);
        if (error)
            return soPdfError(error);

        pdf_updateobject(outFile->xref, rootNum, rootGen, catObj);

        fz_dropobj(catObj);

        // Create trailer
        error = fz_packobj(&outFile->xref->trailer, "<</Root %r /Info %r>>", 
            rootNum, rootGen, infoNum, infoGen);
        if (error)
            return soPdfError(error);

    }

    // Update the info in the target file and save the xref
    printf("\nSaving.\n");
    error = setPageInfo(inFile, outFile);
    if (error)
        return soPdfError(error);

    error = pdf_savexref(outFile->xref, outFile->fileName, NULL);
    if (error)
        return soPdfError(error);

    if (g_errorCount != 0)
    {
        printf("\nFollowing issues encounted were ignored.\n\n");
        int ctr;
        for (ctr = g_errorCount - 1; ctr >= 0; ctr--)
            soPdfError(g_errorList[ctr]);
    }
    printf("\nSaved.\n");

    return 0;
}


int 
processPdfFile(
    soPdfFile* inFile,
    soPdfFile* outFile
    )
{
    int retCode = 0;

    assert(inFile != NULL);
    assert(outFile != NULL);

    // Open the input file
    retCode = openPdfFile(inFile);
    if (retCode != 0)
        goto Cleanup;

    // Create an output file
    retCode = newPdfFile(outFile);
    if (retCode != 0)
        goto Cleanup;
    
    // Copy from source to destination
    retCode = copyPdfFile(inFile, outFile);
    if (retCode != 0)
        goto Cleanup;

Cleanup:

    closePdfFile(inFile);
    closePdfFile(outFile);

    return retCode;
}
