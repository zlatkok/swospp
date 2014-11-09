#pragma once

#include "xmltree.h"

typedef enum XmlSymbol {
    XML_SYM_TAG_OPEN,           /* <    */
    XML_SYM_TAG_OPEN_END_TAG,   /* </   */
    XML_SYM_TAG_CLOSE,          /* >    */
    XML_SYM_TAG_CLOSE_SOLO_TAG, /* />   */
    XML_SYM_EQ,                 /* =    */
    XML_SYM_TEXT,
    XML_SYM_EOF,
    XML_SYM_MAX,
} XmlSymbol;

typedef bool (*XmlSymbolProcessingFunction)(XmlSymbol sym, char *buf, int bufSize);

bool LoadXmlFile(XmlNode **root, const char *fileName, XmlSymbolProcessingFunction symProc);
bool SaveXmlFile(XmlNode *root, const char *fileName, bool checkIfNeeded);