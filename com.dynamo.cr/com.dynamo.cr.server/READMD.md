# Defold Server

## SSL Certificates

Example based on certificates provided by Trustwave.

    # Combine and bundle cert and key
    openssl pkcs12 -export -in \*-defold-com/STAR.defold.com.pem -inkey key.txt -out defold.p12 -name defold

    # Import intermediate certificate
    keytool -import -trustcacerts  -file \*-defold-com/chain.cer  -alias root

    # Import cert/key bundle
    keytool -importkeystore -srckeystore defold.p12 -srcstoretype PKCS12
