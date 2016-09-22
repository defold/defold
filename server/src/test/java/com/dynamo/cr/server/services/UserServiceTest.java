package com.dynamo.cr.server.services;

import com.dynamo.cr.server.auth.AccessTokenStore;
import com.dynamo.cr.server.model.AccessToken;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.test.EntityManagerRule;
import com.dynamo.cr.server.test.TestUser;
import com.dynamo.cr.server.test.TestUtils;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import javax.persistence.EntityManager;
import javax.persistence.RollbackException;
import java.util.Date;

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

        User userJoe = TestUtils.buildUser(TestUser.JOE);
        em.persist(userJoe);

        User userLisa = TestUtils.buildUser(TestUser.LISA);
        em.persist(userLisa);
        AccessTokenStore accessTokenStore = new AccessTokenStore(() -> em);
        accessTokenStore.store(new AccessToken(userLisa, "hash", new Date(), new Date(), new Date(), ""));

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
        User u = userService.findByEmail(TestUser.LISA.email).orElseThrow(RuntimeException::new);
        em.remove(u);
        em.getTransaction().commit();
    }

    @Test(expected=RollbackException.class)
    public void userThatIsProjectOwnerCanNotBeRemoved() throws Exception {
        EntityManager em = entityManagerRule.getEntityManager();
        User u = userService.findByEmail(TestUser.CARL.email).orElseThrow(RuntimeException::new);
        userService.remove(u);
        em.getTransaction().commit();
    }

    @Test
    public void userShouldBeRemovedIfNotOwnerOfAnyProject() throws Exception {
        User u = userService.findByEmail(TestUser.LISA.email).orElseThrow(RuntimeException::new);
        userService.remove(u);
    }
}
