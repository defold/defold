package com.dynamo.cr.server.model;

import java.util.HashSet;
import java.util.Set;

import javax.persistence.Entity;
import javax.persistence.Id;
import javax.persistence.ManyToOne;
import javax.persistence.OneToMany;
import javax.persistence.Table;

@Entity
@Table(name="projects")
public class Project {

    @Id
    @ManyToOne
    private User owner;

    @Id
    private String name;

    @OneToMany
    private Set<User> users = new HashSet<User>();

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

    public Set<User> getUsers() {
        return users;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getName(), getOwner().getEmail());
    }

}
