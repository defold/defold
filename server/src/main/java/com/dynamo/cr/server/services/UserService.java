package com.dynamo.cr.server.services;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.ServerException;
import com.dynamo.cr.server.auth.AccessTokenStore;
import com.dynamo.cr.server.auth.PasswordHashGenerator;
import com.dynamo.cr.server.model.*;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.inject.Inject;
import javax.persistence.EntityManager;
import javax.persistence.Query;
import javax.ws.rs.core.Response;
import java.security.SecureRandom;
import java.util.HashMap;
import java.util.List;
import java.util.Optional;
import java.util.UUID;

public class UserService {
    private static final Logger LOGGER = LoggerFactory.getLogger(UserService.class);
    @Inject
    private EntityManager entityManager;
    @Inject
    private SecureRandom secureRandom;
    @Inject
    private EmailService emailService;
    @Inject
    private Configuration configuration;

    public UserService() {
    }

    public UserService(EntityManager entityManager) {
        this.entityManager = entityManager;
    }

    public void save(User user) {
        entityManager.persist(user);
    }

    public Optional<User> find(long userId) {
        return Optional.ofNullable(entityManager.find(User.class, userId));
    }

    public Optional<User> findByEmail(String email) {

        // Always search for lowercase address.
        email = email.toLowerCase();

        List<User> resultList = entityManager
                .createNamedQuery("User.findByEmail", User.class)
                .setParameter("email", email)
                .getResultList();

        if (resultList.size() == 0) {
            return Optional.empty();
        }

        return Optional.of(resultList.get(0));
    }

    public User create(String token) {

        List<NewUser> list = entityManager
                .createNamedQuery("NewUser.findByLoginToken", NewUser.class)
                .setParameter("loginToken", token)
                .getResultList();

        if (list.size() == 0) {
            LOGGER.error("Failed to create user. Sign-up with token {} not found.", token);
            throw new ServerException("Unable to register.", Response.Status.BAD_REQUEST);
        }

        NewUser newUser = list.get(0);

        User user = new User();
        user.setEmail(newUser.getEmail());
        user.setFirstName(newUser.getFirstName());
        user.setLastName(newUser.getLastName());

        if (newUser.getPassword() == null) {
            // Generate some random password for OpenID accounts.
            // TODO: There is no really good reason to set a random password that the user doesn't know.
            byte[] passwordBytes = new byte[32];
            secureRandom.nextBytes(passwordBytes);
            String password = org.apache.commons.codec.binary.Base64.encodeBase64String(passwordBytes);
            user.setPassword(password);
        } else {
            user.setHashedPassword(newUser.getPassword());
        }

        entityManager.persist(user);
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

        // Remove access tokens
        AccessTokenStore accessTokenStore = new AccessTokenStore(() -> entityManager);
        List<AccessToken> accessTokens = accessTokenStore.find(user);
        accessTokens.forEach(accessTokenStore::delete);

        entityManager.remove(user);
    }

    public void signupOAuth(String email, String firstName, String lastName, String token) {
        signup(email, firstName, lastName, token, null);
    }

    public void signupEmailPassword(String email, String firstName, String lastName, String password) {
        // King e-mail addresses are not allowed. Kingsters should use OAuth with 2-factor auth for good security level.
        if (email.toLowerCase().contains("@king.com")) {
            LOGGER.warn("Failed to sign-up new user with e-mail {}. Kingsters are only allowed to use OAuth accounts.", email);
            throw new ServerException("E-mail not allowed.", Response.Status.CONFLICT);
        }

        // Verify that user not already registered.
        Optional<User> existingUser = findByEmail(email);

        if (existingUser.isPresent()) {
            LOGGER.warn("Failed to sign-up new user. User {} already registered.", email);
            throw new ServerException("User already registered.", Response.Status.CONFLICT);
        }

        // Create a new pending user.
        String activationToken = UUID.randomUUID().toString();
        signup(email, firstName, lastName, activationToken, password);

        // Send activation e-mail
        HashMap<String, String> params = new HashMap<>();
        params.put("activation_token", activationToken);
        params.put("first_name", firstName);
        params.put("last_name", lastName);
        emailService.send(email, configuration.getSignupEmailTemplate(), params);
    }

    private void signup(String email, String firstName, String lastName, String token, String password) {
        // Ensure lowercase e-mail
        email = email.toLowerCase();

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
        newUser.setPassword(password);
        entityManager.persist(newUser);
    }

    public void changePassword(Long userId, String currentPassword, String newPassword) {
        User user = find(userId).orElseThrow(() -> new ServerException("User not found.", Response.Status.NOT_FOUND));

        if (!user.authenticate(currentPassword)) {
            throw new ServerException("Invalid password.", Response.Status.UNAUTHORIZED);
        }

        user.setPassword(newPassword);

        LOGGER.info("User {} changed password.", user.getEmail());
    }

    public void forgotPassword(String email) {
        User user = findByEmail(email)
                .orElseThrow(() -> new ServerException("User not found.", Response.Status.NOT_FOUND));

        // Generate a new password reset token that can be used as a one-time code to change password
        String token = UUID.randomUUID().toString();
        PasswordResetToken passwordResetToken = new PasswordResetToken(user.getEmail(), PasswordHashGenerator.generateHash(token));
        entityManager.persist(passwordResetToken);

        // Send mail with URL to password change page (including token).
        HashMap<String, String> params = new HashMap<>();
        params.put("reset_token", token);
        params.put("email", user.getEmail());
        emailService.send(user.getEmail(), configuration.getForgotPasswordEmailTemplate(), params);

        LOGGER.info("Forgot password e-mail sent to {}", email);
    }

    public void resetPassword(String email, String token, String newPassword) {
        // Find password reset token.
        List<PasswordResetToken> results = entityManager
                .createNamedQuery("PasswordResetToken.findByEmail", PasswordResetToken.class)
                .setParameter("email", email)
                .getResultList();

        // Filter out matching token
        Optional<PasswordResetToken> passwordResetTokenOptional = results.stream()
                .filter(t -> (PasswordHashGenerator.validatePassword(token, t.getTokenHash())))
                .findAny();

        // Ensure that token was found.
        if (!passwordResetTokenOptional.isPresent()) {
            LOGGER.warn("Failed to reset password. Password reset token {} not found.", token);
            throw new ServerException("Invalid password reset token.", Response.Status.BAD_REQUEST);
        }

        // Reset user's password.
        User user = findByEmail(email).orElseThrow(() -> new ServerException("Failed to find user matching password reset token."));
        user.setPassword(newPassword);

        // Delete password reset token.
        entityManager.remove(passwordResetTokenOptional.get());

        LOGGER.info("Password reset for user {}", user.getEmail());
    }
}
