package com.dynamo.cr.server.model;

import java.util.Date;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.GeneratedValue;
import javax.persistence.GenerationType;
import javax.persistence.Id;
import javax.persistence.Table;
import javax.persistence.Temporal;
import javax.persistence.TemporalType;

@Entity
@Table(name="invitations")
public class Invitation {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false, unique = true)
    private String email;

    @Column(nullable = false)
    private String inviterEmail;

    @Column(nullable = false, unique = true)
    private String registrationKey;

    @Column(nullable = false)
    private int initialInvitationCount;

    @Temporal(TemporalType.TIMESTAMP)
    private Date date = new Date();

    public Invitation() {
    }

    public Long getId() {
        return id;
    }

    public String getEmail() {
        return email;
    }

    public void setEmail(String email) {
        this.email = email;
    }

    public String getInviterEmail() {
        return inviterEmail;
    }

    public void setInviterEmail(String inviterEmail) {
        this.inviterEmail = inviterEmail;
    }

    public String getRegistrationKey() {
        return registrationKey;
    }

    public void setRegistrationKey(String registrationKey) {
        this.registrationKey = registrationKey;
    }

    public int getInitialInvitationCount() {
        return this.initialInvitationCount;
    }

    public void setInitialInvitationCount(int initialInvitationCount) {
        this.initialInvitationCount = initialInvitationCount;
    }

    public Date getDate() {
        return date;
    }

    @Override
    public String toString() {
        return String.format("invitation (%s, %s)", email, inviterEmail);
    }
}
