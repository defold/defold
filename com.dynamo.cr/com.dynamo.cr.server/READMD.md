# Defold Server

## SSL Certificates

Example based on certificates provided by Trustwave.

    # Combine root and intermediate certificates
    cat SecureTrustCA.txt TrustwaveOrganizationValidationCA,Level2.txt > chain.txt

    # Create a p12 bundles of certificates and private key
    openssl pkcs12 -export -chain -in \*-defold-com/STAR.defold.com.pem -inkey key.txt -out defold.p12 -name defold -CAfile chain.txt

    # Import bundle to keystore
    keytool -importkeystore -srckeystore defold.p12 -srcstoretype PKCS12

Copy the output-file, ~/.keystore, to server
