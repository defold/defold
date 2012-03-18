package com.dynamo.cr.server.model;

import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.JoinColumn;
import javax.persistence.OneToOne;
import javax.persistence.Table;

@Entity
@Table(name="invitation_accounts")
public class InvitationAccount {
    @Id
    @OneToOne
    @JoinColumn(nullable = false)
    private User user;

    public InvitationAccount() {
    }

    public User getUser() {
        return user;
    }

    public void setUser(User user) {
        this.user = user;
    }

    @Override
    public String toString() {
        return String.format("invitation account (%s)", user);
    }
}
