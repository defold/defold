// these are the functions we currently use

#include <mbedtls/certs.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/entropy.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/md5.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/pk_internal.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/ssl.h>

int mbedtls_ctr_drbg_seed( mbedtls_ctr_drbg_context *ctx, int (*f_entropy)(void *, unsigned char *, size_t), void *p_entropy, const unsigned char *custom, size_t len )  { return 0; }
int mbedtls_md5_finish_ret( mbedtls_md5_context *ctx, unsigned char output[16] )  { return 0; }
int mbedtls_md5_starts_ret( mbedtls_md5_context *ctx )  { return 0; }
int mbedtls_md5_update_ret( mbedtls_md5_context *ctx, const unsigned char *input, size_t ilen )  { return 0; }
int mbedtls_pk_parse_public_key( mbedtls_pk_context *ctx, const unsigned char *key, size_t keylen )  { return 0; }
int mbedtls_rsa_pkcs1_decrypt( mbedtls_rsa_context *ctx, int (*f_rng)(void *, unsigned char *, size_t), void *p_rng, int mode, size_t *olen, const unsigned char *input, unsigned char *output, size_t output_max_len )  { return 0; }
int mbedtls_sha1_finish_ret( mbedtls_sha1_context *ctx, unsigned char output[20] )  { return 0; }
int mbedtls_sha1_starts_ret( mbedtls_sha1_context *ctx )  { return 0; }
int mbedtls_sha1_update_ret( mbedtls_sha1_context *ctx, const unsigned char *input, size_t ilen )  { return 0; }
int mbedtls_sha256_finish_ret( mbedtls_sha256_context *ctx, unsigned char output[32] )  { return 0; }
int mbedtls_sha256_starts_ret( mbedtls_sha256_context *ctx, int is224 )  { return 0; }
int mbedtls_sha256_update_ret( mbedtls_sha256_context *ctx, const unsigned char *input, size_t ilen )  { return 0; }
int mbedtls_sha512_finish_ret( mbedtls_sha512_context *ctx, unsigned char output[64] )  { return 0; }
int mbedtls_sha512_starts_ret( mbedtls_sha512_context *ctx, int is384 )  { return 0; }
int mbedtls_sha512_update_ret( mbedtls_sha512_context *ctx, const unsigned char *input, size_t ilen )  { return 0; }
int mbedtls_ssl_close_notify( mbedtls_ssl_context *ssl )  { return 0; }
int mbedtls_ssl_config_defaults( mbedtls_ssl_config *conf, int endpoint, int transport, int preset )  { return 0; }
int mbedtls_ssl_handshake( mbedtls_ssl_context *ssl )  { return 0; }
int mbedtls_ssl_read( mbedtls_ssl_context *ssl, unsigned char *buf, size_t len )  { return 0; }
int mbedtls_ssl_set_hostname( mbedtls_ssl_context *ssl, const char *hostname )  { return 0; }
int mbedtls_ssl_setup( mbedtls_ssl_context *ssl, const mbedtls_ssl_config *conf )  { return 0; }
int mbedtls_ssl_write( mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len )  { return 0; }
int mbedtls_x509_crt_verify_info( char *buf, size_t size, const char *prefix, uint32_t flags )  { return 0; }
size_t mbedtls_rsa_get_len( const mbedtls_rsa_context *ctx ) { return 0; }
void mbedtls_ctr_drbg_free( mbedtls_ctr_drbg_context *ctx ) {}
void mbedtls_ctr_drbg_init( mbedtls_ctr_drbg_context *ctx ) {}
void mbedtls_debug_set_threshold( int threshold ) {}
void mbedtls_entropy_free( mbedtls_entropy_context *ctx ) {}
void mbedtls_entropy_init( mbedtls_entropy_context *ctx ) {}
int mbedtls_entropy_func( void *data, unsigned char *output, size_t len ) { return 0; }
void mbedtls_md5_clone( mbedtls_md5_context *dst, const mbedtls_md5_context *src ) {}
void mbedtls_md5_free( mbedtls_md5_context *ctx ) {}
void mbedtls_md5_init( mbedtls_md5_context *ctx ) {}
void mbedtls_net_free( mbedtls_net_context *ctx ) {}
void mbedtls_net_init( mbedtls_net_context *ctx ) {}
void mbedtls_pk_free( mbedtls_pk_context *ctx ) {}
void mbedtls_pk_init( mbedtls_pk_context *ctx ) {}
void mbedtls_sha1_clone( mbedtls_sha1_context *dst, const mbedtls_sha1_context *src ) {}
void mbedtls_sha1_free( mbedtls_sha1_context *ctx ) {}
void mbedtls_sha1_init( mbedtls_sha1_context *ctx ) {}
void mbedtls_sha256_clone( mbedtls_sha256_context *dst, const mbedtls_sha256_context *src ) {}
void mbedtls_sha256_free( mbedtls_sha256_context *ctx ) {}
void mbedtls_sha256_init( mbedtls_sha256_context *ctx ) {}
void mbedtls_sha512_clone( mbedtls_sha512_context *dst, const mbedtls_sha512_context *src ) {}
void mbedtls_sha512_free( mbedtls_sha512_context *ctx ) {}
void mbedtls_sha512_init( mbedtls_sha512_context *ctx ) {}
void mbedtls_ssl_conf_authmode( mbedtls_ssl_config *conf, int authmode ) {}
void mbedtls_ssl_conf_dbg( mbedtls_ssl_config *conf, void (*f_dbg)(void *, int, const char *, int, const char *), void  *p_dbg ) {}
void mbedtls_ssl_conf_handshake_timeout( mbedtls_ssl_config *conf, uint32_t min, uint32_t max ) {}
void mbedtls_ssl_conf_rng( mbedtls_ssl_config *conf, int (*f_rng)(void *, unsigned char *, size_t), void *p_rng ) {}
void mbedtls_ssl_config_free( mbedtls_ssl_config *conf ) {}
void mbedtls_ssl_config_init( mbedtls_ssl_config *conf ) {}
void mbedtls_ssl_free( mbedtls_ssl_context *ssl ) {}
void mbedtls_ssl_init( mbedtls_ssl_context *ssl ) {}
void mbedtls_ssl_set_bio( mbedtls_ssl_context *ssl, void *p_bio, mbedtls_ssl_send_t *f_send, mbedtls_ssl_recv_t *f_recv, mbedtls_ssl_recv_timeout_t *f_recv_timeout ) {}
uint32_t mbedtls_ssl_get_verify_result( const mbedtls_ssl_context *ssl ) { return 0; }
int mbedtls_net_recv( void *ctx, unsigned char *buf, size_t len ) { return 0; }
int mbedtls_net_send( void *ctx, const unsigned char *buf, size_t len ) { return 0; }
int mbedtls_ctr_drbg_random( void *p_rng, unsigned char *output, size_t output_len ) { return 0; }
