#include "xmltree.h"
#include "xmlparse.h"
#include "qalloc.h"

/* don't try loading too big files :P */
static char xmlHeap[20 * 1024];
static unsigned int xmlHeapIndex;

static XmlNodeType GetAttributeType(const char *value, int length);

static void *XmlAlloc(int size)
{
    char *p = &xmlHeap[xmlHeapIndex];
    assert(size >= 0 && xmlHeapIndex + size <= sizeof(xmlHeap));
    if (xmlHeapIndex + size > sizeof(xmlHeap))
        return nullptr;
#ifdef DEBUG
    memset(xmlHeap + xmlHeapIndex, 0xbb, size);
#endif
    xmlHeapIndex += size;
    return p;
}

static void XmlFree(void *ptr)
{
    if (!ptr)
        return;
    assert((char *)ptr >= xmlHeap && (char *)ptr < xmlHeap + sizeof(xmlHeap));
    xmlHeapIndex = (char *)ptr - xmlHeap;
    assert(xmlHeapIndex < sizeof(xmlHeap));
#ifdef DEBUG
    memset(xmlHeap + xmlHeapIndex, 0xaa, sizeof(xmlHeap) - xmlHeapIndex);
#endif
}

static char *XmlStrdup(const char *str, int size)
{
    char *duplicate = (char *)XmlAlloc(size + 1);
    assert(str && size >= 0);
    if (duplicate) {
        memcpy(duplicate, str, size);
        duplicate[size] = '\0';
    }
    return duplicate;
}

void XmlFreeAll()
{
    XmlFree(xmlHeap);
}

void XmlDeallocateTree(XmlNode *tree)
{
    XmlFree(tree);
}

XmlNode *NewEmptyXmlNode(const char *name, int nameLen)
{
    return NewXmlNode(name, nameLen, XML_EMPTY, 0, false);
}

XmlNode *NewXmlNode(const char *name, int nameLen, XmlNodeType type, int length, bool alloc)
{
    assert(((!name || !*name) && !nameLen) || (name && nameLen));
    assert(!name || strlen(name) == nameLen);

    XmlNode *x = (XmlNode *)XmlAlloc(sizeof(XmlNode) + (name && *name ? nameLen + 1 : 0));
    if (name && *name) {
        x->name = (char *)x + sizeof(XmlNode);
        memcpy(x->name, name, nameLen);
        x->name[nameLen] = '\0';
        x->nameHash = simpleHash(name, nameLen);
        x->savedValue.ptr = (char *)XmlAlloc(length);
    } else {
        /* anonymous node */
        x->nameHash = 0;
        x->name = nullptr;
        x->savedValue.ptr = nullptr;
    }

    x->nameLength = nameLen;
    x->type = type;
    x->length = length;
    x->savedLength = length;
    x->value.ptr = alloc ? (char *)XmlAlloc(length) : nullptr;
    assert((XmlNodeIsAnonymous(x) == (x->savedValue.ptr == nullptr)) && (!alloc || x->value.ptr));
    x->children = x->nextChild = nullptr;
    x->attributes = nullptr;
    x->numAttributes = 0;
    return x;
}

XmlNode *AddXmlNode(XmlNode *parent, XmlNode *child)
{
    /* insert into children list */
    child->nextChild = parent->children;
    parent->children = child;
    return parent;
}

void SetFunc(XmlNode *node, void *(*func)())
{
    node->type = XML_FUNC;
    node->value.func = func;
    node->length = 0;
}

void RefreshFuncData(const XmlNode *node)
{
    int ofs = 0;
    char *data;
    assert(node && node->type == XML_FUNC && node->value.func && node->children);
    data = (char *)node->value.func();

    for (XmlNode *child = node->children; child; child = child->nextChild) {
        child->value.ptr = data + ofs;
        ofs += child->length;
    }
}

static void SnapshotNode(XmlNode *node)
{
    assert(node);

    if (XmlNodeIsAnonymous(node))
        return;

    if (!node->savedValue.ptr && node->length)
        node->savedValue.ptr = (char *)XmlAlloc(node->length);

    switch (node->type) {
    case XML_CHAR:
    case XML_SHORT:
    case XML_INT:
    case XML_STRING:
    case XML_ARRAY:
        node->savedLength = node->length;
        memcpy(node->savedValue.ptr, node->value.ptr, node->length);
        break;

    case XML_FUNC:
        RefreshFuncData(node);
        break;

    case XML_EMPTY:
        break;

    default:
        assert_msg(0, "Unknown XML node type");
    }
}

void XmlTreeSnapshot(XmlNode *root)
{
    while (root) {
        SnapshotNode(root);
        XmlTreeSnapshot(root->children);
        root = root->nextChild;
    }
}

static bool NodeUnmodified(const XmlNode *node)
{
    assert(node);

    if (XmlNodeIsAnonymous(node))
        return true;

    switch (node->type) {
    case XML_CHAR:
    case XML_SHORT:
    case XML_INT:
    case XML_STRING:
    case XML_ARRAY:
        assert(node->value.ptr && node->savedValue.ptr);
        return node->length == node->savedLength && !memcmp(node->value.ptr, node->savedValue.ptr, node->length);

    case XML_FUNC:
        RefreshFuncData(node);
        /* assume fall-through */

    case XML_EMPTY:
        return true;

    default:
        assert_msg(0, "Unknown XML node type");
    }

    return false;
}

bool XmlTreeUnmodified(const XmlNode *root)
{
    while (root) {
        if (!XmlTreeUnmodified(root->children))
            return false;

        if (!NodeUnmodified(root))
            return false;

        root = root->nextChild;
    }
    return true;
}

/** NodesEqual

    src -> XML node 1 to compare
    dst -> XML node 2 to compare

    Return true if nodes are considered to be equal. So far only compare by name, but other criteria
    could be added, such as equality of certain attributes for example.
*/
static bool NodesEqual(const XmlNode *src, const XmlNode *dst)
{
    assert(src && dst);
    return src->nameLength == dst->nameLength && src->nameHash == dst->nameHash && !strcmp(src->name, dst->name);
}

static void ConvertNumberToString(char *dst, int dstSize, char *from, int fromSize)
{
    int fromVal;
    char *convVal;
    assert(dst && dstSize > 0 && from);

    switch (fromSize) {
    case 1:
        fromVal = *(int8_t *)from;
        break;

    case 2:
        fromVal = *(int16_t *)from;
        break;

    case 4:
        fromVal = *(int32_t *)from;
        break;

    default:
        assert(0);
    }

    convVal = int2str(fromVal);
    *strncpy(dst, convVal, dstSize - 1) = '\0';
}

static int convertStringToNumber(const char *src, int srcLen)
{
    char convBuf[33];
    int copyLen = min(srcLen, (int)sizeof(convBuf) - 1);
    memcpy(convBuf, src, copyLen);
    convBuf[copyLen] = '\0';
    return strtol(convBuf, nullptr, 0, nullptr);
}

/** MergeNodes

    dstNode -> destination node
    srcNode -> source node

    Put value from source node to destination node, if their types differ perform conversion.
*/
static void MergeNodes(XmlNode *dstNode, const XmlNode *srcNode)
{
    assert(dstNode && srcNode);
    assert((XmlNodeGetName(srcNode) && XmlNodeGetName(dstNode)) || (!XmlNodeGetName(srcNode) && !XmlNodeGetName(dstNode)));

    /* skip anonymous nodes */
    if (XmlNodeIsAnonymous(srcNode))
        return;

    if ((dstNode->type == srcNode->type) ||
        ((dstNode->type == XML_STRING) && (srcNode->type == XML_ARRAY)) ||
        ((dstNode->type == XML_ARRAY) && (srcNode->type == XML_STRING))) {
        if (dstNode->length > 0) {
            assert(dstNode->value.ptr && srcNode->value.ptr);
            assert(dstNode->type == XML_STRING || dstNode->type == XML_ARRAY || dstNode->length == srcNode->length);

            if (dstNode->length > srcNode->length) {
                memcpy(dstNode->value.ptr, srcNode->value.ptr, srcNode->length);
                memset(dstNode->value.ptr + srcNode->length, 0, dstNode->length - srcNode->length);
            } else
                memcpy(dstNode->value.ptr, srcNode->value.ptr, dstNode->length);

            if (dstNode->type == XML_STRING)
                dstNode->value.ptr[dstNode->length - 1] = '\0';
        }
        return;
    } else {
        assert(dstNode->type != srcNode->type);
        #define CONV(to, from) ((to) * XML_TYPE_MAX + (from))
        switch (CONV(dstNode->type, srcNode->type)) {
        case CONV(XML_CHAR, XML_SHORT):
            *dstNode->value.charVal = *srcNode->value.shortVal;
            break;

        case CONV(XML_CHAR, XML_INT):
            *dstNode->value.charVal = *srcNode->value.intVal;
            break;

        case CONV(XML_CHAR, XML_STRING):
        case CONV(XML_CHAR, XML_ARRAY):
            *dstNode->value.charVal = convertStringToNumber(srcNode->value.ptr, srcNode->length);
            break;

        case CONV(XML_SHORT, XML_CHAR):
            *dstNode->value.shortVal = *srcNode->value.charVal;
            break;

        case CONV(XML_SHORT, XML_INT):
            *dstNode->value.shortVal = *srcNode->value.intVal;
            break;

        case CONV(XML_SHORT, XML_STRING):
        case CONV(XML_SHORT, XML_ARRAY):
            *dstNode->value.shortVal = convertStringToNumber(srcNode->value.ptr, srcNode->length);
            break;

        case CONV(XML_INT, XML_CHAR):
            *dstNode->value.intVal = *srcNode->value.charVal;
            break;

        case CONV(XML_INT, XML_SHORT):
            *dstNode->value.intVal = *srcNode->value.shortVal;
            break;

        case CONV(XML_INT, XML_STRING):
        case CONV(XML_INT, XML_ARRAY):
            *dstNode->value.intVal = convertStringToNumber(srcNode->value.ptr, srcNode->length);
            break;

        case CONV(XML_STRING, XML_CHAR):
            ConvertNumberToString(dstNode->value.ptr, dstNode->length, srcNode->value.ptr, 1);
            break;

        case CONV(XML_STRING, XML_SHORT):
            ConvertNumberToString(dstNode->value.ptr, dstNode->length, srcNode->value.ptr, 2);
            break;

        case CONV(XML_STRING, XML_INT):
            ConvertNumberToString(dstNode->value.ptr, dstNode->length, srcNode->value.ptr, 4);
            break;

        case CONV(XML_ARRAY, XML_CHAR):
            ConvertNumberToString(dstNode->value.ptr, dstNode->length, srcNode->value.ptr, 1);
            break;

        case CONV(XML_ARRAY, XML_SHORT):
            ConvertNumberToString(dstNode->value.ptr, dstNode->length, srcNode->value.ptr, 2);
            break;

        case CONV(XML_ARRAY, XML_INT):
            ConvertNumberToString(dstNode->value.ptr, dstNode->length, srcNode->value.ptr, 4);
            break;
        }
    }
}

/** XmlMergeTrees

    destTree -> authoritative tree for structure, receives results of merging
    srcTree  -> tree from which we will read new data for nodes, if present

    Go level by level. For each source node match destination node by name, if present.
    Perform conversion from source type to destination if types are different.
*/
void XmlMergeTrees(XmlNode *destTree, const XmlNode *srcTree)
{
    if (!srcTree)
        return;
    while (destTree) {
        const XmlNode *srcNode = srcTree;

        while (srcNode) {
            if (NodesEqual(destTree, srcNode))
                break;
            srcNode = srcNode->nextChild;
        }

        if (srcNode) {
            MergeNodes(destTree, srcNode);
            XmlMergeTrees(destTree->children, srcNode->children);
        }

        destTree = destTree->nextChild;
    }
}

static XmlAttribute *FindAttribute(const XmlNode *node, const char *attrName, uint32_t hash)
{
    XmlAttribute *attr;
    assert(node && attrName);

    for (attr = node->attributes; attr; attr = attr->next) {
        if (attr->nameHash == hash && !strcmp(attr->name, attrName))
            return attr;
    }

    return nullptr;
}

static bool CheckNodeType(XmlNodeType type, const char *name, const char *value)
{
    UNUSED(name);
    UNUSED(value);

    if (type == XML_TYPE_MAX) {
        WriteToLog("XML node '%s' has invalid type (%s).", name, value);
        return false;
    }

    return true;
}

bool AddXmlNodeAttribute(XmlNode *node, const char *name, int nameLen, const char *value, int valueLen)
{
    uint32_t hash;
    XmlAttribute *attr;
    assert(node && name && value);
    hash = simpleHash(name, nameLen);

    if (FindAttribute(node, name, hash)) {
        WriteToLog("Attribute '%s' already specified for node '%s'.", name, node->name);
        return false;
    }

    assert(simpleHash("type", 4) == 0x7c9ebd07);
    if (hash == 0x7c9ebd07 && !strcmp(name, "type")) {
        XmlNodeType type = GetAttributeType(value, valueLen);
        if (!CheckNodeType(type, node->name, value))
            return false;
        node->type = type;
    }

    attr = (XmlAttribute *)XmlAlloc(sizeof(XmlAttribute));
    if (attr) {
        attr->name = XmlStrdup(name, nameLen);
        if (!attr->name)
            return false;
        attr->nameLength = nameLen;
        attr->nameHash = hash;
        attr->value = XmlStrdup(value, valueLen);
        if (!attr->value) {
            XmlFree(attr->name);
            return false;
        }
        attr->valueLength = valueLen;
        attr->next = node->attributes;
        node->attributes = attr;
        node->numAttributes++;
        return true;
    }

    return false;
}

static XmlNodeType GetAttributeType(const char *value, int length)
{
    uint32_t hash;
    static_assert(XML_TYPE_MAX == 7, "Xml types changed.");
    struct AllowedType {
        const char *name;
        uint32_t hash;
        XmlNodeType type;
    } static const allowedTypes[] = {
        { "char",   0x7c952063, XML_CHAR   },
        { "short",  0x105af0d5, XML_SHORT  },
        { "int",    0x0b888030, XML_INT    },
        { "string", 0x1c93affc, XML_STRING },
        { "array",  0x0f1abe24, XML_ARRAY  },
        { "empty",  0x0f605c34, XML_EMPTY  },
    };

    assert(simpleHash("char", 4) == 0x7c952063);    /* in case algo gets changed :P */
    assert(value);
    hash = simpleHash(value, length);

    for (size_t i = 0; i < sizeofarray(allowedTypes); i++)
        if (allowedTypes[i].hash == hash && !strcmp(value, allowedTypes[i].name))
            return allowedTypes[i].type;

    return XML_TYPE_MAX;
}

const char *XmlNodeTypeToString(XmlNodeType nodeType, size_t *length)
{
    struct TypeString {
        const char *name;
        size_t length;
    } static const typeNames[] = {
        "char",     4,
        "short",    5,
        "int",      3,
        "string",   6,
        "array",    5,
        "empty",    5,
        "function", 8,
    };

    assert(XML_TYPE_MAX == 7);

    if (nodeType >= sizeofarray(typeNames))
        nodeType = XML_EMPTY;

    if (length)
        *length = typeNames[nodeType].length;

    return typeNames[nodeType].name;
}

bool AddXmlContent(XmlNode *node, XmlNodeType type, void *content, int size)
{
    assert(node && content && size > 0);
    node->type = type;

    if (!(node->value.ptr = (char *)XmlAlloc(size)))
        return false;

    memcpy(node->value.ptr, content, size);
    node->length = size;
    return true;
}

const char *GetXmlNodeAttribute(const XmlNode *node, const char *attrName, size_t attrNameLen, size_t *valLen)
{
    uint32_t hash;
    assert(node);
    if (valLen)
        *valLen = 0;

    if (!node->attributes)
        return nullptr;

    hash = simpleHash(attrName, attrNameLen);

    for (XmlAttribute *attr = node->attributes; attr; attr = attr->next) {
        if (attr->nameHash == hash && !strcmp(attr->name, attrName)) {
            if (valLen)
                *valLen = attr->valueLength;
            return attr->value;
        }
    }

    return nullptr;
}

void GetXmlNodeAttributes(const XmlNode *node, XmlAttributeInfo *attrInfo)
{
    size_t i = 0;
    XmlAttribute *elem;
    assert(node);

    for (elem = node->attributes; elem; elem = elem->next, i++) {
        attrInfo->name = elem->name;
        attrInfo->nameLength = elem->nameLength;
        attrInfo->value = elem->value;
        attrInfo->valueLength = elem->valueLength;
        attrInfo++;
    }

    assert(i == node->numAttributes);
}

void ReverseChildren(XmlNode *root)
{
    if (root && root->children) {
        XmlNode *current = root->children;
        XmlNode *next = current->nextChild;
        XmlNode *nextNext;

        ReverseChildren(current);
        current->nextChild = nullptr;

        while (next) {
            nextNext = next->nextChild;
            next->nextChild = current;
            current = next;
            ReverseChildren(current);
            next = nextNext;
        }

        root->children = current;
    }
}
