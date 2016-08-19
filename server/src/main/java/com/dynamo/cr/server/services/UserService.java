package com.dynamo.cr.server.services;

import com.dynamo.cr.proto.Config;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AccessTokenStore;
import com.dynamo.cr.server.model.*;
import com.dynamo.cr.server.resources.ResourceUtil;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.persistence.Query;
import javax.persistence.TypedQuery;
import javax.ws.rs.core.Response;
import java.security.SecureRandom;
import java.util.*;

public class UserService {
    private static final Logger LOGGER = LoggerFactory.getLogger(UserService.class);
    @Inject
    private EntityManager entityManager;
    @Inject
    private Configuration configuration;
    @Inject
    private EmailService emailService;
    @Inject
    private SecureRandom secureRandom;

    public UserService() {
    }

    public UserService(EntityManager entityManager, Configuration configuration, EmailService emailService) {
        this.entityManager = entityManager;
        this.configuration = configuration;
        this.emailService = emailService;
    }

    public void save(User user) {
        entityManager.persist(user);
    }

    public Optional<User> find(long userId) {
        return Optional.ofNullable(entityManager.find(User.class, userId));
    }

    public Optional<User> findByEmail(String email) {
        List<User> resultList = entityManager
                .createNamedQuery("User.findByEmail", User.class)
                .setParameter("email", email)
                .getResultList();

        if (resultList.size() == 0) {
            return Optional.empty();
        }

        return Optional.of(resultList.get(0));
    }

    public User create(String key, String token) {
        int initialInvitationCount;

        String testKey = configuration.getTestRegistrationKey();
        if (testKey.equals(key)) {
            initialInvitationCount = configuration.getTestInvitationCount();
        } else {
            TypedQuery<Invitation> q = entityManager.createQuery("select i from Invitation i where i.registrationKey = :key", Invitation.class);
            List<Invitation> lst = q.setParameter("key", key).getResultList();
            if (lst.size() == 0) {
                throw new ServerException("Invalid registration key", Response.Status.UNAUTHORIZED);
            }
            Invitation invitation = lst.get(0);
            initialInvitationCount = invitation.getInitialInvitationCount();
            // Remove invitation
            entityManager.remove(lst.get(0));
        }

        List<NewUser> list = entityManager
                .createNamedQuery("NewUser.findByLoginToken", NewUser.class)
                .setParameter("loginToken", token)
                .getResultList();

        if (list.size() == 0) {
            LOGGER.error("Unable to find NewUser row for {}", token);
            throw new ServerException("Unable to register.", Response.Status.BAD_REQUEST);
        }

        NewUser newUser = list.get(0);

        // Remove prospect (if registered with different email than invitation
        Prospect p = ModelUtil.findProspectByEmail(entityManager, newUser.getEmail());
        if (p != null) {
            entityManager.remove(p);
        }

        // Generate some random password for OpenID accounts.
        byte[] passwordBytes = new byte[32];
        secureRandom.nextBytes(passwordBytes);
        String password = org.apache.commons.codec.binary.Base64.encodeBase64String(passwordBytes);

        User user = new User();
        user.setEmail(newUser.getEmail());
        user.setFirstName(newUser.getFirstName());
        user.setLastName(newUser.getLastName());
        user.setPassword(password);
        entityManager.persist(user);
        InvitationAccount account = new InvitationAccount();
        account.setUser(user);
        account.setOriginalCount(initialInvitationCount);
        account.setCurrentCount(initialInvitationCount);
        entityManager.persist(account);
        entityManager.remove(newUser);
        entityManager.flush();

        LOGGER.info("New user registered: {}", user.getEmail());

        ModelUtil.subscribeToNewsLetter(entityManager, newUser.getEmail(), newUser.getFirstName(), newUser.getLastName());

        return user;
    }

    public void remove(User user) {
        // Remove user from projects user is member of
        for (Project p : user.getProjects()) {
            p.getMembers().remove(user);
        }

        // Find all users with connections to this user and remove their connection.
        List<User> connectedUsers = entityManager
                .createQuery("SELECT u FROM User u WHERE :user MEMBER OF u.connections", User.class)
                .setParameter("user", user)
                .getResultList();
        for (User connectedUser : connectedUsers) {
            connectedUser.getConnections().remove(user);
        }

        // Remove invitation account if exists
        InvitationAccount account = entityManager.find(InvitationAccount.class, user.getId());
        if (account != null) {
            entityManager.remove(account);
        }

        // Remove access tokens
        AccessTokenStore accessTokenStore = new AccessTokenStore(() -> entityManager);
        List<AccessToken> accessTokens = accessTokenStore.find(user);
        accessTokens.stream().forEach(accessTokenStore::delete);

        entityManager.remove(user);
    }

    public void invite(String email, String inviter, String inviterEmail, int originalInvitationCount) {

        // Check for existing invite
        TypedQuery<Invitation> query = entityManager.createQuery("select i from Invitation i where i.email = :email", Invitation.class);
        List<Invitation> invitations = query.setParameter("email", email).getResultList();

        if (invitations.size() > 0) {
            String msg = String.format("The email %s is already registered. An invitation will be sent to you as soon as we can " +
                    "handle the high volume of registrations.", email);
            ResourceUtil.throwWebApplicationException(Response.Status.CONFLICT, msg);
        }

        // Remove prospects
        Prospect prospect = ModelUtil.findProspectByEmail(entityManager, email);
        if (prospect != null) {
            entityManager.remove(prospect);
        }

        String registrationKey = UUID.randomUUID().toString();

        Invitation invitation = new Invitation();
        invitation.setEmail(email);
        invitation.setInviterEmail(inviterEmail);
        invitation.setRegistrationKey(registrationKey);
        invitation.setInitialInvitationCount(getInvitationCount(originalInvitationCount));
        entityManager.persist(invitation);

        Config.EMailTemplate template = configuration.getInvitationTemplate();
        Map<String, String> params = new HashMap<>();
        params.put("inviter", inviter);
        params.put("key", registrationKey);

        emailService.send(email, template, params);

        ModelUtil.subscribeToNewsLetter(entityManager, email, "", "");
    }

    private int getInvitationCount(int originalCount) {
        for (Config.InvitationCountEntry entry : this.configuration.getInvitationCountMapList()) {
            if (entry.getKey() == originalCount) {
                return entry.getValue();
            }
        }
        return 0;
    }

    public InvitationAccount getInvitationAccount(String userId) throws ServerException {
        InvitationAccount invitationAccount = entityManager.find(InvitationAccount.class, Long.parseLong(userId));
        if (invitationAccount == null)
            throw new ServerException(String.format("No such invitation account %s", userId), javax.ws.rs.core.Response.Status.NOT_FOUND);

        return invitationAccount;
    }

    public void signup(String email, String firstName, String lastName, String token) {
        // Delete old pending NewUser entries first.
        Query deleteQuery = entityManager.createNamedQuery("NewUser.delete").setParameter("email", email);

        if (deleteQuery.executeUpdate() > 0) {
            LOGGER.info("Removed old NewUser row for {}", email);
        }

        NewUser newUser = new NewUser();
        newUser.setFirstName(firstName);
        newUser.setLastName(lastName);
        newUser.setEmail(email);
        newUser.setLoginToken(token);
        entityManager.persist(newUser);
    }

}
