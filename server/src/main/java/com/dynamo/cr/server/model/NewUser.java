package com.dynamo.cr.server.model;

import com.dynamo.cr.server.auth.PasswordHashGenerator;

import javax.persistence.*;

@Entity
@Table(name="new_users")
@NamedQueries({
        @NamedQuery(name = "NewUser.delete", query = "DELETE FROM NewUser u WHERE u.email = :email"),
        @NamedQuery(name = "NewUser.findByLoginToken", query = "SELECT u FROM NewUser u WHERE u.loginToken = :loginToken")
})
public class NewUser {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(unique = true, nullable = false)
    private String loginToken;

    @Column(unique = true, nullable = false)
    private String email;

    @Column(nullable = false)
    private String firstName;

    @Column(nullable = false)
    private String lastName;

    @Column
    private String password;

    public Long getId() {
        return id;
    }

    public String getLoginToken() {
        return loginToken;
    }

    public void setLoginToken(String loginToken) {
        this.loginToken = loginToken;
    }

    public String getEmail() {
        return email;
    }

    public void setEmail(String email) {
        this.email = email;
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

    public String getPassword() {
        return password;
    }

    public void setPassword(String password) {
        this.password = PasswordHashGenerator.generateHash(password);
    }

    @Override
    public String toString() {
        return String.format("%s (%d)", email, getId());
    }

}
