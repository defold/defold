#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "json.h"
#include "math.h"

extern "C"
{
#include "jsmn/jsmn.h"
}

namespace dmJson
{
    static int CopyToken(const jsmntok_t* jsmntokens, Node* nodes, int index);

    static int CopyArray(const jsmntok_t* jsmntokens, Node* nodes, int index)
    {
        const jsmntok_t& t = jsmntokens[index];
        int count = t.size;

        ++index;
        for (int i = 0; i < count; ++i)
        {
            int prev = index;
            const jsmntok_t& ct = jsmntokens[index];
            if (ct.type == JSMN_PRIMITIVE || ct.type == JSMN_STRING)
            {
                // Optimization in order to avoid one level recursion here
                Node& n = nodes[index];
                n.m_Type = (Type) ct.type;
                n.m_Start = ct.start;
                n.m_End = ct.end;
                n.m_Size = ct.size;
                n.m_Sibling = -1;
                ++index;
            }
            else
            {
                index = CopyToken(jsmntokens, nodes, index);
            }

            // Skip last
            if (i < count - 1)
                nodes[prev].m_Sibling = index;
        }
        return index;
    }

    static int CopyObject(const jsmntok_t* jsmntokens, Node* nodes, int index)
    {
        const jsmntok_t& t = jsmntokens[index];
        int count = t.size;

        ++index;
        int prev = index;
        for (int i = 0; i < count; ++i)
        {
            if ((i & 1) == 0)
                prev = index;

            index = CopyToken(jsmntokens, nodes, index);

            if ((i & 1) == 1)
            {
                // Link prev key
                if (i < count - 1)
                {
                    nodes[prev].m_Sibling = index;
                }
            }
            else
            {
                // Value
            }
        }
        return index;
    }

    static int CopyToken(const jsmntok_t* jsmntokens, Node* nodes, int index)
    {
        const jsmntok_t& t = jsmntokens[index];
        Node& n = nodes[index];
        n.m_Type = (Type) t.type;
        n.m_Start = t.start;
        n.m_End = t.end;
        n.m_Size = t.size;
        n.m_Sibling = -1;

        switch(t.type)
        {
        case JSMN_PRIMITIVE:
        case JSMN_STRING:
            return index + 1;
        case JSMN_OBJECT:
            return CopyObject(jsmntokens, nodes, index);
            break;
        case JSMN_ARRAY:
            return CopyArray(jsmntokens, nodes, index);
        default:
            assert(0);
        }

        assert(0 && "not reached");
        return -1;
    }

    Result Parse(const char* buffer, Document* doc)
    {
        memset(doc, 0, sizeof(Document));
        jsmn_parser parser;
        // NOTE: initial count is increased in do-while
        unsigned int token_count = 64;

        jsmnerr_t err = JSMN_SUCCESS;
        jsmntok_t* tokens = 0;
        do {
            jsmn_init(&parser);
            token_count += dmMath::Min(256U, token_count);
            free(tokens);
            tokens = (jsmntok_t*) malloc(sizeof(jsmntok_t) * token_count);
            err = jsmn_parse(&parser, buffer, tokens, token_count);

        } while (err == JSMN_ERROR_NOMEM);

        if (err == JSMN_SUCCESS)
        {
            if (parser.toknext > 0)
            {
                doc->m_Nodes = (Node*) malloc(sizeof(Node) * parser.toknext);
                doc->m_NodeCount = CopyToken(tokens, doc->m_Nodes, 0);
            }
            else
            {
                doc->m_NodeCount = 0;
                doc->m_Nodes = 0;
            }
            free(tokens);
            return RESULT_OK;
        }
        else
        {
            free(tokens);
            switch (err)
            {
            case JSMN_ERROR_INVAL:
                return RESULT_SYNTAX_ERROR;
            case JSMN_ERROR_PART:
                return RESULT_INCOMPLETE;
            default:
                return RESULT_UNKNOWN;
            }
        }
        assert(0 && "not reachable");
        return RESULT_UNKNOWN;
    }

    void Free(Document* doc)
    {
        free(doc->m_Nodes);
        memset(doc, 0, sizeof(Document));
    }

}
