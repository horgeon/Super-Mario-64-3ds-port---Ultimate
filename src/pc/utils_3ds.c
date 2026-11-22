#include <stdarg.h>

#include "pc/utils_3ds.h"

void error_box(bool wordWrap, bool fatal, const char *u8Message)
{
    errorConf err;
    memset(&err, 0, sizeof(err)); // errorInit doesn't zero the rest
    // We do not handle language.
    if (wordWrap) {
        errorInit(&err, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_DEFAULT);
    }
    else {
        errorInit(&err, ERROR_TEXT, CFG_LANGUAGE_DEFAULT);
    }

    if (fatal) {
        err.softwareReset = true;
    }
    errorText(&err, u8Message);

    errorDisp(&err);

    if (R_FAILED(err.returnCode)) {
        // applet couldn't launch, fall back to popup
        puts(u8Message);
    }
}

void error_box_format(bool wordWrap, bool fatal, const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    error_box(wordWrap, fatal, (const char *)buffer);
}