// plist.cpp : Apple xml plist
//

#include "stdafx.h"


XmlNode::XmlNode(char* data)
{
    char* end = NULL;

    if ('<' != data[0]) return;
    tag = data + 1;

    while (tag[cchTag] && ('>' != tag[cchTag]) && (' ' != tag[cchTag]) && ('\t' != tag[cchTag]))
    {
        cchTag++;
    }

    value = strchr(tag + cchTag, '>');
    tag[cchTag--] = 0;
    if (!value)
    {
        size = strlen(data);
        return;
    }
    if ('/' == *(value - 1))
    {
        size = (uint32_t)(value - data) + 1;
        value = NULL;
        return;
    }
    value++;

    end = value;
    while (end)
    {
        end = strchr(end, '<');
        if (end)
        {
            if (('/' == end[1]) && (0 == strncmp(end + 2, tag, cchTag)) && ('>' == end[cchTag + 3]))
            {
                break;
            }
            else
            {
                XmlNode last(end);
                children.push_back(last);
                if (!last.size)
                {
                    return;
                }
                end += last.size;
            }
        }
    }
    if (end)
    {
        cchValue = (uint32_t)(end - value);
        size = (uint32_t)(end - data) + cchTag + 3;
    }
    else
    {
        cchValue = strlen(value);
        size = (uint32_t)(value - data) + cchValue;
    }
    value[cchValue--] = 0;
}


PlistEntry::PlistEntry(XmlNode* keyNode, XmlNode* valueNode)
{
    if (keyNode)
    {
        if (0 != strncmp(keyNode->tag, "key", sizeof("key") - 1)) return;
        if (!keyNode->value) return;
        if (!keyNode->children.empty()) return;

        key = keyNode->value;
    }

    if (!valueNode) return;
    type = valueNode->tag;

    if ((0 == strncmp(type, "string", sizeof("string") - 1)) ||
        (0 == strncmp(type, "integer", sizeof("integer") - 1)) ||
        (0 == strncmp(type, "data", sizeof("data") - 1)))
    {
        if (!valueNode->value) return;
        if (!valueNode->children.empty()) return;

        value = valueNode->value;
    }

    else if ((0 == strncmp(type, "array", sizeof("array") - 1)) ||
        (0 == strncmp(type, "dict", sizeof("dict") - 1)))
    {
        for (uint32_t i = 0; i < valueNode->children.size(); i++)
        {
            XmlNode*    k = NULL;
            XmlNode*    v = NULL;

            if ('d' == type[0])
            {
                if (i + 1 == valueNode->children.size()) return;
                k = &valueNode->children.at(i++);
            }
            v = &valueNode->children.at(i);

            children.push_back(PlistEntry(k, v));
        }
    }
}

PlistEntry::PlistEntry(char* data) : PlistEntry(NULL, &XmlNode(data)) { }


PlistEntry* PlistEntry::get(char* search)
{
    for (uint32_t i = 0; i < children.size(); i++)
    {
        if (0 == strcmp(children.at(i).key, search))
        {
            return &children.at(i);
        }
    }
    return NULL;
}


void PlistEntry::debug_print(uint8_t indent)
{
    for (uint8_t i = 0; i < indent; i++)
    {
        fwprintf(stderr, L" ");
    }
    if (value)
    {
        fwprintf(stderr, L"%hs (%hs) = %hs", key, type, value);
    }
    else if (children.size())
    {
        fwprintf(stderr, L"%hs (%hs) = {", key, type);
        fwprintf(stderr, L"\r\n");
        for (uint32_t i = 0; i < children.size(); i++)
        {
            children.at(i).debug_print(indent + 4);
        }
        for (uint8_t i = 0; i < indent; i++)
        {
            fwprintf(stderr, L" ");
        }
        fwprintf(stderr, L"}");
    }
    else
    {
        fwprintf(stderr, L"%hs (%hs)", key, type);
    }
    fwprintf(stderr, L"\r\n");
    if (!indent)
    {
        fwprintf(stderr, L"\r\n");
    }
}
