package com.dynamo.cr.server.model;

import javax.persistence.Column;
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

    @Column(nullable = false)
    private int originalCount;

    @Column(nullable = false)
    private int currentCount;

    public InvitationAccount() {
    }

    public User getUser() {
        return user;
    }

    public void setUser(User user) {
        this.user = user;
    }

    public int getOriginalCount() {
        return this.originalCount;
    }

    public void setOriginalCount(int originalCount) {
        this.originalCount = originalCount;
    }

    public int getCurrentCount() {
        return this.currentCount;
    }

    public void setCurrentCount(int currentCount) {
        this.currentCount = currentCount;
    }

    @Override
    public String toString() {
        return String.format("invitation account (%s)", user);
    }
}
