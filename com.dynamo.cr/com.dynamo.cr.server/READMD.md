# Defold Server

## REST API

**NOTES**

The `user-id` present in the majority of the REST API:s is almost always not used
anymore. It was a mistake to include the `user-id` in the URL as URL:s should
be universal.
The interpretation of the `user-id` should be changed to `owner-id`. This
change is related to namespaces and user-friendly id:s. Compare with github.
Note that namespaces isn't currently required as project-id:s are globally unique but
with support for namespaces, similar to github, we must include `owner-id` in the URL.

## SSL Certificates

Example based on certificates provided by Trustwave.

    # Combine root and intermediate certificates
    cat SecureTrustCA.txt TrustwaveOrganizationValidationCA,Level2.txt > chain.txt

    # Create a p12 bundles of certificates and private key
    openssl pkcs12 -export -chain -in \*-defold-com/STAR.defold.com.pem -inkey key.txt -out defold.p12 -name defold -CAfile chain.txt

    # Import bundle to keystore
    keytool -importkeystore -srckeystore defold.p12 -srcstoretype PKCS12

Copy the output-file, ~/.keystore, to server
