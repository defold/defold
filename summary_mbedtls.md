# Mbed TLS 4.1.0 Feature Summary

## Summary

The current Mbed TLS 4.1.0 profile is at least on par with the Mbed TLS 2.17 usage Defold exposes through `sslsocket_mbedtls.cpp` for common HTTPS client cases, and it adds TLS 1.3 support.

It is not strict config parity with the broad 2.17 default profile. The current profile intentionally removes unused or legacy functionality such as DTLS, server TLS, PSK-only handshakes, uncommon curves, persistent PSA storage, and X.509 write/CSR/CRL functionality.

## Common Client Feature Comparison

| Capability | Mbed TLS 2.17 | Current Mbed TLS 4.1.0 |
|---|---:|---:|
| TLS client | yes | yes |
| TLS 1.2 | yes | yes |
| TLS 1.3 | no | yes |
| ECDHE-RSA | yes | yes |
| ECDHE-ECDSA | yes | yes |
| TLS 1.3 ECDHE | no | yes |
| TLS session tickets | yes | yes |
| TLS 1.3 PSK+ECDHE resumption | no | yes |
| X.509 certificate parsing | yes | yes |
| X.509 certificate verification | yes | yes |
| RSA certificate support | yes | yes |
| ECDSA certificate support | yes | yes |
| AES-GCM | yes | yes |
| ChaCha20-Poly1305 | yes | yes |
| AES-CCM | yes | yes |
| SHA-1/SHA-256/SHA-384/SHA-512 | yes | yes |
| MD5 for `dmCrypt` compatibility | yes | yes |
| Base64 for `dmCrypt` compatibility | yes | yes |

## Current TLS 1.3 Support

The current TLS 1.3 support is a good modern HTTPS client profile:

| TLS 1.3 Feature | Current State |
|---|---:|
| `MBEDTLS_SSL_PROTO_TLS1_3` | enabled |
| `MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE` | enabled |
| `MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_EPHEMERAL_ENABLED` | enabled |
| `MBEDTLS_SSL_SESSION_TICKETS` | enabled |
| `MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED` | enabled |
| `MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED` | disabled |
| `MBEDTLS_SSL_TICKET_C` | disabled |

This means TLS 1.3 supports certificate-authenticated ephemeral handshakes and ticket-based resumption with PSK+ECDHE. Pure PSK remains disabled, and the server-side ticket callback implementation remains disabled.

## Curves And Algorithms

Kept for common HTTPS:

| Category | Kept |
|---|---|
| Curves/groups | X25519, P-256, P-384 |
| Key exchange/signature | ECDH, ECDSA, RSA PKCS#1 v1.5 sign, RSA-PSS |
| AEAD | AES-GCM, AES-CCM, ChaCha20-Poly1305 |
| Hash/HKDF/HMAC | SHA-1, SHA-224, SHA-256, SHA-384, SHA-512, HKDF, HMAC |

Removed from the client profile:

| Removed Area | Notes |
|---|---|
| DTLS | Defold `sslsocket_mbedtls.cpp` uses stream TLS, not datagram TLS. |
| TLS server | Defold exposes a client socket backend here. |
| Pure PSK / ECDHE-PSK TLS 1.2 | No Defold API exposes PSK credentials. |
| Pure TLS 1.3 PSK | Disabled; PSK+ECDHE resumption is enabled. |
| Uncommon curves | Brainpool, secp256k1, X448, P-521 are disabled. |
| FFDH/DH | Disabled. |
| Persistent PSA storage/ITS | Disabled; Defold does not store persistent PSA keys. |
| X.509 write/CSR/CRL/PKCS7 | Not used by the runtime TLS client path. |

## Compatibility Notes

The current profile should support common modern HTTPS servers and current good practice:

- TLS 1.3 first, with TLS 1.2 fallback.
- Ephemeral ECDHE handshakes.
- Server certificate verification using RSA or ECDSA certificate chains.
- Common TLS 1.3 groups and ciphers.
- TLS 1.3 ticket resumption with PSK+ECDHE.

The main compatibility risk is very old or unusual TLS 1.2 servers requiring static RSA, DHE/FFDHE, PSK-only handshakes, uncommon curves, or legacy protocols. Those are outside the current intended Defold runtime profile.
