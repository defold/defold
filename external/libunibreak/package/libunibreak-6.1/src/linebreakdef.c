/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Line breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2008-2018 Wu Yongwei <wuyongwei at gmail dot com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 *    not claim that you wrote the original software.  If you use this
 *    software in a product, an acknowledgement in the product
 *    documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must
 *    not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source
 *    distribution.
 */

/**
 * @file    linebreakdef.c
 *
 * Definition of language-specific data.
 *
 * @author  Wu Yongwei
 */

#include "linebreak.h"
#include "linebreakdef.h"

/**
 * English-specifc data over the default Unicode rules.
 */
static const struct LineBreakProperties lb_prop_English[] = {
    { 0x2018, 0x2018, LBP_OP }, /* Left single quotation mark: opening */
    { 0x201C, 0x201C, LBP_OP }, /* Left double quotation mark: opening */
    { 0x201D, 0x201D, LBP_CL }, /* Right double quotation mark: closing */
    { 0, 0, LBP_Undefined }
};

/**
 * German-specifc data over the default Unicode rules.
 */
static const struct LineBreakProperties lb_prop_German[] = {
    { 0x00AB, 0x00AB, LBP_CL }, /* Left double angle quotation mark: closing */
    { 0x00BB, 0x00BB, LBP_OP }, /* Right double angle quotation mark: opening */
    { 0x2018, 0x2018, LBP_CL }, /* Left single quotation mark: closing */
    { 0x2019, 0x2019, LBP_GL }, /* Right single quotation mark: glue */
    { 0x201C, 0x201C, LBP_CL }, /* Left double quotation mark: closing */
    { 0x2039, 0x2039, LBP_CL }, /* Left single angle quotation mark: closing */
    { 0x203A, 0x203A, LBP_OP }, /* Right single angle quotation mark: opening */
    { 0, 0, LBP_Undefined }
};

/**
 * Spanish-specifc data over the default Unicode rules.
 */
static const struct LineBreakProperties lb_prop_Spanish[] = {
    { 0x00AB, 0x00AB, LBP_OP }, /* Left double angle quotation mark: opening */
    { 0x00BB, 0x00BB, LBP_CL }, /* Right double angle quotation mark: closing */
    { 0x2018, 0x2018, LBP_OP }, /* Left single quotation mark: opening */
    { 0x201C, 0x201C, LBP_OP }, /* Left double quotation mark: opening */
    { 0x201D, 0x201D, LBP_CL }, /* Right double quotation mark: closing */
    { 0x2039, 0x2039, LBP_OP }, /* Left single angle quotation mark: opening */
    { 0x203A, 0x203A, LBP_CL }, /* Right single angle quotation mark: closing */
    { 0, 0, LBP_Undefined }
};

/**
 * French-specifc data over the default Unicode rules.
 */
static const struct LineBreakProperties lb_prop_French[] = {
    { 0x00AB, 0x00AB, LBP_OP }, /* Left double angle quotation mark: opening */
    { 0x00BB, 0x00BB, LBP_CL }, /* Right double angle quotation mark: closing */
    { 0x2018, 0x2018, LBP_OP }, /* Left single quotation mark: opening */
    { 0x201C, 0x201C, LBP_OP }, /* Left double quotation mark: opening */
    { 0x201D, 0x201D, LBP_CL }, /* Right double quotation mark: closing */
    { 0x2039, 0x2039, LBP_OP }, /* Left single angle quotation mark: opening */
    { 0x203A, 0x203A, LBP_CL }, /* Right single angle quotation mark: closing */
    { 0, 0, LBP_Undefined }
};

/**
 * Russian-specifc data over the default Unicode rules.
 */
static const struct LineBreakProperties lb_prop_Russian[] = {
    { 0x00AB, 0x00AB, LBP_OP }, /* Left double angle quotation mark: opening */
    { 0x00BB, 0x00BB, LBP_CL }, /* Right double angle quotation mark: closing */
    { 0x201C, 0x201C, LBP_CL }, /* Left double quotation mark: closing */
    { 0, 0, LBP_Undefined }
};

/**
 * Chinese-specifc data over the default Unicode rules.
 */
static const struct LineBreakProperties lb_prop_Chinese[] = {
    { 0x2018, 0x2018, LBP_OP }, /* Left single quotation mark: opening */
    { 0x2019, 0x2019, LBP_CL }, /* Right single quotation mark: closing */
    { 0x201C, 0x201C, LBP_OP }, /* Left double quotation mark: opening */
    { 0x201D, 0x201D, LBP_CL }, /* Right double quotation mark: closing */
    { 0, 0, LBP_Undefined }
};

/**
 * Association data of language-specific line breaking properties with
 * language names.  This is the definition for the static data in this
 * file.  If you want more flexibility, or do not need the data here,
 * you may want to redefine \e lb_prop_lang_map in your C source file.
 */
const struct LineBreakPropertiesLang lb_prop_lang_map[] = {
    { "en", 2, lb_prop_English },
    { "de", 2, lb_prop_German },
    { "es", 2, lb_prop_Spanish },
    { "fr", 2, lb_prop_French },
    { "ru", 2, lb_prop_Russian },
    { "zh", 2, lb_prop_Chinese },
    { NULL, 0, NULL }
};
