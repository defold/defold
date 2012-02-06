package com.dynamo.cr.client;

import java.util.regex.Pattern;

import com.dynamo.cr.branchrepo.BranchRepository;
import com.dynamo.cr.protocol.proto.Protocol.UserInfo;
import com.dynamo.server.dgit.GitFactory;

public class LocalBranchRepository extends BranchRepository {

    private UserInfo userInfo;

    public LocalBranchRepository(String branchRoot, String repositoryRoot,
            String builtinsDirectory, Pattern[] filterPatterns, UserInfo userInfo, String password) {
        super(GitFactory.Type.JGIT, branchRoot, repositoryRoot, builtinsDirectory, filterPatterns, userInfo.getEmail(), password);
        this.userInfo = userInfo;
    }

    @Override
    protected void ensureProject(String project) {
        // No-op locally
    }

    @Override
    protected String getFullName(String userId) {
        // NOTE: userId is ignored for local branch repo
        StringBuilder sb = new StringBuilder(userInfo.getFirstName());
        if (userInfo.getLastName() != null) {
            sb.append(" ");
            sb.append(userInfo.getLastName());
        }
        return sb.toString();
    }

    @Override
    protected String getEmail(String userId) {
        // NOTE: userId is ignored for local branch repo
        return userInfo.getEmail();
    }

}
