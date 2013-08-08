package com.dynamo.cr.server.resources;

import java.io.File;
import java.util.Arrays;
import java.util.List;

import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.Response;
import javax.ws.rs.core.Response.Status;

import org.apache.commons.io.FilenameUtils;
import org.eclipse.jgit.util.StringUtils;

import com.dynamo.cr.proto.Config.BillingProduct;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.protocol.proto.Protocol.CreditCardInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProductInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectInfo;
import com.dynamo.cr.protocol.proto.Protocol.ProjectStatus;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionInfo;
import com.dynamo.cr.protocol.proto.Protocol.UserSubscriptionState;
import com.dynamo.cr.server.Server;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.UserSubscription;
import com.dynamo.cr.server.model.UserSubscription.CreditCard;
import com.dynamo.cr.server.util.ChargifyUtil;

public class ResourceUtil {

    public static void throwWebApplicationException(Status status, String msg) {
        Response response = Response
                .status(status)
                .type(MediaType.TEXT_PLAIN)
                .entity(msg)
                .build();
        throw new WebApplicationException(response);
    }

    public static UserInfo createUserInfo(User user) {
        UserInfo userInfo = UserInfo.newBuilder()
            .setId(user.getId())
            .setEmail(user.getEmail())
            .setFirstName(user.getFirstName())
            .setLastName(user.getLastName())
            .build();
        return userInfo;
    }

    /*
     * TODO: We should perhaps move all configuration that requires logic
     * to a setup-phase in Server? Same for getGitBaseUri below
     * Currently Configuration is a immutable configuration data-structure (protobuf)
     * We should perhaps derive settings to a new class with git-base-path etc
     */
    public static String getGitBasePath(Configuration configuration) {
        String repositoryRoot = FilenameUtils.normalize(configuration.getRepositoryRoot(), true);
        List<String> repositoryRootList = Arrays.asList(repositoryRoot.split("/"));
        if (repositoryRootList.size() < 1) {
            throw new RuntimeException("repositoryRoot path must contain at least 1 element");
        } else if (repositoryRootList.size() == 1) {
            repositoryRootList.add(0, ".");
        }

        String basePath = StringUtils.join(repositoryRootList.subList(0, repositoryRootList.size()-1), "/");
        basePath = new File(basePath).getAbsolutePath();
        return basePath;
    }

    public static String getGitBaseUri(Configuration configuration) {
        String repositoryRoot = FilenameUtils.normalize(configuration.getRepositoryRoot(), true);
        List<String> repositoryRootList = Arrays.asList(repositoryRoot.split("/"));
        if (repositoryRootList.size() < 1) {
            throw new RuntimeException("repositoryRoot path must contain at least 1 element");
        } else if (repositoryRootList.size() == 1) {
            repositoryRootList.add(0, ".");
        }

        String baseUri = "/" + repositoryRootList.get(repositoryRootList.size()-1);
        return baseUri;
    }

    public static ProjectInfo createProjectInfo(Configuration configuration, User user, Project project,
            boolean isQualified) {
        ProjectInfo.Builder b = ProjectInfo.newBuilder()
            .setId(project.getId())
            .setName(project.getName())
            .setDescription(project.getDescription())
            .setOwner(createUserInfo(project.getOwner()))
            .setCreated(project.getCreated().getTime())
            .setLastUpdated(project.getLastUpdated().getTime())
            .setRepositoryUrl(String.format("http://%s:%d%s/%d", configuration.getHostname(),
                    configuration.getGitPort(),
                    getGitBaseUri(configuration),
                    project.getId()));
        if (isQualified) {
            b.setStatus(ProjectStatus.PROJECT_STATUS_OK);
        } else {
            b.setStatus(ProjectStatus.PROJECT_STATUS_UNQUALIFIED);
        }

        if (Server.getEngineFile(configuration, Long.toString(project.getId()), "ios").exists()) {
            String key = Server.getEngineDownloadKey(project);
            String url = String.format("http://%s:%d/projects/%d/%d/engine_manifest/ios/%s",
                    configuration.getHostname(),
                    configuration.getServicePort(),
                    user.getId(),
                    project.getId(),
                    key);

            b.setIOSExecutableUrl(url);
        }

        for (User u : project.getMembers()) {
            b.addMembers(createUserInfo(u));
        }

        return b.build();
    }

    public static ProductInfo createProductInfo(BillingProduct product, String billingApiUrl) {
        ProductInfo.Builder b = ProductInfo.newBuilder().setId(product.getId()).setName(product.getName())
                .setMaxMemberCount(product.getMaxMemberCount()).setFee(product.getFee());
        b.setSignupUrl(ChargifyUtil.generateSignupUrl((long) product.getId(), billingApiUrl));
        return b.build();
    }

    public static CreditCardInfo createCreditCardInfo(CreditCard creditCard) {
        CreditCardInfo.Builder b = CreditCardInfo.newBuilder();
        b.setMaskedNumber(creditCard.getMaskedNumber()).setExpirationMonth(creditCard.getExpirationMonth())
                .setExpirationYear(creditCard.getExpirationYear());
        return b.build();
    }

    public static UserSubscriptionInfo createUserSubscriptionInfo(UserSubscription subscription, Configuration configuration) {
        UserSubscriptionInfo.Builder b = UserSubscriptionInfo.newBuilder();
        switch (subscription.getState()) {
        case CANCELED:
            b.setState(UserSubscriptionState.CANCELED);
            break;
        case PENDING:
            b.setState(UserSubscriptionState.PENDING);
            break;
        case ACTIVE:
            b.setState(UserSubscriptionState.ACTIVE);
            break;
        }
        for (BillingProduct product : configuration.getProductsList()) {
            if (product.getId() == subscription.getProductId()) {
                b.setProduct(createProductInfo(product, configuration.getBillingApiUrl()));
                break;
            }
        }
        if (subscription.getCreditCard() != null) {
            b.setCreditCard(createCreditCardInfo(subscription.getCreditCard()));
        }
        b.setUpdateUrl(ChargifyUtil.generateUpdateUrl(subscription.getExternalId(), configuration.getBillingApiUrl(),
                configuration.getBillingSharedKey()));
        if (subscription.getCancellationMessage() != null) {
            b.setCancellationMessage(subscription.getCancellationMessage());
        }
        return b.build();
    }
}
