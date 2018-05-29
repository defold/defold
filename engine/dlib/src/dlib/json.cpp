#include "json.h"

#include "math.h"
#include "utf8.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

    static void UnescapeString(Document* doc, Node* node)
    {
        char* read = doc->m_Json + node->m_Start;
        char* write = read;
        char* end = doc->m_Json + node->m_End;

        while (read < end) {
            if (read[0] == '\\') {
                switch (read[1]) {
                case '\"':
                    write[0] = '"';
                    break;
                case '/':
                    write[0] = '/';
                    break;
                case '\\':
                    write[0] = '\\';
                    break;
                case 'b':
                    write[0] = '\b';
                    break;
                case 'f':
                    write[0] = '\f';
                    break;
                case 'r':
                    write[0] = '\r';
                    break;
                case 'n':
                    write[0] = '\n';
                    break;
                case 't':
                    write[0] = '\t';
                    break;
                // Unicode
                case 'u':
                    char buf[5];
                    buf[0] = read[2];
                    buf[1] = read[3];
                    buf[2] = read[4];
                    buf[3] = read[5];
                    buf[4] = '\0';
                    uint32_t val =  strtoul(buf, 0, 16);
                    uint32_t n = dmUtf8::ToUtf8((uint16_t) val, write);
                    write += n;
                    write--;   // NOTE: We add 1 below and we don't want to subtract from an unsigned value
                    read += 4; // NOTE: We add 2 below
                    break;
                }
                read += 2;
                write++;
            } else {
                *write = *read;
                read++;
                write++;
            }
        }
        node->m_End = write - doc->m_Json;
    }

    static void UnescapeStrings(Document* doc)
    {
        int n = doc->m_NodeCount;
        for (int i = 0; i < n; ++i) {
            Node* node = &doc->m_Nodes[i];
            if (node->m_Type == TYPE_STRING) {
                UnescapeString(doc, node);
            }
        }
    }

    Result Parse(const char* buffer, Document* doc)
    {
        memset(doc, 0, sizeof(Document));
        jsmn_parser parser;
        // NOTE: initial count is increased in do-while
        unsigned int token_count = 64;

        if(!buffer)
        {
            doc->m_NodeCount = 0;
            doc->m_Nodes = 0;
            return RESULT_OK;
        }

        jsmnerr_t err = (jsmnerr_t) 0;
        jsmntok_t* tokens = 0;
        do {
            jsmn_init(&parser);
            token_count += dmMath::Min(256U, token_count);
            free(tokens);
            tokens = (jsmntok_t*) malloc(sizeof(jsmntok_t) * token_count);
            err = jsmn_parse(&parser, buffer, strlen(buffer), tokens, token_count);

        } while (err == JSMN_ERROR_NOMEM);

        if (err >= 0)
        {
            if (parser.toknext > 0)
            {
                doc->m_Nodes = (Node*) malloc(sizeof(Node) * parser.toknext);
                doc->m_NodeCount = CopyToken(tokens, doc->m_Nodes, 0);
                doc->m_Json = strdup(buffer);
                UnescapeStrings(doc);
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
        free(doc->m_Json);
        memset(doc, 0, sizeof(Document));
    }

    const char* CStringArrayToJsonString(const char** array,
        unsigned int length)
    {
        // Calculate the memory required to store the JSON string.
        unsigned int data_length = 2 + length * 2 + (length - 1);
        for (unsigned int i = 0; i < length; ++i)
        {
            data_length += strlen(array[i]);
            for (unsigned int n = 0; n < strlen(array[i]); ++n)
            {
                if (array[i][n] == '"')
                {
                    // We will have to escape this character with a backslash
                    data_length += 1;
                }
            }
        }

        // Allocate memory for the JSON string,
        // this has to be free'd by the caller.
        char* json_array = (char*) malloc(data_length + 1);
        if (json_array != 0)
        {
            unsigned int position = 0;
            memset((void*) json_array, 0, data_length + 1);

            json_array[position++] = '[';
            for (unsigned int i = 0; i < length; ++i)
            {
                json_array[position++] = '"';
                for (unsigned int n = 0; n < strlen(array[i]); ++n)
                {
                    if (array[i][n] == '"')
                    {
                        json_array[position++] = '\\';
                    }
                    json_array[position++] = array[i][n];
                }
                json_array[position++] = '"';
            }

            json_array[position++] = ']';
            json_array[position] = '\0';
        }

        return json_array;
    }

}
