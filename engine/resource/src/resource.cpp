#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef __linux__
#include <limits.h>
#elif defined (__MACH__)
#include <sys/param.h>
#endif

#include <dlib/buffer.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/http_client.h>
#include <dlib/http_cache.h>
#include <dlib/http_cache_verify.h>
#include <dlib/math.h>
#include <dlib/memory.h>
#include <dlib/uri.h>
#include <dlib/path.h>
#include <dlib/profile.h>
#include <dlib/message.h>
#include <dlib/sys.h>
#include <dlib/time.h>
#include <dlib/mutex.h>

#include "resource.h"
#include "resource_ddf.h"
#include "resource_private.h"

/*
 * TODO:
 *
 *  - Resources could be loaded twice if canonical path is different for equivalent files.
 *    We could use realpath or similar function but we want to avoid file accesses when converting
 *    a canonical path to hash value. This functionality is used in the GetDescriptor function.
 *
 *  - If GetCanonicalPath exceeds RESOURCE_PATH_MAX PATH_TOO_LONG should be returned.
 *
 *  - Handle out of resources. Eg Hashtables full.
 */

extern dmDDF::Descriptor dmLiveUpdateDDF_ManifestFile_DESCRIPTOR;

namespace dmResource
{
const int DEFAULT_BUFFER_SIZE = 1024 * 1024;

#define RESOURCE_SOCKET_NAME "@resource"

const char* MAX_RESOURCES_KEY = "resource.max_resources";

const char SHARED_NAME_CHARACTER = ':';

struct ResourceReloadedCallbackPair
{
    ResourceReloadedCallback    m_Callback;
    void*                       m_UserData;
};

struct SResourceFactory
{
    // TODO: Arg... budget. Two hash-maps. Really necessary?
    dmHashTable<uint64_t, SResourceDescriptor>*  m_Resources;
    dmHashTable<uintptr_t, uint64_t>*            m_ResourceToHash;
    // Only valid if RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT is set
    // Used for reloading of resources
    dmHashTable<uint64_t, const char*>*          m_ResourceHashToFilename;
    // Only valid if RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT is set
    dmArray<ResourceReloadedCallbackPair>*       m_ResourceReloadedCallbacks;
    SResourceType                                m_ResourceTypes[MAX_RESOURCE_TYPES];
    uint32_t                                     m_ResourceTypesCount;

    // Guard for anything that touches anything that could be shared
    // with GetRaw (used for async threaded loading). Liveupdate, HttpClient, m_Buffer
    // m_BuiltinsArchive and m_Archive
    dmMutex::Mutex                               m_LoadMutex;

    // dmResource::Get recursion depth
    uint32_t                                     m_RecursionDepth;
    // List of resources currently in dmResource::Get call-stack
    dmArray<const char*>                         m_GetResourceStack;

    dmMessage::HSocket                           m_Socket;

    dmURI::Parts                                 m_UriParts;
    dmHttpClient::HClient                        m_HttpClient;
    dmHttpCache::HCache                          m_HttpCache;
    LoadBufferType*                              m_HttpBuffer;

    dmArray<char>                                m_Buffer;

    // HTTP related state
    // Total number bytes loaded in current GET-request
    int32_t                                      m_HttpContentLength;
    uint32_t                                     m_HttpTotalBytesStreamed;
    int                                          m_HttpStatus;
    Result                                       m_HttpFactoryResult;

    // Manifest for builtin resources
    Manifest*                                   m_BuiltinsManifest;

    // Resource manifest
    Manifest*                                    m_Manifest;
    void*                                        m_ArchiveMountInfo;

    // Shared resources
    uint32_t                                    m_NonSharedCount; // a running number, helping id the potentially non shared assets
};

SResourceType* FindResourceType(SResourceFactory* factory, const char* extension)
{
    for (uint32_t i = 0; i < factory->m_ResourceTypesCount; ++i)
    {
        SResourceType* rt = &factory->m_ResourceTypes[i];
        if (strcmp(extension, rt->m_Extension) == 0)
        {
            return rt;
        }
    }
    return 0;
}

// TODO: Test this...
static void GetCanonicalPath(const char* base_dir, const char* relative_dir, char* buf)
{
    DM_SNPRINTF(buf, RESOURCE_PATH_MAX, "%s/%s", base_dir, relative_dir);

    char* source = buf;
    char* dest = buf;
    char last_c = 0;
    while (*source != 0)
    {
        char c = *source;
        if (c != '/' || (c == '/' && last_c != '/'))
            *dest++ = c;

        last_c = c;
        ++source;
    }
    *dest = '\0';
}

void GetCanonicalPath(HFactory factory, const char* relative_dir, char* buf)
{
    return GetCanonicalPath(factory->m_UriParts.m_Path, relative_dir, buf);
}

Result CheckSuppliedResourcePath(const char* name)
{
    if (name[0] == 0)
    {
        dmLogError("Empty resource path");
        return RESULT_RESOURCE_NOT_FOUND;
    }
    if (name[0] != '/')
    {
        dmLogError("Resource path is not absolute (%s)", name);
        return RESULT_RESOURCE_NOT_FOUND;
    }
    return RESULT_OK;
}

void SetDefaultNewFactoryParams(struct NewFactoryParams* params)
{
    params->m_MaxResources = 1024;
    params->m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;

    params->m_ArchiveManifest.m_Data = 0;
    params->m_ArchiveManifest.m_Size = 0;
    params->m_ArchiveIndex.m_Data = 0;
    params->m_ArchiveIndex.m_Size = 0;
    params->m_ArchiveData.m_Data = 0;
    params->m_ArchiveData.m_Size = 0;
}

static void HttpHeader(dmHttpClient::HResponse response, void* user_data, int status_code, const char* key, const char* value)
{
    SResourceFactory* factory = (SResourceFactory*) user_data;
    factory->m_HttpStatus = status_code;

    if (strcmp(key, "Content-Length") == 0)
    {
        factory->m_HttpContentLength = strtol(value, 0, 10);
        if (factory->m_HttpContentLength < 0) {
            dmLogError("Content-Length negative (%d)", factory->m_HttpContentLength);
        } else {
            if (factory->m_HttpBuffer->Capacity() < factory->m_HttpContentLength) {
                factory->m_HttpBuffer->SetCapacity(factory->m_HttpContentLength);
            }
            factory->m_HttpBuffer->SetSize(0);
        }
    }
}

static void HttpContent(dmHttpClient::HResponse, void* user_data, int status_code, const void* content_data, uint32_t content_data_size)
{
    SResourceFactory* factory = (SResourceFactory*) user_data;
    (void) status_code;

    if (!content_data && content_data_size)
    {
        factory->m_HttpBuffer->SetSize(0);
        return;
    }

    // We must set http-status here. For direct cached result HttpHeader is not called.
    factory->m_HttpStatus = status_code;

    if (factory->m_HttpBuffer->Remaining() < content_data_size) {
        uint32_t diff = content_data_size - factory->m_HttpBuffer->Remaining();
        // NOTE: Resizing the the array can be inefficient but sometimes we don't know the actual size, i.e. when "Content-Size" isn't set
        factory->m_HttpBuffer->OffsetCapacity(diff + 1024 * 1024);
    }

    factory->m_HttpBuffer->PushArray((const char*) content_data, content_data_size);
    factory->m_HttpTotalBytesStreamed += content_data_size;
}

Manifest* GetManifest(HFactory factory)
{
    return factory->m_Manifest;
}

uint32_t HashLength(dmLiveUpdateDDF::HashAlgorithm algorithm)
{
    const uint32_t bitlen[5] = { 0U, 128U, 160U, 256U, 512U };
    return bitlen[(int) algorithm] / 8U;
}

void HashToString(dmLiveUpdateDDF::HashAlgorithm algorithm, const uint8_t* hash, char* buf, uint32_t buflen)
{
    const uint32_t hlen = HashLength(algorithm);
    if (buf != NULL && buflen > 0)
    {
        buf[0] = 0x0;
        for (uint32_t i = 0; i < hlen; ++i)
        {
            char current[3];
            DM_SNPRINTF(current, 3, "%02x", hash[i]);
            dmStrlCat(buf, current, buflen);
        }
    }
}

Result StoreManifest(Manifest* manifest)
{
    char app_support_path[DMPATH_MAX_PATH];
    char id_buf[MANIFEST_PROJ_ID_LEN]; // String repr. of project id SHA1 hash
    char manifest_file_path[DMPATH_MAX_PATH];
    char manifest_tmp_file_path[DMPATH_MAX_PATH];
    HashToString(dmLiveUpdateDDF::HASH_SHA1, manifest->m_DDFData->m_Header.m_ProjectIdentifier.m_Data.m_Data, id_buf, MANIFEST_PROJ_ID_LEN);
    dmSys::GetApplicationSupportPath(id_buf, app_support_path, DMPATH_MAX_PATH);
    dmPath::Concat(app_support_path, "liveupdate.dmanifest", manifest_file_path, DMPATH_MAX_PATH);
    dmStrlCpy(manifest_tmp_file_path, manifest_file_path, DMPATH_MAX_PATH);
    dmStrlCat(manifest_tmp_file_path, ".tmp", DMPATH_MAX_PATH);
    dmLogInfo("Storing new manifest file to path: %s, (%s)", manifest_file_path, manifest_tmp_file_path);
    // write to tempfile, if successful move/rename and then delete tmpfile
    dmDDF::Result ddf_result = dmDDF::SaveMessageToFile(manifest->m_DDF, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, manifest_tmp_file_path);
    if (ddf_result != dmDDF::RESULT_OK)
    {
        dmLogInfo("Failed to store manifest with result: %u", ddf_result);
        return RESULT_DDF_ERROR;
    }
    dmSys::Result sys_result = dmSys::WriteWithMove(manifest_file_path, manifest_tmp_file_path);
    if (sys_result !=dmSys::RESULT_OK)
    {
        dmLogError("Failed to write manifest to storage, result = %i", sys_result); 
        return RESULT_IO_ERROR;
    }
    dmSys::Unlink(manifest_tmp_file_path);
    return RESULT_OK;
}

Result LoadArchiveIndex(const char* manifestPath, const char* bundle_dir, HFactory factory)
{
    Result result = RESULT_OK;
    const uint32_t manifest_extension_length = strlen("dmanifest");
    const uint32_t index_extension_length = strlen("arci");

    char archive_index_path[DMPATH_MAX_PATH];
    char archive_resource_path[DMPATH_MAX_PATH];
    char liveupdate_index_path[DMPATH_MAX_PATH];
    char app_support_path[DMPATH_MAX_PATH];
    char id_buf[MANIFEST_PROJ_ID_LEN]; // String repr. of project id SHA1 hash

    dmStrlCpy(archive_resource_path, bundle_dir, strlen(bundle_dir) - manifest_extension_length + 1);
    dmStrlCat(archive_resource_path, "arcd", DMPATH_MAX_PATH);
    // derive path to arci file from path to arcd file
    dmStrlCpy(archive_index_path, archive_resource_path, DMPATH_MAX_PATH);
    archive_index_path[strlen(archive_index_path) - 1] = 'i';

    HashToString(dmLiveUpdateDDF::HASH_SHA1, factory->m_Manifest->m_DDFData->m_Header.m_ProjectIdentifier.m_Data.m_Data, id_buf, MANIFEST_PROJ_ID_LEN);
    dmSys::GetApplicationSupportPath(id_buf, app_support_path, DMPATH_MAX_PATH);
    dmPath::Concat(app_support_path, "liveupdate.arci", liveupdate_index_path, DMPATH_MAX_PATH);
    struct stat file_stat;
    bool luIndexExists = stat(liveupdate_index_path, &file_stat) == 0;

    if (!luIndexExists)
    {
        result = MountArchiveInternal(archive_index_path, archive_resource_path, 0x0, &factory->m_Manifest->m_ArchiveIndex, &factory->m_ArchiveMountInfo);
    }
    else // If a liveupdate index exists, use that one instead
    {
        char liveupdate_resource_path[DMPATH_MAX_PATH];
        dmStrlCpy(liveupdate_resource_path, liveupdate_index_path, strlen(liveupdate_index_path) - index_extension_length + 1);
        dmStrlCat(liveupdate_resource_path, "arcd", DMPATH_MAX_PATH);
        // Check if any liveupdate resources were stored last time engine was running
        char temp_archive_index_path[DMPATH_MAX_PATH];
        dmStrlCpy(temp_archive_index_path, liveupdate_index_path, strlen(liveupdate_index_path)+1);
        dmStrlCat(temp_archive_index_path, ".tmp", DMPATH_MAX_PATH); // check for liveupdate.arci.tmp
        bool luTempIndexExists = stat(temp_archive_index_path, &file_stat) == 0;
        if (luTempIndexExists)
        {
            dmLogInfo("Temp exists!");
            dmSys::Result moveResult = dmSys::WriteWithMove(liveupdate_index_path, temp_archive_index_path);

            if (moveResult != dmSys::RESULT_OK)
            {
                // The recently added resources will not be available if we proceed after this point
                dmLogError("Fail to load liveupdate index data.")
                return RESULT_IO_ERROR;
            }
            dmSys::Unlink(temp_archive_index_path);
        }
        result = MountArchiveInternal(liveupdate_index_path, archive_resource_path, liveupdate_resource_path, &factory->m_Manifest->m_ArchiveIndex, &factory->m_ArchiveMountInfo);
        if (result != RESULT_OK)
        {
            dmLogError("Failed to mount archive, result = %i", result);
            return RESULT_IO_ERROR;
        }
        int archive_id_cmp = dmResourceArchive::CmpArchiveIdentifier(factory->m_Manifest->m_ArchiveIndex, factory->m_Manifest->m_DDF->m_ArchiveIdentifier.m_Data, factory->m_Manifest->m_DDF->m_ArchiveIdentifier.m_Count);
        if (archive_id_cmp != 0)
        {
            dmLogInfo("Reloading index!");
            dmResourceArchive::Result reload_res = ReloadBundledArchiveIndex(archive_index_path, archive_resource_path, liveupdate_index_path, liveupdate_resource_path, factory->m_Manifest->m_ArchiveIndex, factory->m_ArchiveMountInfo);

            if (reload_res != dmResourceArchive::RESULT_OK)
            {
                dmLogError("Failed to reload liveupdate index with bundled index, result = %i", reload_res);
                return RESULT_IO_ERROR;
            }
        }
	}

    return result;
}

Result ParseManifestDDF(uint8_t* manifest_buf, uint32_t size, dmResource::Manifest*& manifest)
{
    // Read from manifest resource
    dmDDF::Result result = dmDDF::LoadMessage(manifest_buf, size, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, (void**) &manifest->m_DDF);
    if (result != dmDDF::RESULT_OK)
    {
        dmLogError("Failed to parse Manifest (%i)", result);
        return RESULT_IO_ERROR;
    }

    // Read data blob from ManifestFile into ManifestData message
    result = dmDDF::LoadMessage(manifest->m_DDF->m_Data.m_Data, manifest->m_DDF->m_Data.m_Count, dmLiveUpdateDDF::ManifestData::m_DDFDescriptor, (void**) &manifest->m_DDFData);
    if (manifest->m_DDFData->m_Header.m_MagicNumber != MANIFEST_MAGIC_NUMBER)
    {
        dmLogError("Manifest format mismatch (expected '%x', actual '%x')",
            MANIFEST_MAGIC_NUMBER, manifest->m_DDFData->m_Header.m_MagicNumber);
        dmDDF::FreeMessage(manifest->m_DDF);
        return RESULT_FORMAT_ERROR;
    }

    if (manifest->m_DDFData->m_Header.m_Version != MANIFEST_VERSION)
    {
        dmLogError("Manifest version mismatch (expected '%i', actual '%i')",
            dmResourceArchive::VERSION, manifest->m_DDFData->m_Header.m_Version);
        dmDDF::FreeMessage(manifest->m_DDF);
        return RESULT_FORMAT_ERROR;
    }

    return RESULT_OK;
}

Result LoadManifest(const char* manifestPath, HFactory factory)
{
    uint32_t manifestLength = 0;
    uint8_t* manifestBuffer = 0x0;

    uint32_t dummy_file_size = 0;
    dmSys::ResourceSize(manifestPath, &manifestLength);
    dmMemory::AlignedMalloc((void**)&manifestBuffer, 16, manifestLength);
    assert(manifestBuffer);
    dmSys::Result sysResult = dmSys::LoadResource(manifestPath, manifestBuffer, manifestLength, &dummy_file_size);

    if (sysResult != dmSys::RESULT_OK)
    {
        dmLogError("Failed to read Manifest (%i)", sysResult);
        dmMemory::AlignedFree(manifestBuffer);
        return RESULT_IO_ERROR;
    }

    Result result = ParseManifestDDF(manifestBuffer, manifestLength, factory->m_Manifest);
    if (result == RESULT_OK)
        dmDDF::FreeMessage(factory->m_Manifest->m_DDF);
    dmMemory::AlignedFree(manifestBuffer);

    return result;
}

// Separate function to load manifest stored in disk instead of in bundle
Result LoadExternalManifest(const char* manifest_path, HFactory factory)
{
#if !defined(__ANDROID__)
        return LoadManifest(manifest_path, factory);
#endif

    uint32_t manifest_len = 0;
    uint8_t* manifest_buf = 0x0;

    dmLogInfo(" ### MOUNTING MANIFEST!");
    Result map_res = MountManifest(manifest_path, (void*&)manifest_buf, manifest_len);
    assert(manifest_buf);
    if (map_res != RESULT_OK)
    {
        dmLogError(" ## Failed to read Manifest (%i)", map_res);
        UnmountManifest((void*&)manifest_buf, manifest_len);
        return RESULT_IO_ERROR;
    }
    else // DEBUG PRINT
    {
        dmLogInfo("Successfully mounted manifest file: %s, size: %u", manifest_path, manifest_len);
    }

    Result result = ParseManifestDDF(manifest_buf, manifest_len, factory->m_Manifest);
    if (result == RESULT_OK)
        dmDDF::FreeMessage(factory->m_Manifest->m_DDF);
    UnmountManifest((void*&)manifest_buf, manifest_len);

    return result;
}

// -- BEGIN -- WIP DEBUG RSA decypt using public key, functions missing in axTLS lib
#include <axtls/ssl/ssl.h>
typedef struct {
    uint8_t *buffer;
    size_t size;
} DER_key;

typedef struct {
    bigint *m;              /* modulus */
    bigint *e;              /* public exponent */
    int num_octets;
    BI_CTX *bi_ctx;         /* big integer handle */
} RSA_parameters;

// TODO move to dlib axtls
/**
 * Similar to RSA_pub_key_new, rewritten to make this program depend only on bi module
 */
void RSA_pub_key_raw_new(RSA_parameters **rsa, const uint8_t *modulus, int mod_len, const uint8_t *pub_exp, int pub_len)
{
    RSA_parameters *rsa_parameters;

    BI_CTX *bi_ctx = bi_initialize();
    *rsa = (RSA_parameters *)calloc(1, sizeof(RSA_parameters));
    rsa_parameters = *rsa;
    rsa_parameters->bi_ctx = bi_ctx;
    rsa_parameters->num_octets = (mod_len & 0xFFF0);
    rsa_parameters->m = bi_import(bi_ctx, modulus, mod_len);
    bi_set_mod(bi_ctx, rsa_parameters->m, BIGINT_M_OFFSET);
    rsa_parameters->e = bi_import(bi_ctx, pub_exp, pub_len);
    bi_permanent(rsa_parameters->e);
}

// TODO move to dlib axtls
/**
 * Get the public key specifics from an ASN.1 encoded file
 * A function lacking in the axTLS API
 *
 * This is a really weird hack that only works with RSA public key
 * files
 */
static int asn1_get_public_key(const uint8_t *buf, int len, RSA_parameters **rsa_parameters)
{
    uint8_t *modulus, *pub_exp;
    int mod_len, pub_len;

    pub_len = 3;
    mod_len = len - 34;
    if (buf[0] != 0x30) {
        return -1;
    }

    pub_exp = (uint8_t *)malloc(3);
    modulus = (uint8_t *)malloc(mod_len);

    printf("mod_len: %i\n", mod_len);
    printf("pub_exp buf offset: %i\n", len - 3);

    memcpy(modulus, buf + 25, mod_len);
    // memcpy(pub_exp, buf + 31 + mod_len, 3);
    memcpy(pub_exp, buf + len - 3, 3);

    // memcpy(pub_exp, buf + 50 + mod_len, 3); 
    if (mod_len <= 0 || pub_len <= 0 )
        return -1;
    RSA_pub_key_raw_new(rsa_parameters, modulus, mod_len, pub_exp, pub_len);

    free(modulus);
    free(pub_exp);
    return 0;
}

// TODO move to dlib axtls
/**
 * Decrypts the signature buffer using the rsa public key loaded
 */
int RSA_decrypt_public(RSA_parameters *rsa, uint8_t *buffer_in, uint8_t *buffer_out)
{
    bigint *dat_bi;
    bigint *decrypted_bi;
    int byte_size;

    byte_size = rsa->num_octets; 
    if (byte_size != 128)
        return 1;
    dat_bi = bi_import(rsa->bi_ctx, buffer_in, byte_size);
    rsa->bi_ctx->mod_offset = BIGINT_M_OFFSET;
    bi_copy(rsa->m);
    decrypted_bi = bi_mod_power(rsa->bi_ctx, dat_bi, rsa->e);
    bi_export(rsa->bi_ctx, decrypted_bi, buffer_out, byte_size);
    free(dat_bi);
    free(decrypted_bi);
    return 0;
}

void bi_print(const char *label, bigint *x)
{
    int i, j;

    if (x == NULL)
    {
        printf("%s: (null)\n", label);
        return;
    }

    printf("%s: (size %d)\n", label, x->size);
    for (i = x->size-1; i >= 0; i--)
    {
        for (j = COMP_NUM_NIBBLES-1; j >= 0; j--)
        {
            comp mask = 0x0f << (j*4);
            comp num = (x->comps[i] & mask) >> (j*4);
            putc((num <= 9) ? (num + '0') : (num + 'A' - 10), stdout);
        }
    }

    printf("\n");
}
void RSA_param_print(const RSA_parameters* rsa_parameters)
{
    if (rsa_parameters == NULL)
        return;

    printf("-----------------   RSA PARAMETERS DEBUG   ----------------\n");
    printf("Size:\t%d\n", rsa_parameters->num_octets);
    bi_print("Modulus", rsa_parameters->m);
    bi_print("Public Key", rsa_parameters->e);
    printf("-----------------------------------------------------------\n");
}
// -- END -- WIP DEBUG RSA decypt using public key, functions missing in axTLS lib

// Diagram of what need to be done; https://crypto.stackexchange.com/questions/12768/why-hash-the-message-before-signing-it-with-rsa
// Inspect asn1 key; http://lapo.it/asn1js/#
Result VerifyManifest(Manifest* manifest)
{
    Result res = RESULT_INVALID_DATA;
    dmLogInfo("## VerifyManifest ##");
    char public_key_path[DMPATH_MAX_PATH];
    uint32_t public_cert_size = 0;
    uint32_t out_resource_size = 0; // not used
    uint8_t* public_cert_buf = 0x0, *public_cert_buf_flipped = 0x0;

    char game_dir[DMPATH_MAX_PATH];
    dmSys::GetResourcesPath(0, 0, game_dir, DMPATH_MAX_PATH);
    // const char* game_dir = "/Applications/eclipse/branches/42506/21869/master/build/default/Defold test.app/Contents/Resources"; // FOR LLDB DEBUGGING ONLY
    dmPath::Concat(game_dir, "game.public.der", public_key_path, DMPATH_MAX_PATH);

    // Read public key from file to buffer
    dmSys::ResourceSize(public_key_path, &public_cert_size);
    dmLogInfo("public_cert_size: %u", public_cert_size);
    public_cert_buf = (uint8_t*)malloc(public_cert_size);
    public_cert_buf_flipped = (uint8_t*)malloc(public_cert_size);


    assert(public_cert_buf);
    dmSys::Result sys_res = dmSys::LoadResource(public_key_path, public_cert_buf, public_cert_size, &out_resource_size);

    if (sys_res != dmSys::RESULT_OK)
    {
        dmLogError("Failed to verify manifest (%i)", sys_res);
        return RESULT_IO_ERROR;
    }

    // Decrypt signed manifest hash
    dmLiveUpdateDDF::HashAlgorithm signature_hash_algorithm = manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm;
    uint8_t* signature = manifest->m_DDF->m_Signature.m_Data;
    uint8_t* cert_buf = (uint8_t*)malloc(public_cert_size);

    int hash_algo_size = 128;
    if (signature_hash_algorithm == dmLiveUpdateDDF::HASH_SHA1)
        hash_algo_size = 128;
    else if (signature_hash_algorithm == dmLiveUpdateDDF::HASH_SHA256)
        hash_algo_size = 256;
    else if (signature_hash_algorithm == dmLiveUpdateDDF::HASH_SHA512)
        hash_algo_size = 512;
    dmLogInfo("hash_algo_size: %i", hash_algo_size);
    uint8_t* hash_decrypted = (uint8_t*)malloc(hash_algo_size);

    for(uint32_t i = 0; i<public_cert_size; i+=2)
    {
        memset(public_cert_buf_flipped, htons((uint16_t)*(public_cert_buf+i)), sizeof(uint16_t));
    }
    
    uint8_t* hash = new uint8_t[hash_algo_size];
    RSA_parameters* rsa_parameters;
    DER_key* derkey = new DER_key;
    derkey->buffer = public_cert_buf;
    derkey->size = public_cert_size;
    if ((asn1_get_public_key(derkey->buffer, derkey->size, &rsa_parameters)) != 0) {
        dmLogError("Call to asn1_get_public_key failed :(");
    } else {
        dmLogInfo("Call to asn1_get_public_key successful!");
        RSA_param_print(rsa_parameters);
    }
    // TODO free derkey here
    if (rsa_parameters->num_octets != hash_algo_size)
        dmLogError("The RSA public key size is not 1024 bits, actual: %i", rsa_parameters->num_octets)
    if(RSA_decrypt_public(rsa_parameters, signature, hash_decrypted)) {
        dmLogError("Decrypting signature :(");
    }
    memcpy(hash, hash_decrypted, hash_algo_size);
    // PrintHash(hash, 20);

    char* hexDigest = new char[dmResource::HashLength(signature_hash_algorithm) * 2 + 1];
    dmResource::HashToString(dmLiveUpdateDDF::HASH_SHA1, hash, hexDigest, dmResource::HashLength(signature_hash_algorithm) * 2 + 1);
    dmLogInfo("Hash printed: %s", hexDigest);
    delete[] hexDigest;
    
    free(hash_decrypted);
    free(cert_buf);
    return res;
}

Result NewArchiveIndexWithResource(Manifest* manifest, const uint8_t* hashDigest, uint32_t hashDigestLength, const dmResourceArchive::LiveUpdateResource* resource, const char* proj_id, dmResourceArchive::HArchiveIndex& out_new_index)
{
    dmResourceArchive::Result result = dmResourceArchive::NewArchiveIndexWithResource(manifest->m_ArchiveIndex, hashDigest, hashDigestLength, resource, proj_id, out_new_index);
    return (result == dmResourceArchive::RESULT_OK) ? RESULT_OK : RESULT_INVAL;
}

HFactory NewFactory(NewFactoryParams* params, const char* uri)
{
    dmMessage::HSocket socket = 0;

    dmMessage::Result mr = dmMessage::NewSocket(RESOURCE_SOCKET_NAME, &socket);
    if (mr != dmMessage::RESULT_OK)
    {
        dmLogFatal("Unable to create resource socket: %s (%d)", RESOURCE_SOCKET_NAME, mr);
        return 0;
    }

    SResourceFactory* factory = new SResourceFactory;
    memset(factory, 0, sizeof(*factory));
    factory->m_Socket = socket;

    dmURI::Result uri_result = dmURI::Parse(uri, &factory->m_UriParts);
    if (uri_result != dmURI::RESULT_OK)
    {
        dmLogError("Unable to parse uri: %s", uri);
        dmMessage::DeleteSocket(socket);
        delete factory;
        return 0;
    }

    dmLogInfo("factory->m_UriParts.m_Scheme: %s", factory->m_UriParts.m_Scheme);

    factory->m_HttpBuffer = 0;
    factory->m_HttpClient = 0;
    factory->m_HttpCache = 0;
    if (strcmp(factory->m_UriParts.m_Scheme, "http") == 0 || strcmp(factory->m_UriParts.m_Scheme, "https") == 0)
    {
        factory->m_HttpCache = 0;
        if (params->m_Flags & RESOURCE_FACTORY_FLAGS_HTTP_CACHE)
        {
            dmHttpCache::NewParams cache_params;
            char path[1024];
            dmSys::Result sys_result = dmSys::GetApplicationSupportPath("defold", path, sizeof(path));
            if (sys_result == dmSys::RESULT_OK)
            {
                // NOTE: The other http-service cache is called /http-cache
                dmStrlCat(path, "/cache", sizeof(path));
                cache_params.m_Path = path;
                dmHttpCache::Result cache_r = dmHttpCache::Open(&cache_params, &factory->m_HttpCache);
                if (cache_r != dmHttpCache::RESULT_OK)
                {
                    dmLogWarning("Unable to open http cache (%d)", cache_r);
                }
                else
                {
                    dmHttpCacheVerify::Result verify_r = dmHttpCacheVerify::VerifyCache(factory->m_HttpCache, &factory->m_UriParts, 60 * 60 * 24 * 5); // 5 days
                    // Http-cache batch verification might be unsupported
                    // We currently does not have support for batch validation in the editor http-server
                    // Batch validation was introduced when we had remote branch and latency problems
                    if (verify_r != dmHttpCacheVerify::RESULT_OK && verify_r != dmHttpCacheVerify::RESULT_UNSUPPORTED)
                    {
                        dmLogWarning("Cache validation failed (%d)", verify_r);
                    }

                    dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);
                }
            }
            else
            {
                dmLogWarning("Unable to locate application support path (%d)", sys_result);
            }
        }

        dmHttpClient::NewParams http_params;
        http_params.m_HttpHeader = &HttpHeader;
        http_params.m_HttpContent = &HttpContent;
        http_params.m_Userdata = factory;
        http_params.m_HttpCache = factory->m_HttpCache;
        factory->m_HttpClient = dmHttpClient::New(&http_params, factory->m_UriParts.m_Hostname, factory->m_UriParts.m_Port, strcmp(factory->m_UriParts.m_Scheme, "https") == 0);
        if (!factory->m_HttpClient)
        {
            dmLogError("Invalid URI: %s", uri);
            dmMessage::DeleteSocket(socket);
            delete factory;
            return 0;
        }
    }
    else if (strcmp(factory->m_UriParts.m_Scheme, "file") == 0)
    {
        // Ok
    }
    else if (strcmp(factory->m_UriParts.m_Scheme, "dmanif") == 0)
    {
        factory->m_Manifest = new Manifest();
        factory->m_ArchiveMountInfo = 0x0;

        /* DEF-2411 Check app support path for if "liveupdate.dmanifest" exists, if it does load that one instead of bundled*/
        char* manifest_path = factory->m_UriParts.m_Path;
        Result r = LoadManifest(manifest_path, factory);

        // Check if liveupdate.dmanifest exists, if it does, try to load it
        char app_support_path[DMPATH_MAX_PATH];
        char manifest_file_path[DMPATH_MAX_PATH];
        char id_buf[MANIFEST_PROJ_ID_LEN]; // String repr. of project id SHA1 hash
        HashToString(dmLiveUpdateDDF::HASH_SHA1, factory->m_Manifest->m_DDFData->m_Header.m_ProjectIdentifier.m_Data.m_Data, id_buf, MANIFEST_PROJ_ID_LEN);
        dmSys::GetApplicationSupportPath(id_buf, app_support_path, DMPATH_MAX_PATH);
        dmPath::Concat(app_support_path, "liveupdate.dmanifest", manifest_file_path, DMPATH_MAX_PATH);
        struct stat file_stat;
        bool luManifestExists = stat(manifest_file_path, &file_stat) == 0;
        if (luManifestExists)
        // if (false)
        {
            // TODO unload loaded bundled manifest?
            manifest_path = manifest_file_path;
            dmLogInfo("LiveUpdate manifest file exists! path: %s", manifest_path);
            r = LoadExternalManifest(manifest_path, factory);
        }
        else
            dmLogInfo("No external LiveUpdate manifest file found :( Looked for path: %s", manifest_file_path);

        if (r == RESULT_OK)
        {
            r = LoadArchiveIndex(manifest_path, factory->m_UriParts.m_Path, factory);
        }

        if (r != RESULT_OK)
        {
            dmLogError("Unable to load manifest: %s with result = %i", factory->m_UriParts.m_Path, r);
            dmMessage::DeleteSocket(socket);
            dmDDF::FreeMessage(factory->m_Manifest->m_DDF);
            delete factory->m_Manifest;
            delete factory;
            return 0;
        }
    }
    else
    {
        dmLogError("Invalid URI: %s", uri);
        dmMessage::DeleteSocket(socket);
        delete factory;
        return 0;
    }

    factory->m_NonSharedCount = 0;
    factory->m_ResourceTypesCount = 0;

    const uint32_t table_size = dmMath::Max(1u, (3 * params->m_MaxResources) / 4);
    factory->m_Resources = new dmHashTable<uint64_t, SResourceDescriptor>();
    factory->m_Resources->SetCapacity(table_size, params->m_MaxResources);

    factory->m_ResourceToHash = new dmHashTable<uintptr_t, uint64_t>();
    factory->m_ResourceToHash->SetCapacity(table_size, params->m_MaxResources);

    if (params->m_Flags & RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT)
    {
        factory->m_ResourceHashToFilename = new dmHashTable<uint64_t, const char*>();
        factory->m_ResourceHashToFilename->SetCapacity(table_size, params->m_MaxResources);

        factory->m_ResourceReloadedCallbacks = new dmArray<ResourceReloadedCallbackPair>();
        factory->m_ResourceReloadedCallbacks->SetCapacity(256);
    }
    else
    {
        factory->m_ResourceHashToFilename = 0;
        factory->m_ResourceReloadedCallbacks = 0;
    }

    if (params->m_ArchiveManifest.m_Size)
    {
        factory->m_BuiltinsManifest = new Manifest();
        dmDDF::Result res = dmDDF::LoadMessage(params->m_ArchiveManifest.m_Data, params->m_ArchiveManifest.m_Size, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, (void**)&factory->m_BuiltinsManifest->m_DDF);

        if (res != dmDDF::RESULT_OK)
        {
            dmLogError("Failed to load builtins manifest, result: %u", res);
        }
        else
        {
            res = dmDDF::LoadMessage(factory->m_BuiltinsManifest->m_DDF->m_Data.m_Data, factory->m_BuiltinsManifest->m_DDF->m_Data.m_Count, dmLiveUpdateDDF::ManifestData::m_DDFDescriptor, (void**)&factory->m_BuiltinsManifest->m_DDFData);
            dmResourceArchive::WrapArchiveBuffer(params->m_ArchiveIndex.m_Data, params->m_ArchiveIndex.m_Size, params->m_ArchiveData.m_Data, 0x0, 0x0, 0x0, &factory->m_BuiltinsManifest->m_ArchiveIndex);
        }
    }

    factory->m_LoadMutex = dmMutex::New();
    return factory;
}

void DeleteFactory(HFactory factory)
{
    if (factory->m_Socket)
    {
        dmMessage::DeleteSocket(factory->m_Socket);
    }
    if (factory->m_HttpClient)
    {
        dmHttpClient::Delete(factory->m_HttpClient);
    }
    if (factory->m_HttpCache)
    {
        dmHttpCache::Close(factory->m_HttpCache);
    }
    if (factory->m_LoadMutex)
    {
        dmMutex::Delete(factory->m_LoadMutex);
    }
    if (factory->m_Manifest)
    {
        if (factory->m_Manifest->m_DDFData)
        {
            dmDDF::FreeMessage(factory->m_Manifest->m_DDFData);
        }

        if (factory->m_Manifest->m_ArchiveIndex)
        {
            if (factory->m_ArchiveMountInfo)
                UnmountArchiveInternal(factory->m_Manifest->m_ArchiveIndex, factory->m_ArchiveMountInfo);
            else
                dmResourceArchive::Delete(factory->m_Manifest->m_ArchiveIndex);
        }

        delete factory->m_Manifest;
    }

    ReleaseBuiltinsManifest(factory);

    delete factory->m_Resources;
    delete factory->m_ResourceToHash;
    if (factory->m_ResourceHashToFilename)
        delete factory->m_ResourceHashToFilename;
    if (factory->m_ResourceReloadedCallbacks)
        delete factory->m_ResourceReloadedCallbacks;
    delete factory;
}

static void Dispatch(dmMessage::Message* message, void* user_ptr)
{
    HFactory factory = (HFactory) user_ptr;

    if (message->m_Descriptor)
    {
        dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
        if (descriptor == dmResourceDDF::Reload::m_DDFDescriptor)
        {
            dmResourceDDF::Reload* reload_resource = (dmResourceDDF::Reload*) message->m_Data;
            const char* resource = (const char*) ((uintptr_t) reload_resource + (uintptr_t) reload_resource->m_Resource);

            SResourceDescriptor* resource_descriptor;
            ReloadResource(factory, resource, &resource_descriptor);

        }
        else
        {
            dmLogError("Unknown message '%s' sent to socket '%s'.\n", descriptor->m_Name, RESOURCE_SOCKET_NAME);
        }
    }
    else
    {
        dmLogError("Only system messages can be sent to the '%s' socket.\n", RESOURCE_SOCKET_NAME);
    }
}

void UpdateFactory(HFactory factory)
{
    dmMessage::Dispatch(factory->m_Socket, &Dispatch, factory);
}

Result RegisterType(HFactory factory,
                           const char* extension,
                           void* context,
                           FResourcePreload preload_function,
                           FResourceCreate create_function,
                           FResourcePostCreate post_create_function,
                           FResourceDestroy destroy_function,
                           FResourceRecreate recreate_function,
                           FResourceDuplicate duplicate_function)
{
    if (factory->m_ResourceTypesCount == MAX_RESOURCE_TYPES)
        return RESULT_OUT_OF_RESOURCES;

    // Dots not allowed in extension
    if (strrchr(extension, '.') != 0)
        return RESULT_INVAL;

    if (create_function == 0 || destroy_function == 0)
        return RESULT_INVAL;

    if (FindResourceType(factory, extension) != 0)
        return RESULT_ALREADY_REGISTERED;

    SResourceType resource_type;
    resource_type.m_Extension = extension;
    resource_type.m_Context = context;
    resource_type.m_PreloadFunction = preload_function;
    resource_type.m_CreateFunction = create_function;
    resource_type.m_PostCreateFunction = post_create_function;
    resource_type.m_DestroyFunction = destroy_function;
    resource_type.m_RecreateFunction = recreate_function;
    resource_type.m_DuplicateFunction = duplicate_function;

    factory->m_ResourceTypes[factory->m_ResourceTypesCount++] = resource_type;

    return RESULT_OK;
}

uint32_t HexDigestLength(dmLiveUpdateDDF::HashAlgorithm algorithm)
{

    return dmResource::HashLength(algorithm) * 2U;
}

Result LoadFromManifest(const Manifest* manifest, const char* path, uint32_t* resource_size, LoadBufferType* buffer)
{
    // Get resource hash from path_hash
    uint32_t entry_count = manifest->m_DDFData->m_Resources.m_Count;
    dmLiveUpdateDDF::ResourceEntry* entries = manifest->m_DDFData->m_Resources.m_Data;

    int first = 0;
    int last = entry_count-1;
    uint64_t path_hash = dmHashString64(path);
    while (first <= last)
    {
        int mid = first + (last - first) / 2;
        uint64_t h = entries[mid].m_UrlHash;

        if (h == path_hash)
        {
            dmResourceArchive::EntryData ed;
            dmResourceArchive::Result res = dmResourceArchive::FindEntry(manifest->m_ArchiveIndex, entries[mid].m_Hash.m_Data.m_Data, &ed);
            if (res == dmResourceArchive::RESULT_OK)
            {
                uint32_t file_size = ed.m_ResourceSize;
                if (buffer->Capacity() < file_size)
                {
                    buffer->SetCapacity(file_size);
                }

                buffer->SetSize(0);
                dmResourceArchive::Result read_result = dmResourceArchive::Read(manifest->m_ArchiveIndex, &ed, buffer->Begin());
                if (read_result != dmResourceArchive::RESULT_OK)
                {
                    return RESULT_IO_ERROR;
                }

                buffer->SetSize(file_size);
                *resource_size = file_size;

                return RESULT_OK;
            }
            else if (res == dmResourceArchive::RESULT_NOT_FOUND)
            {
                // Resource was found in manifest, but not in archive
                return RESULT_RESOURCE_NOT_FOUND;
            }
            else
            {
                return RESULT_IO_ERROR;
            }
        }
        else if (h > path_hash)
        {
            last = mid - 1;
        }
        else if (h < path_hash)
        {
            first = mid + 1;
        }
    }

    return RESULT_RESOURCE_NOT_FOUND;
}

// Assumes m_LoadMutex is already held
static Result DoLoadResourceLocked(HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, LoadBufferType* buffer)
{
    DM_PROFILE(Resource, "LoadResource");
    if (factory->m_BuiltinsManifest)
    {
        if (LoadFromManifest(factory->m_BuiltinsManifest, original_name, resource_size, buffer) == RESULT_OK)
        {
            return RESULT_OK;
        }
    }

    // NOTE: No else if here. Fall through
    if (factory->m_HttpClient)
    {
        // Load over HTTP
        *resource_size = 0;
        factory->m_HttpBuffer = buffer;
        factory->m_HttpContentLength = -1;
        factory->m_HttpTotalBytesStreamed = 0;
        factory->m_HttpFactoryResult = RESULT_OK;
        factory->m_HttpStatus = -1;

        char uri[RESOURCE_PATH_MAX*2];
        dmURI::Encode(path, uri, sizeof(uri));

        dmHttpClient::Result http_result = dmHttpClient::Get(factory->m_HttpClient, uri);
        if (http_result != dmHttpClient::RESULT_OK)
        {
            if (factory->m_HttpStatus == 404)
            {
                return RESULT_RESOURCE_NOT_FOUND;
            }
            else
            {
                // 304 (NOT MODIFIED) is OK. 304 is returned when the resource is loaded from cache, ie ETag or similar match
                if (http_result == dmHttpClient::RESULT_NOT_200_OK && factory->m_HttpStatus != 304)
                {
                    dmLogWarning("Unexpected http status code: %d", factory->m_HttpStatus);
                    return RESULT_IO_ERROR;
                }
            }
        }

        if (factory->m_HttpFactoryResult != RESULT_OK)
            return factory->m_HttpFactoryResult;

        // Only check content-length if status != 304 (NOT MODIFIED)
        if (factory->m_HttpStatus != 304 && factory->m_HttpContentLength != -1 && factory->m_HttpContentLength != factory->m_HttpTotalBytesStreamed)
        {
            dmLogError("Expected content length differs from actually streamed for resource %s (%d != %d)", path, factory->m_HttpContentLength, factory->m_HttpTotalBytesStreamed);
        }

        *resource_size = factory->m_HttpTotalBytesStreamed;
        return RESULT_OK;
    }
    else if (factory->m_Manifest)
    {
        Result r = LoadFromManifest(factory->m_Manifest, original_name, resource_size, buffer);
        return r;
    }
    else
    {
        // Load over local file system
        uint32_t file_size;
        dmSys::Result r = dmSys::ResourceSize(path, &file_size);
        if (r != dmSys::RESULT_OK) {
            if (r == dmSys::RESULT_NOENT)
                return RESULT_RESOURCE_NOT_FOUND;
            else
                return RESULT_IO_ERROR;
        }

        if (buffer->Capacity() < file_size) {
            buffer->SetCapacity(file_size);
        }
        buffer->SetSize(0);

        r = dmSys::LoadResource(path, buffer->Begin(), file_size, &file_size);
        if (r == dmSys::RESULT_OK) {
            buffer->SetSize(file_size);
            *resource_size = file_size;
            return RESULT_OK;
        } else {
            if (r == dmSys::RESULT_NOENT)
                return RESULT_RESOURCE_NOT_FOUND;
            else
                return RESULT_IO_ERROR;
        }
    }
}

// Takes the lock.
Result DoLoadResource(HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, LoadBufferType* buffer)
{
    // Called from async queue so we wrap around a lock
    dmMutex::ScopedLock lk(factory->m_LoadMutex);
    return DoLoadResourceLocked(factory, path, original_name, resource_size, buffer);
}

// Assumes m_LoadMutex is already held
Result LoadResource(HFactory factory, const char* path, const char* original_name, void** buffer, uint32_t* resource_size)
{
    if (factory->m_Buffer.Capacity() != DEFAULT_BUFFER_SIZE) {
        factory->m_Buffer.SetCapacity(DEFAULT_BUFFER_SIZE);
    }
    factory->m_Buffer.SetSize(0);
    Result r = DoLoadResourceLocked(factory, path, original_name, resource_size, &factory->m_Buffer);
    if (r == RESULT_OK)
        *buffer = factory->m_Buffer.Begin();
    else
        *buffer = 0;
    return r;
}


static const char* GetExtFromPath(const char* name, char* buffer, uint32_t buffersize)
{
    const char* ext = strrchr(name, '.');
    if( !ext )
        return 0;

    int result = dmStrlCpy(buffer, ext, buffersize);
    if( result >= 0 )
    {
        if( buffer[result-1] == SHARED_NAME_CHARACTER )
        {
            buffer[result-1] = 0;
        }
        return buffer;
    }
    return 0;
}

/*
Tagged ("unique") resources:

A) if the original resource already exists, create a duplicate resource which points to the old resource
B) if the original does not exist: create it, and then make a duplicate
*/

static Result CreateDuplicateResource(HFactory factory, const char* canonical_path, SResourceDescriptor* rd, void** resource)
{
    if (factory->m_Resources->Full())
    {
        dmLogError("The max number of resources (%d) has been passed, tweak \"%s\" in the config file.", factory->m_Resources->Capacity(), MAX_RESOURCES_KEY);
        return RESULT_OUT_OF_RESOURCES;
    }

    char extbuffer[64];
    const char* ext = GetExtFromPath(canonical_path, extbuffer, sizeof(extbuffer));

    ext++;
    SResourceType* resource_type = FindResourceType(factory, ext);
    if (resource_type == 0)
    {
        dmLogError("Unknown resource type: %s", ext);
        return RESULT_UNKNOWN_RESOURCE_TYPE;
    }

    if (!resource_type->m_DuplicateFunction)
    {
        dmLogError("The resource type '%s' does not support duplication", ext);
        return RESULT_NOT_SUPPORTED;
    }

    char tagged_path[RESOURCE_PATH_MAX];    // The constructed unique path (if needed). E.g. "/my/icon.texturec_123"
    {
        size_t len = dmStrlCpy(tagged_path, canonical_path, sizeof(tagged_path));
        int result = DM_SNPRINTF(tagged_path+len, sizeof(tagged_path)-len, "_%u", factory->m_NonSharedCount);
        assert(result != -1);

        ++factory->m_NonSharedCount;
    }

    uint64_t tagged_path_hash = dmHashBuffer64(tagged_path, strlen(tagged_path));

    SResourceDescriptor tmp_resource;
    memset(&tmp_resource, 0, sizeof(tmp_resource));
    tmp_resource.m_NameHash = tagged_path_hash;
    tmp_resource.m_OriginalNameHash = rd->m_NameHash;
    tmp_resource.m_ReferenceCount = 1;
    tmp_resource.m_ResourceType = (void*) resource_type;
    tmp_resource.m_SharedState = DATA_SHARE_STATE_SHALLOW;

    ResourceDuplicateParams params;
    params.m_Factory = factory;
    params.m_Context = resource_type->m_Context;
    params.m_OriginalResource = rd;
    params.m_Resource = &tmp_resource;

    Result duplicate_result = resource_type->m_DuplicateFunction(params);
    if( duplicate_result != RESULT_OK )
    {
        dmLogError("Failed to duplicate resource '%s'", tagged_path);
        return duplicate_result;
    }

    ++rd->m_ReferenceCount;

    Result insert_error = InsertResource(factory, tagged_path, tagged_path_hash, &tmp_resource);
    if (insert_error != RESULT_OK)
    {
        ResourceDestroyParams params;
        params.m_Factory = factory;
        params.m_Context = resource_type->m_Context;
        params.m_Resource = &tmp_resource;
        resource_type->m_DestroyFunction(params);
        return insert_error;
    }

    *resource = tmp_resource.m_Resource;
    return RESULT_OK;
}

// Assumes m_LoadMutex is already held
static Result DoGet(HFactory factory, const char* name, void** resource)
{
    assert(name);
    assert(resource);

    DM_PROFILE(Resource, "Get");

    *resource = 0;

    char canonical_path[RESOURCE_PATH_MAX]; // The actual resource path. E.g. "/my/icon.texturec"
    GetCanonicalPath(factory->m_UriParts.m_Path, name, canonical_path);

    bool is_shared = !IsPathTagged(name);
    if( !is_shared )
    {
        int len = strlen(canonical_path);
        canonical_path[len-1] = 0;
    }

    uint64_t canonical_path_hash = dmHashBuffer64(canonical_path, strlen(canonical_path));

    // Try to get from already loaded resources
    SResourceDescriptor* rd = factory->m_Resources->Get(canonical_path_hash);
    if (rd)
    {
        assert(factory->m_ResourceToHash->Get((uintptr_t) rd->m_Resource));
        if( is_shared )
        {
            rd->m_ReferenceCount++;
            *resource = rd->m_Resource;
            return RESULT_OK;
        }

        return CreateDuplicateResource(factory, canonical_path, rd, resource);
    }

    if (factory->m_Resources->Full())
    {
        dmLogError("The max number of resources (%d) has been passed, tweak \"%s\" in the config file.", factory->m_Resources->Capacity(), MAX_RESOURCES_KEY);
        return RESULT_OUT_OF_RESOURCES;
    }

    char extbuffer[64];
    const char* ext = GetExtFromPath(canonical_path, extbuffer, sizeof(extbuffer));
    if (ext)
    {
        ext++;

        SResourceType* resource_type = FindResourceType(factory, ext);
        if (resource_type == 0)
        {
            dmLogError("Unknown resource type: %s", ext);
            return RESULT_UNKNOWN_RESOURCE_TYPE;
        }

        void *buffer;
        uint32_t file_size;
        Result result = LoadResource(factory, canonical_path, name, &buffer, &file_size);
        if (result != RESULT_OK) {
            if (result == RESULT_RESOURCE_NOT_FOUND) {
                dmLogWarning("Resource not found: %s", name);
            }
            return result;
        }

        assert(buffer == factory->m_Buffer.Begin());

        // TODO: We should *NOT* allocate SResource dynamically...
        SResourceDescriptor tmp_resource;
        memset(&tmp_resource, 0, sizeof(tmp_resource));
        tmp_resource.m_NameHash = canonical_path_hash;
        tmp_resource.m_OriginalNameHash = 0;
        tmp_resource.m_ReferenceCount = 1;
        tmp_resource.m_ResourceType = (void*) resource_type;
        tmp_resource.m_SharedState = DATA_SHARE_STATE_NONE;

        void *preload_data = 0;
        Result create_error = RESULT_OK;

        if (resource_type->m_PreloadFunction)
        {
            ResourcePreloadParams params;
            params.m_Factory = factory;
            params.m_Context = resource_type->m_Context;
            params.m_Buffer = buffer;
            params.m_BufferSize = file_size;
            params.m_PreloadData = &preload_data;
            params.m_Filename = name;
            params.m_HintInfo = 0; // No hinting now
            create_error = resource_type->m_PreloadFunction(params);
        }

        if (create_error == RESULT_OK)
        {
            ResourceCreateParams params;
            params.m_Factory = factory;
            params.m_Context = resource_type->m_Context;
            params.m_Buffer = buffer;
            params.m_BufferSize = file_size;
            params.m_PreloadData = preload_data;
            params.m_Resource = &tmp_resource;
            params.m_Filename = name;
            create_error = resource_type->m_CreateFunction(params);
        }

        if (create_error == RESULT_OK && resource_type->m_PostCreateFunction)
        {
            ResourcePostCreateParams params;
            params.m_Factory = factory;
            params.m_Context = resource_type->m_Context;
            params.m_PreloadData = preload_data;
            params.m_Resource = &tmp_resource;
            for(;;)
            {
                create_error = resource_type->m_PostCreateFunction(params);
                if(create_error != RESULT_PENDING)
                    break;
                dmTime::Sleep(1000);
            }
        }

        // Restore to default buffer size
        if (factory->m_Buffer.Capacity() != DEFAULT_BUFFER_SIZE) {
            factory->m_Buffer.SetCapacity(DEFAULT_BUFFER_SIZE);
        }
        factory->m_Buffer.SetSize(0);

        if (create_error == RESULT_OK)
        {
            Result insert_error = InsertResource(factory, name, canonical_path_hash, &tmp_resource);
            if (insert_error == RESULT_OK)
            {
                if( is_shared )
                {
                    *resource = tmp_resource.m_Resource;
                    return RESULT_OK;
                }

                Result duplicate_error = CreateDuplicateResource(factory, canonical_path, &tmp_resource, resource);
                if( duplicate_error == RESULT_OK )
                {
                    return RESULT_OK;
                }

                // If we succeeded to create the resource, but failed to duplicate it, we fail.
                insert_error = duplicate_error;
            }

            ResourceDestroyParams params;
            params.m_Factory = factory;
            params.m_Context = resource_type->m_Context;
            params.m_Resource = &tmp_resource;
            resource_type->m_DestroyFunction(params);
            return insert_error;
        }
        else
        {
            dmLogWarning("Unable to create resource: %s", canonical_path);
            return create_error;
        }
    }
    else
    {
        dmLogWarning("Unable to load resource: '%s'. Missing file extension.", name);
        return RESULT_MISSING_FILE_EXTENSION;
    }
}

Result Get(HFactory factory, const char* name, void** resource)
{
    assert(name);
    assert(resource);
    *resource = 0;

    Result chk = CheckSuppliedResourcePath(name);
    if (chk != RESULT_OK)
        return chk;

    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    dmArray<const char*>& stack = factory->m_GetResourceStack;
    if (factory->m_RecursionDepth == 0)
    {
        stack.SetSize(0);
    }

    ++factory->m_RecursionDepth;

    uint32_t n = stack.Size();
    for (uint32_t i=0;i<n;i++)
    {
        if (strcmp(stack[i], name) == 0)
        {
            dmLogError("Self referring resource detected");
            dmLogError("Reference chain:");
            for (uint32_t j = 0; j < n; ++j)
            {
                dmLogError("%d: %s", j, stack[j]);
            }
            dmLogError("%d: %s", n, name);
            --factory->m_RecursionDepth;
            return RESULT_RESOURCE_LOOP_ERROR;
        }
    }

    if (stack.Full())
    {
        stack.SetCapacity(stack.Capacity() + 16);
    }
    stack.Push(name);
    Result r = DoGet(factory, name, resource);
    stack.SetSize(stack.Size() - 1);
    --factory->m_RecursionDepth;
    return r;
}

SResourceDescriptor* GetByHash(HFactory factory, uint64_t canonical_path_hash)
{
    return factory->m_Resources->Get(canonical_path_hash);
}

Result InsertResource(HFactory factory, const char* path, uint64_t canonical_path_hash, SResourceDescriptor* descriptor)
{
    if (factory->m_Resources->Full())
    {
        dmLogError("The max number of resources (%d) has been passed, tweak \"%s\" in the config file.", factory->m_Resources->Capacity(), MAX_RESOURCES_KEY);
        return RESULT_OUT_OF_RESOURCES;
    }

    assert(descriptor->m_Resource);
    assert(descriptor->m_ReferenceCount == 1);

    factory->m_Resources->Put(canonical_path_hash, *descriptor);
    factory->m_ResourceToHash->Put((uintptr_t) descriptor->m_Resource, canonical_path_hash);
    if (factory->m_ResourceHashToFilename)
    {
        char canonical_path[RESOURCE_PATH_MAX];
        GetCanonicalPath(factory, path, canonical_path);
        factory->m_ResourceHashToFilename->Put(canonical_path_hash, strdup(canonical_path));
    }

    return RESULT_OK;
}

Result GetRaw(HFactory factory, const char* name, void** resource, uint32_t* resource_size)
{
    DM_PROFILE(Resource, "GetRaw");

    assert(name);
    assert(resource);
    assert(resource_size);

    *resource = 0;
    *resource_size = 0;

    Result chk = CheckSuppliedResourcePath(name);
    if (chk != RESULT_OK)
        return chk;

    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(factory->m_UriParts.m_Path, name, canonical_path);

    void* buffer;
    uint32_t file_size;
    Result result = LoadResource(factory, canonical_path, name, &buffer, &file_size);
    if (result == RESULT_OK) {
        *resource = malloc(file_size);
        assert(buffer == factory->m_Buffer.Begin());
        memcpy(*resource, buffer, file_size);
        *resource_size = file_size;
    }
    return result;
}

static Result DoReloadResource(HFactory factory, const char* name, SResourceDescriptor** out_descriptor)
{
    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(factory->m_UriParts.m_Path, name, canonical_path);

    uint64_t canonical_path_hash = dmHashBuffer64(canonical_path, strlen(canonical_path));

    SResourceDescriptor* rd = factory->m_Resources->Get(canonical_path_hash);

    if (out_descriptor)
        *out_descriptor = rd;

    if (rd == 0x0)
        return RESULT_RESOURCE_NOT_FOUND;

    SResourceType* resource_type = (SResourceType*) rd->m_ResourceType;
    if (!resource_type->m_RecreateFunction)
        return RESULT_NOT_SUPPORTED;

    void* buffer;
    uint32_t file_size;
    Result result = LoadResource(factory, canonical_path, name, &buffer, &file_size);
    if (result != RESULT_OK)
        return result;

    assert(buffer == factory->m_Buffer.Begin());

    ResourceRecreateParams params;
    params.m_Factory = factory;
    params.m_Context = resource_type->m_Context;
    params.m_Message = 0;
    params.m_Buffer = buffer;
    params.m_BufferSize = file_size;
    params.m_Resource = rd;
    params.m_Filename = name;
    Result create_result = resource_type->m_RecreateFunction(params);
    if (create_result == RESULT_OK)
    {
        if (factory->m_ResourceReloadedCallbacks)
        {
            for (uint32_t i = 0; i < factory->m_ResourceReloadedCallbacks->Size(); ++i)
            {
                ResourceReloadedCallbackPair& pair = (*factory->m_ResourceReloadedCallbacks)[i];
                ResourceReloadedParams params;
                params.m_UserData = pair.m_UserData;
                params.m_Resource = rd;
                params.m_Name = name;
                pair.m_Callback(params);
            }
        }
        return RESULT_OK;
    }
    else
    {
        return create_result;
    }
}

Result ReloadResource(HFactory factory, const char* name, SResourceDescriptor** out_descriptor)
{
    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    // Always verify cache for reloaded resources
    if (factory->m_HttpCache)
        dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_VERIFY);

    Result result = DoReloadResource(factory, name, out_descriptor);

    switch (result)
    {
        case RESULT_OK:
            dmLogInfo("%s was successfully reloaded.", name);
            break;
        case RESULT_OUT_OF_MEMORY:
            dmLogError("Not enough memory to reload %s.", name);
            break;
        case RESULT_FORMAT_ERROR:
        case RESULT_CONSTANT_ERROR:
            dmLogError("%s has invalid format and could not be reloaded.", name);
            break;
        case RESULT_RESOURCE_NOT_FOUND:
            dmLogError("%s could not be reloaded since it was never loaded before.", name);
            break;
        case RESULT_NOT_SUPPORTED:
            dmLogWarning("Reloading of resource type %s not supported.", ((SResourceType*)(*out_descriptor)->m_ResourceType)->m_Extension);
            break;
        default:
            dmLogWarning("%s could not be reloaded, unknown error: %d.", name, result);
            break;
    }

    if (factory->m_HttpCache)
        dmHttpCache::SetConsistencyPolicy(factory->m_HttpCache, dmHttpCache::CONSISTENCY_POLICY_TRUST_CACHE);

    return result;
}

Result SetResource(HFactory factory, uint64_t hashed_name, dmBuffer::HBuffer buffer)
{
    DM_PROFILE(Resource, "Set");

    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    assert(buffer);

    SResourceDescriptor* rd = factory->m_Resources->Get(hashed_name);
    if (!rd) {
        return RESULT_RESOURCE_NOT_FOUND;
    }

    SResourceType* resource_type = (SResourceType*) rd->m_ResourceType;
    if (!resource_type->m_RecreateFunction)
        return RESULT_NOT_SUPPORTED;

    uint32_t datasize = 0;
    void* data = 0;
    dmBuffer::GetBytes(buffer, &data, &datasize);

    assert(data);
    assert(datasize > 0);

    ResourceRecreateParams params;
    params.m_Factory = factory;
    params.m_Context = resource_type->m_Context;
    params.m_Message = 0;
    params.m_Buffer = data;
    params.m_BufferSize = datasize;
    params.m_Resource = rd;
    params.m_Filename = 0;
    params.m_NameHash = hashed_name;
    Result create_result = resource_type->m_RecreateFunction(params);
    if (create_result == RESULT_OK)
    {
    	// If it was previously shallow, it is not anymore.
    	if( rd->m_SharedState == DATA_SHARE_STATE_SHALLOW )
    	{
    		SResourceDescriptor* originalrd = factory->m_Resources->Get(rd->m_OriginalNameHash);
    		assert(originalrd);
    		assert(originalrd->m_ReferenceCount > 0);
    		originalrd->m_ReferenceCount--;
    		rd->m_OriginalNameHash = 0;
    	}

        rd->m_SharedState = DATA_SHARE_STATE_NONE; // The resource creator should now fully own the created resources

        if (factory->m_ResourceReloadedCallbacks)
        {
            for (uint32_t i = 0; i < factory->m_ResourceReloadedCallbacks->Size(); ++i)
            {
                ResourceReloadedCallbackPair& pair = (*factory->m_ResourceReloadedCallbacks)[i];
                ResourceReloadedParams params;
                params.m_UserData = pair.m_UserData;
                params.m_Resource = rd;
                params.m_Name = 0;
                params.m_NameHash = hashed_name;
                pair.m_Callback(params);
            }
        }
        return RESULT_OK;
    }
    else
    {
        return create_result;
    }

    return RESULT_OK;
}

Result SetResource(HFactory factory, uint64_t hashed_name, void* message)
{
    DM_PROFILE(Resource, "SetResource");

    dmMutex::ScopedLock lk(factory->m_LoadMutex);

    assert(message);

    SResourceDescriptor* rd = factory->m_Resources->Get(hashed_name);
    if (!rd) {
        return RESULT_RESOURCE_NOT_FOUND;
    }

    SResourceType* resource_type = (SResourceType*) rd->m_ResourceType;
    if (!resource_type->m_RecreateFunction)
        return RESULT_NOT_SUPPORTED;

    ResourceRecreateParams params;
    params.m_Factory = factory;
    params.m_Context = resource_type->m_Context;
    params.m_Message = message;
    params.m_Buffer = 0;
    params.m_BufferSize = 0;
    params.m_Resource = rd;
    params.m_Filename = 0;
    params.m_NameHash = hashed_name;
    Result create_result = resource_type->m_RecreateFunction(params);
    if (create_result == RESULT_OK)
    {
        rd->m_SharedState = DATA_SHARE_STATE_NONE; // The resource creator should now fully own the created resources

        if (factory->m_ResourceReloadedCallbacks)
        {
            for (uint32_t i = 0; i < factory->m_ResourceReloadedCallbacks->Size(); ++i)
            {
                ResourceReloadedCallbackPair& pair = (*factory->m_ResourceReloadedCallbacks)[i];
                ResourceReloadedParams params;
                params.m_UserData = pair.m_UserData;
                params.m_Resource = rd;
                params.m_Name = 0;
                params.m_NameHash = hashed_name;
                pair.m_Callback(params);
            }
        }
        return RESULT_OK;
    }
    else
    {
        return create_result;
    }

    return RESULT_OK;
}

Result GetType(HFactory factory, void* resource, ResourceType* type)
{
    assert(type);

    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    if (!resource_hash)
    {
        return RESULT_NOT_LOADED;
    }

    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    assert(rd->m_ReferenceCount > 0);
    *type = (ResourceType) rd->m_ResourceType;

    return RESULT_OK;
}

Result GetTypeFromExtension(HFactory factory, const char* extension, ResourceType* type)
{
    assert(type);

    SResourceType* resource_type = FindResourceType(factory, extension);
    if (resource_type)
    {
        *type = (ResourceType) resource_type;
        return RESULT_OK;
    }
    else
    {
        return RESULT_UNKNOWN_RESOURCE_TYPE;
    }
}

Result GetExtensionFromType(HFactory factory, ResourceType type, const char** extension)
{
    for (uint32_t i = 0; i < factory->m_ResourceTypesCount; ++i)
    {
        SResourceType* rt = &factory->m_ResourceTypes[i];

        if (((uintptr_t) rt) == type)
        {
            *extension = rt->m_Extension;
            return RESULT_OK;
        }
    }

    *extension = 0;
    return RESULT_UNKNOWN_RESOURCE_TYPE;
}

Result GetDescriptor(HFactory factory, const char* name, SResourceDescriptor* descriptor)
{
    char canonical_path[RESOURCE_PATH_MAX];
    GetCanonicalPath(factory->m_UriParts.m_Path, name, canonical_path);

    uint64_t canonical_path_hash = dmHashBuffer64(canonical_path, strlen(canonical_path));

    SResourceDescriptor* tmp_descriptor = factory->m_Resources->Get(canonical_path_hash);
    if (tmp_descriptor)
    {
        *descriptor = *tmp_descriptor;
        return RESULT_OK;
    }
    else
    {
        return RESULT_NOT_LOADED;
    }
}

void IncRef(HFactory factory, void* resource)
{
    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    assert(resource_hash);

    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    assert(rd->m_ReferenceCount > 0);
    ++rd->m_ReferenceCount;
}

// For unit testing
uint32_t GetRefCount(HFactory factory, void* resource)
{
    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    if(!resource_hash)
        return 0;
    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    return rd->m_ReferenceCount;
}

uint32_t GetRefCount(HFactory factory, dmhash_t identifier)
{
    SResourceDescriptor* rd = factory->m_Resources->Get(identifier);
    if(!rd)
        return 0;
    return rd->m_ReferenceCount;
}

void Release(HFactory factory, void* resource)
{
    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t) resource);
    assert(resource_hash);

    SResourceDescriptor* rd = factory->m_Resources->Get(*resource_hash);
    assert(rd);
    assert(rd->m_ReferenceCount > 0);
    rd->m_ReferenceCount--;

    if (rd->m_ReferenceCount == 0)
    {
        SResourceType* resource_type = (SResourceType*) rd->m_ResourceType;

        ResourceDestroyParams params;
        params.m_Factory = factory;
        params.m_Context = resource_type->m_Context;
        params.m_Resource = rd;
        resource_type->m_DestroyFunction(params);

        factory->m_ResourceToHash->Erase((uintptr_t) resource);
        factory->m_Resources->Erase(*resource_hash);
        if (factory->m_ResourceHashToFilename)
        {
            const char** s = factory->m_ResourceHashToFilename->Get(*resource_hash);
            factory->m_ResourceHashToFilename->Erase(*resource_hash);
            assert(s);
            free((void*) *s);
        }

        if( rd->m_OriginalNameHash )
        {
            SResourceDescriptor* originalrd = factory->m_Resources->Get(rd->m_OriginalNameHash);
            assert(originalrd);
            Release(factory, originalrd->m_Resource);
        }
    }
}

void RegisterResourceReloadedCallback(HFactory factory, ResourceReloadedCallback callback, void* user_data)
{
    if (factory->m_ResourceReloadedCallbacks)
    {
        if (factory->m_ResourceReloadedCallbacks->Full())
        {
            factory->m_ResourceReloadedCallbacks->SetCapacity(factory->m_ResourceReloadedCallbacks->Capacity() + 128);
        }
        ResourceReloadedCallbackPair pair;
        pair.m_Callback = callback;
        pair.m_UserData = user_data;
        factory->m_ResourceReloadedCallbacks->Push(pair);
    }
}

void UnregisterResourceReloadedCallback(HFactory factory, ResourceReloadedCallback callback, void* user_data)
{
    if (factory->m_ResourceReloadedCallbacks)
    {
        uint32_t i = 0;
        uint32_t size = factory->m_ResourceReloadedCallbacks->Size();
        while (i < size)
        {
            ResourceReloadedCallbackPair& pair = (*factory->m_ResourceReloadedCallbacks)[i];
            if (pair.m_Callback == callback && pair.m_UserData == user_data)
            {
                factory->m_ResourceReloadedCallbacks->EraseSwap(i);
                --size;
            }
            else
            {
                ++i;
            }
        }
    }
}

// If the path ends with ":", the path is not shared, i.e. you can later update to a unique resource
bool IsPathTagged(const char* name)
{
    assert(name);
    int len = strlen(name);
    return name[len-1] == SHARED_NAME_CHARACTER;
}

Result GetPath(HFactory factory, const void* resource, uint64_t* hash)
{
    uint64_t* resource_hash = factory->m_ResourceToHash->Get((uintptr_t)resource);
    if( resource_hash ) {
        *hash = *resource_hash;
        return RESULT_OK;
    }
    *hash = 0;
    return RESULT_RESOURCE_NOT_FOUND;
}


dmMutex::Mutex GetLoadMutex(const dmResource::HFactory factory)
{
    return factory->m_LoadMutex;
}

void ReleaseBuiltinsManifest(HFactory factory) 
{
    if (factory->m_BuiltinsManifest)
    {
        dmResourceArchive::Delete(factory->m_BuiltinsManifest->m_ArchiveIndex);
        dmDDF::FreeMessage(factory->m_BuiltinsManifest->m_DDFData);
        dmDDF::FreeMessage(factory->m_BuiltinsManifest->m_DDF);
        factory->m_BuiltinsManifest->m_DDFData = 0;
        factory->m_BuiltinsManifest->m_DDF = 0;
        delete factory->m_BuiltinsManifest;
        factory->m_BuiltinsManifest = 0;
    }
}

}
