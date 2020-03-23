#pragma once


struct XmlNode
{
    char*       tag = NULL;
    uint32_t    cchTag = 0;

    char*       value = NULL;
    uint32_t    cchValue = 0;

    std::vector<XmlNode> children;

    uint32_t    size = 0;

    XmlNode(char* data);
};


struct PlistEntry
{
    char*       key = NULL;
    char*       type = NULL;
    char*       value = NULL;

    std::vector<PlistEntry> children;

    PlistEntry(char* data);
    PlistEntry(XmlNode* keyNode, XmlNode* valueNode);

    PlistEntry* get(char* key);

    void debug_print(uint8_t indent);
};