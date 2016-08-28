package com.dynamo.cr.server.model;

import com.dynamo.cr.server.auth.PasswordHashGenerator;

import javax.persistence.*;
import java.util.Date;
import java.util.HashSet;
import java.util.Set;

@Entity
@Table(name = "users")
@NamedQueries({
        @NamedQuery(name = "User.findByEmail", query = "SELECT u FROM User u WHERE u.email = :email")
})
public class User {

    public enum Role {
        USER,
        ADMIN,
        ANONYMOUS,
    }

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(unique = true, nullable = false)
    private String email;

    @Column(nullable = false)
    private String password;

    @Column(nullable = false)
    private String firstName;

    @Column(nullable = false)
    private String lastName;

    @Column(nullable = false)
    @Enumerated(EnumType.STRING)
    private Role role = Role.USER;

    @Column(nullable = false)
    @Temporal(TemporalType.TIMESTAMP)
    private Date registrationDate = new Date();

    @OneToMany(cascade = {CascadeType.PERSIST})
    private Set<Project> projects = new HashSet<>();

    @OneToMany
    private Set<User> connections = new HashSet<>();

    public Long getId() {
        return id;
    }

    public String getEmail() {
        return email;
    }

    public void setEmail(String email) {
        this.email = email;
    }

    /**
     * Calculates the password hash and stores that in order to never store clear text passwords in the database.
     *
     * @param password Clear text password.
     */
    public void setPassword(String password) {
        this.password = PasswordHashGenerator.generateHash(password);
    }

    /**
     * Validates a clear text password towards hashed password retrieved from the database.
     *
     * @param password Clear text password.
     * @return True if password is correct, false otherwise.
     */
    public boolean authenticate(String password) {
        return PasswordHashGenerator.validatePassword(password, this.password);
    }

    public String getFirstName() {
        return firstName;
    }

    public void setFirstName(String firstName) {
        this.firstName = firstName;
    }

    public String getLastName() {
        return lastName;
    }

    public void setLastName(String lastName) {
        this.lastName = lastName;
    }

    public Role getRole() {
        return role;
    }

    public void setRole(Role role) {
        this.role = role;
    }

    public Set<Project> getProjects() {
        return projects;
    }

    public Set<User> getConnections() {
        return connections;
    }

    @Override
    public String toString() {
        if (getId() == null) {
            return "anonymous";
        } else {
            return String.format("%s (%d)", email, getId());
        }
    }
}
