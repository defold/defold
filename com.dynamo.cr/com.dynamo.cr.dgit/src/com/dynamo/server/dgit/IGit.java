package com.dynamo.server.dgit;

import java.io.IOException;

import com.dynamo.cr.protocol.proto.Protocol.CommitDesc;
import com.dynamo.cr.protocol.proto.Protocol.Log;

public interface IGit {

    /**
     * Clone Git repository (git clone repository directory)
     * @param repository Repository to clone
     * @param directory Directory to cloned
     * @throws IOException
     */
    void cloneRepo(String repository, String directory) throws IOException;

    /**
     * Clone Git repository bare (git clone repository directory). core.sharedRepository is set to group. Permissions is set to "g+ws"
     * @param repository Repository to clone
     * @param directory Directory to cloned
     * @param group Group to set for the repository, ie UNIX-group. Use null for default group.
     * @throws IOException
     */
    void cloneRepoBare(String repository, String directory, String group)
            throws IOException;

    /**
     * Pull and merge changes. (git pull)
     * @note Only valid if is GitState.CLEAN state
     * @param directory Git directory
     * @return True on success. False if merge conflicts
     * @throws IOException
     */
    boolean pull(String directory) throws IOException;

    /**
     * Get state. (git status)
     * @param directory Directory
     * @return GitState
     * @throws IOException
     */
    GitState getState(String directory) throws IOException;

    /**
     * Get status. (git status)
     * @note Slightly different behaviour from git status. When in merge mode
     * files resolved as "yours" are present in the files list even though we file
     * does not differ. This behaviour is required for graphical merge tools. Modified and deleted
     * files are supported.
     * @param directory Directory
     * @param fetch true to fetch from remote
     * @return GitStatus
     * @throws IOException
     */
    GitStatus getStatus(String directory, boolean fetch) throws IOException;

    /**
     * Commit all. (git commit -a -m message)
     * @param directory Directory
     * @param message Commit message
     * @throws IOException
     */
    CommitDesc commitAll(String directory, String message) throws IOException;

    /**
     * Commit. (git commit -m message)
     * @param directory Directory
     * @param message Commit message
     * @throws IOException
     */
    CommitDesc commit(String directory, String message) throws IOException;

    /**
     * Simple conflict resolve. (git checkout-index --stage=X -f FILE + git add FILE)
     * @note Must be in GitState.MERGE
     * @param directory Directory
     * @param file File to resolve
     * @param stage Resolve to stage
     * @throws IOException
     */
    void resolve(String directory, String file, GitStage stage)
            throws IOException;

    /**
     * Push. (git push)
     * @param directory Directory
     * @throws IOException
     */
    void push(String directory) throws IOException;

    /**
     * Push initial. (git push origin master)
     * @param directory Directory
     * @throws IOException
     */
    void pushInitial(String directory) throws IOException;

    /**
     * Add file (git add)
     * @param directory Directory
     * @param file File to add
     * @throws IOException
     */
    void add(String directory, String file) throws IOException;

    /**
     * Number of commits ahead origin/master
     * @param directory Directory
     * @param fetch true to fetch from remote
     * @return Number of commits ahead of origin/master
     * @throws IOException
     */
    int commitsAhead(String directory, boolean fetch) throws IOException;

    /**
     * Number of commits behind origin/master
     * @param directory Directory
     * @param fetch true to fetch from remote
     * @return Number of commits ahead of origin/master
     * @throws IOException
     */
    int commitsBehind(String directory, boolean fetch) throws IOException;

    /**
     * Remove file
     * @param directory Directory
     * @param file File to remove
     * @param force Force delete (-f)
     * @throws IOException
     */
    void rm(String directory, String file, boolean recursive, boolean force)
            throws IOException;

    /**
     * Move or rename resource
     * @param directory Directory
     * @param source Source resource
     * @param destination Destination resource
     * @param force Force delete (-f)
     * @throws IOException
     */
    void mv(String directory, String source, String destination, boolean force)
            throws IOException;

    /**
     * Checkout resource from repository
     * @param directory Directory of the repository
     * @param file Path of the resource to checkout, relative repository
     * @param force Continue even if the resource is unmerged
     * @throws IOException
     */
    void checkout(String directory, String file, boolean force)
            throws IOException;

    /**
     * Resets the index to the specified target version.
     * @param directory Directory of the repository
     * @param file Path of the resource to reset, relative repository. null means the whole tree will be reset.
     * @param target Target version to reset to
     * @throws IOException
     */
    void reset(String directory, GitResetMode resetMode, String file,
            String target) throws IOException;

    /**
     * Create and initialize a new git repository. core.sharedRepository is set to group. Permissions is set to "g+ws"
     * @param path path to where the repository should be created
     * @param group Group to set for the repository, ie UNIX-group. Use null for default group.
     */
    void initBare(String path, String group) throws IOException;

    /**
     * Removes a git repo from the file system. Use with caution since no branches or clones are removed.
     * @param path path of the repository
     */
    void rmRepo(String path) throws IOException;

    /**
     * Returns the content of the file at the specified revision, runs:
     * git show {revision}:{file}
     * @param directory Repository root
     * @param file File to be read
     * @param revision File revision
     * @return File content
     */
    byte[] show(String directory, String file, String revision)
            throws IOException;

    /**
     * Returns an array of performed commits on the branch at the specified directory with a specified max count. The array is ordered from newest to oldest.
     * git log --pretty=format:"%H%x00%cn%x00%ce%x00%s"
     * @param directory Repository root
     * @param maxCount Maximum number of commits
     * @return Log containing commit messages
     */
    Log log(String directory, int maxCount) throws IOException;

    /**
     * Configures the email and name for the user of the specified directory
     * git config --file=.git/config user.email {email}
     * git config --file=.git/config user.name {name}
     * @param directory Directory of the repository
     * @param email Email of the user
     * @param name Full name of the user
     * @throws IOException
     */
    void configUser(String directory, String email, String name)
            throws IOException;

    public abstract boolean checkGitVersion();

    void setUsername(String userName);

    void setPassword(String passWord);

    void setHost(String host);

    void config(String directory, String key, String value) throws IOException;

}