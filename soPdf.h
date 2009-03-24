#pragma once

enum EMode
{
    Fit2xWidth,
    Fit2xHeight,
    FitWidth,
    FitHeight,
    SmartFitWidth,
    SmartFitHeight
};

//
// declarations for parameters
//
extern int     p_reverseLandscape;
extern int     p_cropWhiteSpace;
extern double   p_overlap;
extern enum EMode    p_mode;
extern int     p_proceedWithErrors;

#define SO_PDF_VER  "0.1 alpha Rev 12 linux"

int soPdfError(fz_error *error);
fz_error* soPdfErrorList(fz_error *error);
