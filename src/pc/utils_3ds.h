#pragma once

#include <3ds.h>
#include <string.h>
#include <stdio.h>

/*
 * Display an error box with a specific message.
 * @param u8Message The message to display (UTF-8 encoded).
 */
void error_box(bool wordWrap, bool fatal, const char *u8Message);
/*
 * Display an error box with a formatted message (like printf).
 * @param format The format string (printf-style).
 * @param ... The arguments for the format string.
 */
void error_box_format(bool wordWrap, bool fatal, const char *format, ...);
