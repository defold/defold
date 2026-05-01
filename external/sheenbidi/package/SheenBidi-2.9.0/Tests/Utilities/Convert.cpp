/*
 * Copyright (C) 2015-2025 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <SheenBidi/SBBidiType.h>
#include <SheenBidi/SBGeneralCategory.h>
#include <SheenBidi/SBScript.h>

#include "Convert.h"

using namespace std;
using namespace SheenBidi::Utilities;

static unordered_map<string, uint32_t> MAP_BIDI_TYPE_TO_CODE_POINT = {
    {"AL", 0x0627},
    {"AN", 0x0600},
    {"B", 0x0085},
    {"BN", 0x001B},
    {"CS", 0x002E},
    {"EN", 0x0030},
    {"ES", 0x002B},
    {"ET", 0x0025},
    {"FSI", 0x2068},
    {"L", 0x0041},
    {"LRE", 0x202A},
    {"LRI", 0x2066},
    {"LRO", 0x202D},
    {"NSM", 0x0614},
    {"ON", 0x0028},
    {"PDF", 0x202C},
    {"PDI", 0x2069},
    {"R", 0x05D0},
    {"RLE", 0x202B},
    {"RLI", 0x2067},
    {"RLO", 0x202E},
    {"S", 0x0009},
    {"WS", 0x0020}
};

static unordered_map<SBBidiType, string> MAP_BIDI_TYPE_TO_STRING = {
    {SBBidiTypeNil, "Nil"},
    {SBBidiTypeL, "L"},
    {SBBidiTypeR, "R"},
    {SBBidiTypeAL, "AL"},
    {SBBidiTypeEN, "EN"},
    {SBBidiTypeES, "ES"},
    {SBBidiTypeET, "ET"},
    {SBBidiTypeAN, "AN"},
    {SBBidiTypeCS, "CS"},
    {SBBidiTypeNSM, "NSM"},
    {SBBidiTypeBN, "BN"},
    {SBBidiTypeB, "B"},
    {SBBidiTypeS, "S"},
    {SBBidiTypeWS, "WS"},
    {SBBidiTypeON, "ON"},
    {SBBidiTypeLRE, "LRE"},
    {SBBidiTypeRLE, "RLE"},
    {SBBidiTypeLRO, "LRO"},
    {SBBidiTypeRLO, "RLO"},
    {SBBidiTypePDF, "PDF"},
    {SBBidiTypeLRI, "LRI"},
    {SBBidiTypeRLI, "RLI"},
    {SBBidiTypeFSI, "FSI"},
    {SBBidiTypePDI, "PDI"}
};

static unordered_map<SBGeneralCategory, string> MAP_GENERAL_CATEGORY_TO_STRING = {
    {SBGeneralCategoryLU, "Lu"},
    {SBGeneralCategoryLL, "Ll"},
    {SBGeneralCategoryLT, "Lt"},
    {SBGeneralCategoryLM, "Lm"},
    {SBGeneralCategoryLO, "Lo"},
    {SBGeneralCategoryMN, "Mn"},
    {SBGeneralCategoryMC, "Mc"},
    {SBGeneralCategoryME, "Me"},
    {SBGeneralCategoryND, "Nd"},
    {SBGeneralCategoryNL, "Nl"},
    {SBGeneralCategoryNO, "No"},
    {SBGeneralCategoryPC, "Pc"},
    {SBGeneralCategoryPD, "Pd"},
    {SBGeneralCategoryPS, "Ps"},
    {SBGeneralCategoryPE, "Pe"},
    {SBGeneralCategoryPI, "Pi"},
    {SBGeneralCategoryPF, "Pf"},
    {SBGeneralCategoryPO, "Po"},
    {SBGeneralCategorySM, "Sm"},
    {SBGeneralCategorySC, "Sc"},
    {SBGeneralCategorySK, "Sk"},
    {SBGeneralCategorySO, "So"},
    {SBGeneralCategoryZS, "Zs"},
    {SBGeneralCategoryZL, "Zl"},
    {SBGeneralCategoryZP, "Zp"},
    {SBGeneralCategoryCC, "Cc"},
    {SBGeneralCategoryCF, "Cf"},
    {SBGeneralCategoryCS, "Cs"},
    {SBGeneralCategoryCO, "Co"},
    {SBGeneralCategoryCN, "Cn"}
};

static unordered_map<SBScript, string> MAP_SCRIPT_TO_STRING = {
    {SBScriptZINH, "Zinh"},
    {SBScriptZYYY, "Zyyy"},
    {SBScriptZZZZ, "Zzzz"},
    {SBScriptARAB, "Arab"},
    {SBScriptARMN, "Armn"},
    {SBScriptBENG, "Beng"},
    {SBScriptBOPO, "Bopo"},
    {SBScriptCYRL, "Cyrl"},
    {SBScriptDEVA, "Deva"},
    {SBScriptGEOR, "Geor"},
    {SBScriptGREK, "Grek"},
    {SBScriptGUJR, "Gujr"},
    {SBScriptGURU, "Guru"},
    {SBScriptHANG, "Hang"},
    {SBScriptHANI, "Hani"},
    {SBScriptHEBR, "Hebr"},
    {SBScriptHIRA, "Hira"},
    {SBScriptKANA, "Kana"},
    {SBScriptKNDA, "Knda"},
    {SBScriptLAOO, "Laoo"},
    {SBScriptLATN, "Latn"},
    {SBScriptMLYM, "Mlym"},
    {SBScriptORYA, "Orya"},
    {SBScriptTAML, "Taml"},
    {SBScriptTELU, "Telu"},
    {SBScriptTHAI, "Thai"},
    {SBScriptTIBT, "Tibt"},
    {SBScriptBRAI, "Brai"},
    {SBScriptCANS, "Cans"},
    {SBScriptCHER, "Cher"},
    {SBScriptETHI, "Ethi"},
    {SBScriptKHMR, "Khmr"},
    {SBScriptMONG, "Mong"},
    {SBScriptMYMR, "Mymr"},
    {SBScriptOGAM, "Ogam"},
    {SBScriptRUNR, "Runr"},
    {SBScriptSINH, "Sinh"},
    {SBScriptSYRC, "Syrc"},
    {SBScriptTHAA, "Thaa"},
    {SBScriptYIII, "Yiii"},
    {SBScriptDSRT, "Dsrt"},
    {SBScriptGOTH, "Goth"},
    {SBScriptITAL, "Ital"},
    {SBScriptBUHD, "Buhd"},
    {SBScriptHANO, "Hano"},
    {SBScriptTAGB, "Tagb"},
    {SBScriptTGLG, "Tglg"},
    {SBScriptCPRT, "Cprt"},
    {SBScriptLIMB, "Limb"},
    {SBScriptLINB, "Linb"},
    {SBScriptOSMA, "Osma"},
    {SBScriptSHAW, "Shaw"},
    {SBScriptTALE, "Tale"},
    {SBScriptUGAR, "Ugar"},
    {SBScriptBUGI, "Bugi"},
    {SBScriptCOPT, "Copt"},
    {SBScriptGLAG, "Glag"},
    {SBScriptKHAR, "Khar"},
    {SBScriptSYLO, "Sylo"},
    {SBScriptTALU, "Talu"},
    {SBScriptTFNG, "Tfng"},
    {SBScriptXPEO, "Xpeo"},
    {SBScriptBALI, "Bali"},
    {SBScriptNKOO, "Nkoo"},
    {SBScriptPHAG, "Phag"},
    {SBScriptPHNX, "Phnx"},
    {SBScriptXSUX, "Xsux"},
    {SBScriptCARI, "Cari"},
    {SBScriptCHAM, "Cham"},
    {SBScriptKALI, "Kali"},
    {SBScriptLEPC, "Lepc"},
    {SBScriptLYCI, "Lyci"},
    {SBScriptLYDI, "Lydi"},
    {SBScriptOLCK, "Olck"},
    {SBScriptRJNG, "Rjng"},
    {SBScriptSAUR, "Saur"},
    {SBScriptSUND, "Sund"},
    {SBScriptVAII, "Vaii"},
    {SBScriptARMI, "Armi"},
    {SBScriptAVST, "Avst"},
    {SBScriptBAMU, "Bamu"},
    {SBScriptEGYP, "Egyp"},
    {SBScriptJAVA, "Java"},
    {SBScriptKTHI, "Kthi"},
    {SBScriptLANA, "Lana"},
    {SBScriptLISU, "Lisu"},
    {SBScriptMTEI, "Mtei"},
    {SBScriptORKH, "Orkh"},
    {SBScriptPHLI, "Phli"},
    {SBScriptPRTI, "Prti"},
    {SBScriptSAMR, "Samr"},
    {SBScriptSARB, "Sarb"},
    {SBScriptTAVT, "Tavt"},
    {SBScriptBATK, "Batk"},
    {SBScriptBRAH, "Brah"},
    {SBScriptMAND, "Mand"},
    {SBScriptCAKM, "Cakm"},
    {SBScriptMERC, "Merc"},
    {SBScriptMERO, "Mero"},
    {SBScriptPLRD, "Plrd"},
    {SBScriptSHRD, "Shrd"},
    {SBScriptSORA, "Sora"},
    {SBScriptTAKR, "Takr"},
    {SBScriptAGHB, "Aghb"},
    {SBScriptBASS, "Bass"},
    {SBScriptDUPL, "Dupl"},
    {SBScriptELBA, "Elba"},
    {SBScriptGRAN, "Gran"},
    {SBScriptHMNG, "Hmng"},
    {SBScriptKHOJ, "Khoj"},
    {SBScriptLINA, "Lina"},
    {SBScriptMAHJ, "Mahj"},
    {SBScriptMANI, "Mani"},
    {SBScriptMEND, "Mend"},
    {SBScriptMODI, "Modi"},
    {SBScriptMROO, "Mroo"},
    {SBScriptNARB, "Narb"},
    {SBScriptNBAT, "Nbat"},
    {SBScriptPALM, "Palm"},
    {SBScriptPAUC, "Pauc"},
    {SBScriptPERM, "Perm"},
    {SBScriptPHLP, "Phlp"},
    {SBScriptSIDD, "Sidd"},
    {SBScriptSIND, "Sind"},
    {SBScriptTIRH, "Tirh"},
    {SBScriptWARA, "Wara"},
    {SBScriptAHOM, "Ahom"},
    {SBScriptHATR, "Hatr"},
    {SBScriptHLUW, "Hluw"},
    {SBScriptHUNG, "Hung"},
    {SBScriptMULT, "Mult"},
    {SBScriptSGNW, "Sgnw"},
    {SBScriptADLM, "Adlm"},
    {SBScriptBHKS, "Bhks"},
    {SBScriptMARC, "Marc"},
    {SBScriptNEWA, "Newa"},
    {SBScriptOSGE, "Osge"},
    {SBScriptTANG, "Tang"},
    {SBScriptGONM, "Gonm"},
    {SBScriptNSHU, "Nshu"},
    {SBScriptSOYO, "Soyo"},
    {SBScriptZANB, "Zanb"},
    {SBScriptDOGR, "Dogr"},
    {SBScriptGONG, "Gong"},
    {SBScriptMAKA, "Maka"},
    {SBScriptMEDF, "Medf"},
    {SBScriptROHG, "Rohg"},
    {SBScriptSOGD, "Sogd"},
    {SBScriptSOGO, "Sogo"},
    {SBScriptELYM, "Elym"},
    {SBScriptHMNP, "Hmnp"},
    {SBScriptNAND, "Nand"},
    {SBScriptWCHO, "Wcho"},
    {SBScriptCHRS, "Chrs"},
    {SBScriptDIAK, "Diak"},
    {SBScriptKITS, "Kits"},
    {SBScriptYEZI, "Yezi"},
    {SBScriptCPMN, "Cpmn"},
    {SBScriptOUGR, "Ougr"},
    {SBScriptTNSA, "Tnsa"},
    {SBScriptTOTO, "Toto"},
    {SBScriptVITH, "Vith"},
    {SBScriptKAWI, "Kawi"},
    {SBScriptNAGM, "Nagm"},
    {SBScriptGARA, "Gara"},
    {SBScriptGUKH, "Gukh"},
    {SBScriptKRAI, "Krai"},
    {SBScriptONAO, "Onao"},
    {SBScriptSUNU, "Sunu"},
    {SBScriptTODR, "Todr"},
    {SBScriptTUTG, "Tutg"}
};

const string &Convert::bidiTypeToString(SBBidiType bidiType) {
    return MAP_BIDI_TYPE_TO_STRING[bidiType];
}

const string &Convert::generalCategoryToString(SBGeneralCategory generalCategory) {
    return MAP_GENERAL_CATEGORY_TO_STRING[generalCategory];
}

const string &Convert::scriptToString(SBScript script) {
    return MAP_SCRIPT_TO_STRING[script];
}

uint32_t Convert::stringToTag(const string &str) {
    if (str.length() != 4) {
        throw invalid_argument("The length of tag string is not equal to four");
    }

    uint32_t a = static_cast<unsigned char>(str[0]);
    uint32_t b = static_cast<unsigned char>(str[1]);
    uint32_t c = static_cast<unsigned char>(str[2]);
    uint32_t d = static_cast<unsigned char>(str[3]);

    return (a << 24) | (b << 16) | (c << 8) | d;
}

uint32_t Convert::toCodePoint(const string &bidiType) {
    return MAP_BIDI_TYPE_TO_CODE_POINT[bidiType];
}

void Convert::toLowercase(string &str) {
    transform(str.begin(), str.end(), str.begin(),
              [](auto c) { return tolower(c); });
}
