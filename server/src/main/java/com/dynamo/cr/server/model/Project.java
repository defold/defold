package com.dynamo.cr.server.model;

import java.util.Date;
import java.util.HashSet;
import java.util.Set;

import javax.persistence.Column;
import javax.persistence.Entity;
import javax.persistence.GeneratedValue;
import javax.persistence.GenerationType;
import javax.persistence.Id;
import javax.persistence.OneToMany;
import javax.persistence.OneToOne;
import javax.persistence.Table;
import javax.persistence.Temporal;
import javax.persistence.TemporalType;

import com.dynamo.cr.server.model.User.Role;

@Entity
@Table(name="projects")
public class Project {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @OneToOne(optional = false)
    private User owner;

    @Column(nullable = false)
    private String name;

    @Column(nullable = false)
    private String description;

    @Temporal(TemporalType.TIMESTAMP)
    private Date created = new Date();

    @Temporal(TemporalType.TIMESTAMP)
    private Date lastUpdated = new Date();

    @OneToMany
    private Set<User> members = new HashSet<User>();

    public Long getId() {
        return id;
    }

    public User getOwner() {
        return owner;
    }

    public void setOwner(User owner) {
        this.owner = owner;
    }

    public String getName() {
        return name;
    }

    public void setName(String name) {
        this.name = name;
    }

    public String getDescription() {
        return description;
    }

    public void setDescription(String description) {
        this.description = description;
    }

    public Date getCreated() {
        return created;
    }

    public Date getLastUpdated() {
        return lastUpdated;
    }

    public void setLastUpdated(Date lastUpdated) {
        this.lastUpdated = lastUpdated;
    }

    public Set<User> getMembers() {
        return members;
    }

    public int getMemberCount() {
        int memberCount = 0;
        for (User u : this.members) {
            if (u.getRole() != Role.ADMIN) {
                ++memberCount;
            }
        }
        return memberCount;
    }

    @Override
    public String toString() {
        return String.format("%s (%d) (%s)", getName(), getId(), getOwner().getEmail());
    }

}
