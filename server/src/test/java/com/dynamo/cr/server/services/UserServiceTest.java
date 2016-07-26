package com.dynamo.cr.server.services;

import com.dynamo.cr.server.auth.AccessTokenStore;
import com.dynamo.cr.server.model.*;
import com.dynamo.cr.server.test.EntityManagerRule;
import com.dynamo.cr.server.test.TestUser;
import com.dynamo.cr.server.test.TestUtils;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import javax.persistence.EntityManager;
import javax.persistence.RollbackException;
import java.util.Date;

import static org.junit.Assert.assertTrue;

public class UserServiceTest {
    @Rule
    public EntityManagerRule entityManagerRule = new EntityManagerRule();

    private UserService userService;

    @Before
    public void setUp() throws Exception {
        userService = new UserService(entityManagerRule.getEntityManager());
        createData();
    }

    private void createData() {
        EntityManager em = entityManagerRule.getEntityManager();

        User userCarl = TestUtils.buildUser(TestUser.CARL);
        em.persist(userCarl);
        InvitationAccount a1 = newInvitationAccount(userCarl, 1);
        em.persist(a1);

        User userJoe = TestUtils.buildUser(TestUser.JOE);
        em.persist(userJoe);
        InvitationAccount a2 = newInvitationAccount(userJoe, 1);
        em.persist(a2);

        User userLisa = TestUtils.buildUser(TestUser.LISA);
        em.persist(userLisa);
        AccessTokenStore accessTokenStore = new AccessTokenStore(() -> em);
        accessTokenStore.store(new AccessToken(userLisa, "hash", new Date(), new Date(), new Date(), ""));
        InvitationAccount a3 = newInvitationAccount(userLisa, 1);
        em.persist(a3);

        // Create new projects
        Project project1 = ModelUtil.newProject(em, userCarl, "Carl Contents Project", "Carl Contents Project Description");
        Project project2 = ModelUtil.newProject(em, userJoe, "Joe Coders' Project", "Joe Coders' Project Description");

        // Add users to project
        ModelUtil.addMember(project1, userJoe);
        ModelUtil.addMember(project1, userLisa);
        ModelUtil.addMember(project2, userCarl);
        ModelUtil.addMember(project2, userLisa);

        // Set up connections
        ModelUtil.connect(userCarl, userJoe);
        ModelUtil.connect(userCarl, userLisa);

        em.persist(userCarl);
        em.persist(userJoe);
        em.persist(userLisa);
    }

    @Test(expected=RollbackException.class)
    public void userThatIsProjectMemberCanNotBeRemoved() throws Exception {
        EntityManager em = entityManagerRule.getEntityManager();
        User u = ModelUtil.findUserByEmail(em, TestUser.LISA.email);
        em.remove(u);
        em.getTransaction().commit();
    }

    @Test(expected=RollbackException.class)
    public void userThatIsProjectOwnerCanNotBeRemoved() throws Exception {
        EntityManager em = entityManagerRule.getEntityManager();
        User u = ModelUtil.findUserByEmail(em, TestUser.CARL.email);
        userService.remove(u);
        em.getTransaction().commit();
    }

    @Test
    public void userShouldBeRemovedIfNotOwnerOfAnyProject() throws Exception {
        EntityManager em = entityManagerRule.getEntityManager();
        User u = ModelUtil.findUserByEmail(em, TestUser.LISA.email);
        userService.remove(u);
        assertTrue(false);
    }

    private InvitationAccount newInvitationAccount(User user, int originalCount) {
        InvitationAccount account = new InvitationAccount();
        account.setUser(user);
        account.setOriginalCount(originalCount);
        account.setCurrentCount(originalCount);
        return account;
    }
}
