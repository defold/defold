
#include <stdio.h>
#include <ctype.h>

#include "resource_mounts.h"
#include "resource_mounts_file.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

namespace dmResourceMounts
{
const int MOUNTSFILE_HEADER_VERSION = 1;
const char* MOUNTSFILE_CSV_SEPARATOR = "@,@";

static bool ParseInt(const char* token, int* outvalue)
{
    int value;
    int count = sscanf(token, "%d", &value);
    if (1 == count) *outvalue = value;
    return count == 1;
}

static bool ParseStr(char* token, char** outvalue)
{
    *outvalue = token;
    size_t len = strlen(token);
    while (len && isspace(token[len-1]))
    {
        token[--len] = 0;
    }
    return (*token) != '\0'; // if it's not an empty string
}

static int ParseLine(char* line, int* line_type, int* priority_or_version, char** name, char** uri)
{
    char* iter;
    char* token = dmStrTok(line, MOUNTSFILE_CSV_SEPARATOR, &iter);
    int index = 0;

    *line_type = -1;
    *priority_or_version = -1000;
    *name = 0;
    *uri = 0;

#define PARSE_TOKEN_INT(LINETYPE, INDEX, NAME, PTR) \
    if (LINETYPE == *line_type && INDEX == index) { \
        if (!ParseInt(token, PTR)) { \
            dmLogError("Failed to scan integer for '%s' from token '%s'", NAME, token); \
            return -1; \
        } \
    }

#define PARSE_TOKEN_STR(TYPE, INDEX, NAME, PTR) \
    if (TYPE == *line_type && INDEX == index) { \
        if (!ParseStr(token, PTR)) { \
            dmLogError("Failed to scan string for '%s' from token '%s'", NAME, token); \
            return -1; \
        } \
    }

    while (token) {
        if (index == 0)
        {
            if      (strstr(token, "VERSION")) *line_type = 0;
            else if (strstr(token, "MOUNT")) *line_type = 1;
        }

        // HEADER
        PARSE_TOKEN_INT(0, 1, "HEADER_VERSION", priority_or_version);
        // MOUNT
        PARSE_TOKEN_INT(1, 1, "PRIORITY", priority_or_version);
        PARSE_TOKEN_STR(1, 2, "NAME", name);
        PARSE_TOKEN_STR(1, 3, "URI", uri);

        token = dmStrTok(0, MOUNTSFILE_CSV_SEPARATOR, &iter);
        ++index;
    }

    if (*line_type == 0 && index != 2) return -1;
    if (*line_type == 1 && index != 4) return -1;
    return index;
#undef PARSE_TOKEN_STR
#undef PARSE_TOKEN_INT
}

void FreeMountsFile(dmArray<MountFileEntry>& entries)
{
    uint32_t size = entries.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        MountFileEntry& entry = entries[i];
        free((void*)entry.m_Name);
        free((void*)entry.m_Uri);
    }
}

dmResource::Result ReadMountsFile(const char* path, dmArray<MountFileEntry>& entries)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        dmLogError("Could not open file %s", path);
        return dmResource::RESULT_IO_ERROR;
    }

    // ------------------------------------------
    char line[2048] = {0};
    int header_version = -1;
    while (fgets(line, sizeof(line), file))
    {
        char* name;
        char* uri;
        int priority_or_version = -1;

        int line_type = -1;
        int r = ParseLine(line, &line_type, &priority_or_version, &name, &uri);
        if (r < 0) {
            dmLogError("Found invalid line in mounts file: '%s'", line);
            continue;
        }

        if (line_type == 0) // HEADER
        {
            header_version = priority_or_version;
            if (header_version != MOUNTSFILE_HEADER_VERSION)
            {
                dmLogError("Header version mismatch. Expected %d, got %d", MOUNTSFILE_HEADER_VERSION, header_version);
                fclose(file);
                return dmResource::RESULT_VERSION_MISMATCH;
            }
        }

        if (line_type != 1)
        {
            continue;
        }

        // MOUNT
        if (entries.Full())
            entries.OffsetCapacity(8);

        MountFileEntry entry;
        entry.m_Priority = priority_or_version;
        entry.m_Name = strdup(name);
        entry.m_Uri = strdup(uri);

        entries.Push(entry);
    }

    fclose(file);

    return header_version == MOUNTSFILE_HEADER_VERSION ? dmResource::RESULT_OK : dmResource::RESULT_VERSION_MISMATCH;
}


static uint32_t WriteStr(FILE* file, const char* str)
{
    char line[2048];
    uint32_t size = dmSnPrintf(line, sizeof(line), "%s", str);
    return (uint32_t)fwrite(line, size, 1, file);
}

static uint32_t WriteInt(FILE* file, int value)
{
    char line[2048];
    uint32_t size = dmSnPrintf(line, sizeof(line), "%d", value);
    return (uint32_t)fwrite(line, size, 1, file);
}

dmResource::Result WriteMountsFile(const char* path, const dmArray<MountFileEntry>& entries)
{
    FILE* file = fopen(path, "wb");
    if (!file)
    {
        dmLogError("Could not open file for writing %s", path);
        return dmResource::RESULT_IO_ERROR;
    }

#define WRITE_STR(NAME) \
    if (1 != WriteStr(file, NAME)) {                    \
        dmLogError("Failed to write to '%s'", path);    \
        fclose(file);                                   \
        return dmResource::RESULT_OK;                   \
    }
#define WRITE_INT(VALUE) \
    if (1 != WriteInt(file, VALUE)) {                   \
        dmLogError("Failed to write to '%s'", path);    \
        fclose(file);                                   \
        return dmResource::RESULT_OK;                   \
    }

    WRITE_STR("VERSION"); WRITE_STR(MOUNTSFILE_CSV_SEPARATOR); WRITE_INT(MOUNTSFILE_HEADER_VERSION); WRITE_STR("\n");

    uint32_t size = entries.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        const MountFileEntry& entry = entries[i];

        if (entry.m_Name == 0 || entry.m_Uri == 0 || entry.m_Priority < 0)
            continue;

        WRITE_STR("MOUNT");
            WRITE_STR(MOUNTSFILE_CSV_SEPARATOR); WRITE_INT(entry.m_Priority);
            WRITE_STR(MOUNTSFILE_CSV_SEPARATOR); WRITE_STR(entry.m_Name);
            WRITE_STR(MOUNTSFILE_CSV_SEPARATOR); WRITE_STR(entry.m_Uri);
            WRITE_STR("\n");
    }

    fclose(file);

    dmLogInfo("Wrote %s\n", path);
    return dmResource::RESULT_OK;

#undef WRITE_STR
#undef WRITE_INT
}

} // namespace dmResourceMounts
