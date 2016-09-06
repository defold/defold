package com.dynamo.cr.server.model;

import javax.persistence.*;

@Entity
@Table(name = "password_reset_tokens")
@NamedQueries({
        @NamedQuery(name = "PasswordResetToken.findByEmail", query = "SELECT t FROM PasswordResetToken t WHERE t.email = :email")
})
public class PasswordResetToken {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;
    @Column(nullable = false)
    private String email;
    @Column(nullable = false)
    private String tokenHash;

    public PasswordResetToken() {
    }

    public PasswordResetToken(String email, String tokenHash) {
        this.email = email;
        this.tokenHash = tokenHash;
    }

    public String getEmail() {
        return email;
    }

    public String getTokenHash() {
        return tokenHash;
    }
}
