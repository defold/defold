package org.expressme.openid;

import java.io.Serializable;

/**
 * Authentication information returned from OpenID Provider.
 * 
 * @author Michael Liao (askxuefeng@gmail.com)
 * @author Erwin Quinto (erwin.quinto@gmail.com)
 */
public class Authentication implements Serializable {

    private static final long serialVersionUID = -7031455449710566518L;

    private String identity;

    private String email;

    private String fullname;

    private String firstname;

    private String lastname;

    private String language;

    private String gender;

    /**
     * Get identity.
     */
    public String getIdentity() {
        return identity;
    }

    /**
     * Set identity.
     */
    public void setIdentity(String identity) {
        this.identity = identity;
    }

    /**
     * Get email.
     */
    public String getEmail() {
        return email;
    }

    /**
     * Set email.
     */
    public void setEmail(String email) {
        this.email = email;
    }

    /**
     * Get fullname.
     */
    public String getFullname() {
        return fullname;
    }

    /**
     * Set fullname.
     */
    public void setFullname(String fullname) {
        this.fullname = fullname;
    }

    /**
     * Get firstname.
     */
    public String getFirstname() {
        return firstname;
    }

    /**
     * Set firstname.
     */
    public void setFirstname(String firstname) {
        this.firstname = firstname;
    }

    /**
     * Get lastname.
     */
    public String getLastname() {
        return lastname;
    }

    /**
     * Set lastname.
     */
    public void setLastname(String lastname) {
        this.lastname = lastname;
    }

    /**
     * Get language.
     */
    public String getLanguage() {
        return language;
    }

    /**
     * Set language.
     */
    public void setLanguage(String language) {
        this.language = language;
    }

    /**
     * Get gender.
     */
    public String getGender() {
        return gender;
    }

    /**
     * Set gender.
     */
    public void setGender(String gender) {
        this.gender = gender;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder(1024);
        sb.append("Authentication [")
          .append("identity:").append(identity).append(", ")
          .append("email:").append(email).append(", ")
          .append("fullname:").append(fullname).append(", ")
          .append("language:").append(language).append(", ")
          .append("gender:").append(gender).append(']');
        return sb.toString();
    }
}
