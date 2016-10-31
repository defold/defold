package com.dynamo.cr.server.model;

import com.dynamo.cr.server.model.User.Role;

import javax.persistence.*;
import java.util.Date;
import java.util.HashSet;
import java.util.Set;

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
    private Set<User> members = new HashSet<>();

    @PreRemove
    private void preRemove() {
        // Remove project from members' project lists before removing the project.
        for (User member : members) {
            member.getProjects().remove(this);
        }
    }

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
