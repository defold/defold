package com.dynamo.cr.server.model;

import javax.persistence.*;
import java.util.Date;

@Entity
@Table(name = "access_tokens")
@NamedQueries({
        @NamedQuery(name = "AccessToken.findUserTokens", query = "SELECT a FROM AccessToken a WHERE a.user = :user")
})
public class AccessToken {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;
    @OneToOne(optional = false)
    private User user;
    @Column
    private String tokenHash;
    @Temporal(TemporalType.TIMESTAMP)
    private Date expires;
    @Temporal(TemporalType.TIMESTAMP)
    private Date created;
    @Temporal(TemporalType.TIMESTAMP)
    private Date lastUsed;
    @Column
    private String ip;

    public AccessToken() {
    }

    public AccessToken(User user, String tokenHash, Date expires, Date created, Date lastUsed, String ip) {
        this.user = user;
        this.tokenHash = tokenHash;
        this.expires = expires;
        this.created = created;
        this.lastUsed = lastUsed;
        this.ip = ip;
    }

    public void setExpires(Date expires) {
        this.expires = expires;
    }

    public Date getExpires() {
        return expires;
    }

    public void setLastUsed(Date lastUsed) {
        this.lastUsed = lastUsed;
    }

    public void setIp(String ip) {
        this.ip = ip;
    }

    public String getTokenHash() {
        return tokenHash;
    }

    public boolean isLifetime() {
        return expires == null;
    }
}