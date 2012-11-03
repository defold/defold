package com.dynamo.cr.server.openid;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URLEncoder;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import javax.ws.rs.core.UriBuilder;

import org.apache.commons.codec.binary.Base64;
import org.apache.http.NameValuePair;
import org.apache.http.client.utils.URLEncodedUtils;
import org.eclipse.jetty.util.MultiMap;

import com.dynamo.cr.common.cache.LRUCache;

public class OpenID {

    // Keys that are required. All these keys must also be signed. This validation is performed in verify() below.
    private static final String[] requiredKeys = new String[] {
        "openid.assoc_handle",
        "openid.claimed_id",
        "openid.identity",
        "openid.op_endpoint",
        "openid.response_nonce",
        "openid.return_to" };

    private Map<String, EndPoint> endPoints = new HashMap<String, EndPoint>();
    private Map<URL, Association> associations = new HashMap<URL, Association>();

    private LRUCache<String, Association> assocHandleToAssociation = new LRUCache<String, Association>(128);
    private IEndPointResolver endPointResolver = new EndPointResolver();
    private IAssociator associator = new Associator();

    public OpenID() {
    }

    private EndPoint getEndPoint(String provider) throws IOException, OpenIDException {
        EndPoint ep = endPoints.get(provider);
        if (ep != null && !ep.isExpired()) {
            return ep;
        } else {
            ep = endPointResolver.resolve(provider);
            endPoints.put(provider, ep);
            return ep;
        }
    }

    private Association associate(EndPoint endPoint) throws IOException, OpenIDException, IllegalArgumentException, URISyntaxException {
        Association association = associations.get(endPoint.getUrl());
        if (association != null && !association.isExpired()) {
            return association;
        }

        association = associator.associate(endPoint);
        associations.put(endPoint.getUrl(), association);
        assocHandleToAssociation.put(association.get(Association.Key.assoc_handle), association);
        return association;
    }

    /**
     * Get OpenID authentication URL for provider
     * @param provider OpenID provider alias, e.g. google. End-point resolution is delegated to {@link IEndPointResolver}.
     * @param realm realm
     * @param returnTo return to URL
     * @return authentication URL
     * @throws OpenIDException
     */
    public String getAuthenticationUrl(String provider, String realm, String returnTo) throws OpenIDException {
        try {
            EndPoint endPoint;
            Association association;
            synchronized (this) {
                endPoint = getEndPoint(provider);
                association = associate(endPoint);
            }

            String url = UriBuilder.fromUri(endPoint.getUrl().toString())
                .queryParam("openid.ns", "http://specs.openid.net/auth/2.0")
                .queryParam("openid.claimed_id", "http://specs.openid.net/auth/2.0/identifier_select")
                .queryParam("openid.identity", "http://specs.openid.net/auth/2.0/identifier_select")
                .queryParam("openid.mode", "checkid_setup")
                .queryParam("openid.ns.ax", "http://openid.net/srv/ax/1.0")
                .queryParam("openid.ax.mode", "fetch_request")
                .queryParam("openid.ax.type.email", "http://axschema.org/contact/email")
                .queryParam("openid.ax.type.fullname", "http://axschema.org/namePerson")
                .queryParam("openid.ax.type.alias", "http://axschema.org/namePerson/friendly")
                .queryParam("openid.ax.type.language", "http://axschema.org/pref/language")
                .queryParam("openid.ax.type.firstname", "http://axschema.org/namePerson/first")
                .queryParam("openid.ax.type.lastname", "http://axschema.org/namePerson/last")
                .queryParam("openid.ax.required", "email,fullname,language,firstname,lastname")
                .queryParam("openid.return_to", URLEncoder.encode(returnTo, "UTF-8"))
                .queryParam("openid.assoc_handle", association.get(Association.Key.assoc_handle))
                .queryParam("openid.realm", realm)
                .build().toString();

            return url;

        } catch (IllegalArgumentException e) {
            throw new OpenIDException(e);
        } catch (URISyntaxException e) {
            throw new OpenIDException(e);
        } catch (IOException e) {
            throw new OpenIDException(e);
        }
    }

    /**
     * Verify that the OpenID provider authentication response is properly signed. {@link OpenIDException} is raised if validation fails.
     * @param uri Response URI to verify
     * @return validated {@link OpenIDIdentity}
     * @throws OpenIDException
     */
    public OpenIDIdentity verify(URI uri) throws OpenIDException {
        MultiMap<String> queryParams = new MultiMap<String>();

        Pattern nsPattern = Pattern.compile("openid\\.ns\\.(.*?)");
        String axNameSpace = null;

        List<NameValuePair> nameValueList = URLEncodedUtils.parse(uri, "UTF-8");
        for (NameValuePair pair : nameValueList) {
            queryParams.put(pair.getName(), pair.getValue());

            Matcher matcher = nsPattern.matcher(pair.getName());
            if (matcher.matches() && pair.getValue().equals("http://openid.net/srv/ax/1.0")) {
                axNameSpace = matcher.group(1);
            }
        }

        if (axNameSpace == null) {
            throw new OpenIDException(String.format("Authentication is missing required ax namespace"));
        }

        for (String key : requiredKeys) {
            if (!queryParams.containsKey(key)) {
                throw new OpenIDException(String.format("Authentication is missing required key '%s'", key));
            }
        }
        String assocHandle = queryParams.getString("openid.assoc_handle");

        Association association;
        synchronized (this) {
            association = assocHandleToAssociation.get(assocHandle);
        }

        if (association == null) {
            throw new OpenIDException("Invalid authentication request");
        }
        byte[] macKey = association.getKey();

        List<String> toSignKeysList = Arrays.asList(queryParams.getString("openid.signed").split(","));
        Set<String> toSignKeys = new HashSet<String>(toSignKeysList);

        // Ensure that all values that we trust are signed
        for (String key : requiredKeys) {
            if (!toSignKeys.contains(key.substring("openid.".length()))) {
                throw new OpenIDException(String.format("Required key '%s' is not signed", key));
            }
        }

        if (!toSignKeys.contains(axNameSpace + "." + "value.email")) {
            throw new OpenIDException(String.format("Required key value.email is not signed"));
        }

        if (!toSignKeys.contains(axNameSpace + "." + "value.language")) {
            throw new OpenIDException(String.format("Required key value.language is not signed"));
        }

        /*
         * NOTE: We don't check that firstname, lastname or fullname are correctly signed
         * It doesn't really matter. People could fake their names anyway.
         */
        String firstName = queryParams.getString("openid." + axNameSpace + ".value.firstname");
        String lastName = queryParams.getString("openid." + axNameSpace + ".value.lastname");
        if (firstName == null) {
            /*
             * Yahoo is "special" and will return fullname instead of first and last
             * Yahoo can even skip the fullname in you have an really old account
             * with no names filled in.
             */
            String fullName = queryParams.getString("openid." + axNameSpace + ".value.fullname");
            if (fullName != null) {
                String[] lst = fullName.split(" ", 2);
                if (lst.length > 0) {
                    firstName = lst[0];
                } else {
                    firstName = "";
                }
                if (lst.length > 1) {
                    lastName = lst[1];
                } else {
                    lastName = "";
                }
            } else {
                firstName = "";
                lastName = "";
            }
        }

        StringBuilder toSign = new StringBuilder(1024);
        for (String key : toSignKeysList) {
            String value = queryParams.getString("openid." + key);
            if (value == null) {
                throw new OpenIDException(String.format("Authentication is missing required key '%s'", key));
            }
            toSign.append(key).append(":").append(value).append("\n");
        }

        SecretKeySpec signingKey = new SecretKeySpec(macKey, "HmacSHA1");
        Mac mac = null;
        try {
            mac = Mac.getInstance("HmacSHA1");
            mac.init(signingKey);

            byte[] signedMessage = mac.doFinal(toSign.toString().getBytes("UTF-8"));
            byte[] sign = Base64.decodeBase64(queryParams.getString("openid.sig"));

            if (Arrays.equals(sign, signedMessage)) {
                OpenIDIdentity ret = new OpenIDIdentity();
                // NOTE: All these keys, aport from name(s), must be present in requiredKeys in order to be trusted

                ret.setIdentity(queryParams.getString("openid.identity"));
                ret.setEmail(queryParams.getString("openid." + axNameSpace + ".value.email"));
                ret.setFirstName(firstName);
                ret.setLastName(lastName);
                ret.setLanguage(queryParams.getString("openid." + axNameSpace + ".value.language"));
                return ret;
            } else {
                throw new OpenIDException("Authentication failed. Signature validation failed.");
            }
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        } catch (InvalidKeyException e) {
            throw new OpenIDException(e);
        } catch (IllegalStateException e) {
            throw new OpenIDException(e);
        } catch (UnsupportedEncodingException e) {
            throw new OpenIDException(e);
        }
    }

    /**
     * Set new {@link IEndPointResolver}
     * @param resolver {@link IEndPointResolver} to set
     */
    public void setEndPointResolver(IEndPointResolver resolver) {
        this.endPointResolver = resolver;
    }

    /**
     * Set new {@link IAssociator}
     * @param associator {@link IAssociator} to set
     */
    public void setAssociator(IAssociator associator) {
        this.associator = associator;
    }

}
