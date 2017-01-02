#pragma once

enum XmlNodeType {
    XML_CHAR,
    XML_SHORT,
    XML_INT,
    XML_STRING,
    XML_ARRAY,
    XML_EMPTY,
    XML_FUNC,
    XML_TYPE_MAX,
};

typedef void *(*XmlValueFunction)();

union XmlValue {
    signed char *charVal;
    signed short *shortVal;
    signed int *intVal;
    char *ptr;
    XmlValueFunction func;
};

struct XmlAttribute {
    char *name;
    size_t nameLength;
    uint32_t nameHash;
    char *value;
    size_t valueLength;
    struct XmlAttribute *next;
};

struct XmlNode {
    char *name;
    size_t nameLength;
    uint32_t nameHash;
    XmlNodeType type;
    size_t length;
    XmlValue value;
    size_t savedLength;
    XmlValue savedValue;
    size_t numAttributes;
    XmlAttribute *attributes;
    struct XmlNode *children;   /* head of one-way circular list of this node's children */
    struct XmlNode *nextChild;
};

struct XmlAttributeInfo {
    char *name;
    size_t nameLength;
    char *value;
    size_t valueLength;
};

#define XmlNodeGetName(node)        (assert(node), (node)->name)
#define XmlNodeGetNameLength(node)  (assert(node), (node)->nameLength)
#define XmlNodeGetType(node)        (assert(node), (node)->type)
#define XmlNodeGetChildren(node)    (assert(node), (node)->children)
#define XmlNodeGetNextSibling(node) (assert(node), (node)->nextChild)

#define XmlNodeGetChar(node)    (assert(node && node->type == XML_CHAR), *(node)->value.charVal)
#define XmlNodeGetShort(node)   (assert(node && node->type == XML_SHORT), *(node)->value.shortVal)
#define XmlNodeGetInt(node)     (assert(node && node->type == XML_INT), *(node)->value.intVal)
#define XmlNodeGetFunc(node)    (assert(node && node->type == XML_FUNC), (node)->value.func)
#define XmlNodeGetString(node)  (assert(node && node->type == XML_STRING), (node)->value.ptr)

#define XmlNodeIsAnonymous(node)    (assert(node), !(node)->name || !(node)->name[0])
#define XmlNodeIsLeaf(node)     (assert(node), !(node)->children)
#define XmlNodeIsString(node)   (assert(node), (node)->type == XML_STRING)
#define XmlNodeIsNumeric(node)  (assert(node), (node)->type == XML_CHAR || (node)->type == XML_SHORT || (node)->type == XML_INT)
#define XmlNodeIsEmpty(node)    (assert(node), (node)->type == XML_EMPTY)
#define XmlNodeIsFunc(node)     (assert(node), (node)->type == XML_FUNC)
#define XmlNodeHasContent(node) (assert(node), (node)->name && (node)->name[0] && \
    (node)->type != XML_EMPTY && (node)->type != XML_FUNC && \
    (node)->value.ptr != nullptr && (!XmlNodeIsString(node) || XmlNodeGetString(node)[0]))

#define XmlNodeSetCharValue(node, val)  (assert(node), (node)->value.charVal = (val))
#define XmlNodeSetShortValue(node, val) (assert(node), (node)->value.shortVal = (val))
#define XmlNodeSetIntValue(node, val)   (assert(node), (node)->value.intVal = (val))
#define XmlNodeSetValue(node, val)      (assert(node), (node)->value = (val))
#define XmlNodeSetLength(node, val)     (assert(node), (node)->length = (val))


const char *XmlNodeTypeToString(XmlNodeType nodeType, size_t *length);

XmlNode *NewEmptyXmlNode(const char *name, int nameLen);
XmlNode *NewXmlNode(const char *name, int nameLen, XmlNodeType type, int length, bool alloc);
XmlNode *AddXmlNode(XmlNode *parent, XmlNode *child);
void XmlFreeAll();
void XmlDeallocateTree(XmlNode *tree);
void ReverseChildren(XmlNode *root);

const char *GetXmlNodeAttribute(const XmlNode *node, const char *attrName, size_t attrNameLen, size_t *valLen);
#define GetXmlNodeNumAttributes(node) ((node)->numAttributes)
void GetXmlNodeAttributes(const XmlNode *node, XmlAttributeInfo *attrInfo);

bool AddXmlNodeAttribute(XmlNode *node, const char *name, int nameLen, const char *value, int valueLen);
bool AddXmlContent(XmlNode *node, XmlNodeType type, void *content, int size);

void SetFunc(XmlNode *node, void *(*func)());
void RefreshFuncData(const XmlNode *node);

void XmlTreeSnapshot(XmlNode *root);
bool XmlTreeUnmodified(const XmlNode *root);
void XmlMergeTrees(XmlNode *destTree, const XmlNode *srcTree);
