package com.dynamo.cr.server;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.persistence.EntityManager;
import javax.persistence.EntityManagerFactory;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.osgi.PersistenceProvider;

import com.dynamo.cr.proto.Config;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc.Activity;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;
import com.dynamo.cr.protocol.proto.Protocol.LaunchInfo;
import com.dynamo.cr.protocol.proto.Protocol.ResolveStage;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo.Builder;
import com.dynamo.cr.server.auth.SecurityFilter;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.Project;
import com.dynamo.cr.server.model.User;
import com.dynamo.cr.server.model.User.Role;
import com.dynamo.cr.server.resources.EntityManagerFactoryProvider;
import com.dynamo.cr.server.resources.ServerProvider;
import com.dynamo.cr.server.util.FileUtil;
import com.dynamo.server.git.CommandUtil;
import com.dynamo.server.git.CommandUtil.IListener;
import com.dynamo.server.git.CommandUtil.Result;
import com.dynamo.server.git.Git;
import com.dynamo.server.git.GitStage;
import com.dynamo.server.git.GitState;
import com.dynamo.server.git.GitStatus;
import com.google.protobuf.TextFormat;
import com.sun.grizzly.http.SelectorThread;
import com.sun.grizzly.http.embed.GrizzlyWebServer;
import com.sun.jersey.api.NotFoundException;
import com.sun.jersey.api.container.filter.RolesAllowedResourceFilterFactory;
import com.sun.jersey.api.container.grizzly.GrizzlyWebContainerFactory;
import com.sun.jersey.api.core.ResourceConfig;

public class Server {

    private SelectorThread threadSelector;
    private String baseUri;
    private String branchRoot;
    private Pattern[] filterPatterns;
    private Configuration configuration;
    private GrizzlyWebServer dataServer;
    private EntityManagerFactory emf;
    public Server(String configuration_file) throws IOException {
        loadConfig(configuration_file);

        assert ServerProvider.server == null;
        ServerProvider.server = this;

        HashMap<String, Object> props = new HashMap<String, Object>();
        props.put(PersistenceUnitProperties.CLASSLOADER, this.getClass().getClassLoader());
        // NOTE: JPA-PersistenceUnits: in plug-in MANIFEST.MF has to be set. Otherwise the persistence unit is not found.
        emf = new PersistenceProvider().createEntityManagerFactory(configuration.getPersistenceUnitName(), props);
        if (emf == null) {
            Activator.getLogger().log(Level.SEVERE, String.format("Persistant unit '%s' not found", configuration.getPersistenceUnitName()));
        }
        else {
            bootStrapUsers();
            bootStrapProjects();
        }

        EntityManagerFactoryProvider.emf = emf;

        baseUri = String.format("http://localhost:%d/", this.configuration.getServicePort());
        final Map<String, String> initParams = new HashMap<String, String>();

        /*
         * NOTE:
         * Class scanning in jersey with "com.sun.jersey.config.property.packages"
         * is not compatible with OSGi.
         * Therefore are all resource classes specified in class ResourceConfig
         * using  "com.sun.jersey.config.property.resourceConfigClass" instead
         *
         * Old "code":
         * initParams.put("com.sun.jersey.config.property.packages", "com.dynamo.cr.server.resources;com.dynamo.cr.server.auth;com.dynamo.cr.common.providers");
         */
        initParams.put("com.sun.jersey.config.property.resourceConfigClass",
                       "com.dynamo.cr.server.ResourceConfig");

        initParams.put(ResourceConfig.PROPERTY_CONTAINER_REQUEST_FILTERS,
                SecurityFilter.class.getName());
        initParams.put(ResourceConfig.PROPERTY_RESOURCE_FILTER_FACTORIES,
                RolesAllowedResourceFilterFactory.class.getName());

        threadSelector = GrizzlyWebContainerFactory.create(baseUri, initParams);

        if (!Git.checkGitVersion()) {
            // TODO: Hmm, exception...
            throw new RuntimeException("Invalid version of git or not found");
        }

        dataServer = new GrizzlyWebServer(configuration.getBuildDataPort(), configuration.getBranchRoot());
        dataServer.start();
    }

    private void bootStrapUsers() {
        // TODO: TEMPORARY SOLUTION!!!
        EntityManager em = emf.createEntityManager();
        em.getTransaction().begin();
        String[][] users = new String[][] {
                { "cgmurray@gmail.com", "chmu", "Christian", "Murray"},
                { "ragnar.svensson@gmail.com", "rasv", "Ragnar", "Svensson"},
                { "egeberg.fredrik@gmail.com", "freg", "Fredrik", "Egeberg"},
                { "gustafberg80@gmail.com", "gube", "Gustaf", "Berg"},
                { "dynamogameengine@gmail.com", "admin", "Mr", "Admin" }
                };

        List<User> createdUsers = new ArrayList<User>();
        for (String[] entry : users) {
            if (ModelUtil.findUserByEmail(em, entry[0]) == null) {
                User u = new User();
                u.setEmail(entry[0]);
                u.setFirstName(entry[2]);
                u.setLastName(entry[3]);
                if (entry[1].equals("admin")) {
                    u.setPassword("mobEpeyfUj9");
                    u.setRole(Role.ADMIN);
                } else {
                    u.setPassword(entry[1]);
                }
                em.persist(u);
                createdUsers.add(u);
            }
        }
        em.getTransaction().commit();

        em.getTransaction().begin();
        for (User user : createdUsers) {
            for (User connectTo : createdUsers) {
                if (user != connectTo) {
                    ModelUtil.connect(user, connectTo);
                    em.persist(user);
                    em.persist(connectTo);
                }
            }
        }
        em.getTransaction().commit();
        em.close();
    }

    private void bootStrapProjects() {
        // TODO: TEMPORARY SOLUTION!!!
        EntityManager em = emf.createEntityManager();
        Project p = em.find(Project.class, 1L);
        if (p != null)
            return;

        em.getTransaction().begin();
        User u = ModelUtil.findUserByEmail(em, "cgmurray@gmail.com");
        p = ModelUtil.newProject(em, u, "demos", "Cool demos");
        em.getTransaction().commit();
        em.close();
    }

    void loadConfig(String file_name) throws IOException {

        FileReader fr = new FileReader(file_name);
        Config.Configuration.Builder conf_builder = Config.Configuration.newBuilder();
        TextFormat.merge(fr, conf_builder);

        configuration = conf_builder.build();

        branchRoot = configuration.getBranchRoot();

        filterPatterns = new Pattern[configuration.getFiltersCount()];
        int i = 0;
        for (String f : configuration.getFiltersList()) {
            filterPatterns[i++] = Pattern.compile(f);
        }

    }

    public void stop() {
        assert ServerProvider.server != null;
        ServerProvider.server = null;

        threadSelector.stopEndpoint();
        dataServer.stop();
        emf.close();
    }

    public Project getProject(EntityManager em, String id) throws ServerException {
        Project project = em.find(Project.class, Long.parseLong(id));
        if (project == null)
            throw new ServerException(String.format("No such project %s", project));

        return project;
    }

    public User getUser(EntityManager em, String userId) throws ServerException {
        User user = em.find(User.class, Long.parseLong(userId));
        if (user == null)
            throw new ServerException(String.format("No such user %d", userId));

        return user;
    }

    public static void main(String[] args) throws IOException {
        Server s = new Server("config/jetbot.config");
        System.in.read();
        s.stop();
        System.exit(0);
    }

    public String getBaseURI() {
        return baseUri;
    }

    void ensureProjectBranch(EntityManager em, String project, String user, String branch) throws ServerException {
        // We check that the project really exists here
        getProject(em, project);

        User u = getUser(em, user);
        String p = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        if (!new File(p).exists()) {
            throw new ServerException(String.format("No such branch %s/%s", user, branch));
        }
    }

    public void createBranch(EntityManager em, String project, String user, String branch) throws ServerException, IOException {
        // We check that the project really exists here
        getProject(em, project);
        User u = getUser(em, user);

        String p = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        File f = new File(p);
        if (f.exists()) {
            throw new ServerException(String.format("Branch %s already exists", p));
        }

        f.getParentFile().mkdirs();

        Git git = new Git();
        String sourcePath = String.format("%s/%s", configuration.getRepositoryRoot(), project);
        git.cloneRepo(sourcePath, p);

        if (configuration.hasBuiltinsDirectory()) {
            String builtins = configuration.getBuiltinsDirectory();
            String dest = String.format("%s/builtins", p);
            // Create symbolic link to builtins. See deleteBranch()
            Result r = CommandUtil.execCommand(null, null, new String[] {"ln", "-s", builtins, dest});
            if (r.exitValue != 0) {
                Activator.getLogger().log(Level.SEVERE, r.stdErr.toString());
                FileUtils.deleteDirectory(new File(p));
                throw new ServerException(String.format("Unable to create branch. Internal server error"));
            }
        }
    }

    public String getBranchRoot() {
        return branchRoot;
    }

    public boolean containsBranch(EntityManager em, String project, String user, String branch) throws ServerException {
        User u = getUser(em, user);
        String p = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        return new File(p).exists();
    }

    public void deleteBranch(EntityManager em, String project, String user, String branch) throws ServerException  {
        ensureProjectBranch(em, project, user, branch);
        User u = getUser(em, user);

        String p = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        try {
            String dest = String.format("%s/builtins", p);
            // Remove symbol link to builtins. See createBranch()
            // We need to remove the symbolic link *before* removeDir is invoked. FileUtil.removeDir follow links...
            CommandUtil.execCommand(null, null, new String[] {"rm", dest});
            FileUtil.removeDir(new File(p));
        } catch (IOException e) {
            throw new ServerException("", e);
        }
    }

    public Protocol.ResourceInfo getResourceInfo(EntityManager em, String project, String user, String branch,
            String path) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        User u = getUser(em, user);
        String p = String.format("%s/%s/%d/%s%s", branchRoot, project, u.getId(), branch, path);
        File f = new File(p);

        if (!f.exists())
            throw new NotFoundException(String.format("%s not found", p));

        if (f.isFile()) {
            Builder builder = Protocol.ResourceInfo.newBuilder()
            .setName(f.getName())
            .setPath(String.format("%s", path))
            .setSize((int) f.length())
            .setLastModified(f.lastModified())
            .setType(Protocol.ResourceType.FILE);

            return builder.build();
        }
        else if (f.isDirectory()) {
            Builder builder = Protocol.ResourceInfo.newBuilder()
            .setName(f.getName())
            .setPath(String.format("%s", path))
            .setSize(0)
            .setLastModified(f.lastModified())
            .setType(Protocol.ResourceType.DIRECTORY);

            for (File subf : f.listFiles()) {
                boolean match = false;
                String pathToMatch = path;
                if (pathToMatch.equals("/"))
                    pathToMatch += subf.getName();
                else
                    pathToMatch += "/" + subf.getName();

                for (Pattern pf : filterPatterns) {
                    if (pf.matcher(pathToMatch).matches()) {
                        match = true;
                        break;
                    }
                }
                if (!match)
                    builder.addSubResourceNames(subf.getName());
            }
            return builder.build();
        }
        else {
            throw new ServerException("Unknown resource type: " + f);
        }
    }

    public byte[] getResourceData(EntityManager em, String project, String user, String branch,
            String path, String revision) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        User u = getUser(em, user);
        String p = String.format("%s/%s/%d/%s%s", branchRoot, project, u.getId(), branch, path);
        if (revision != null && !revision.equals("")) {
            Git git = new Git();
            try {
                return git.show(String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch), path.substring(1), revision);
            } catch (IOException e) {
                throw new NotFoundException(String.format("%s (rev '%s') not found", p, revision));
            }
        } else {
            File f = new File(p);

            if (!f.exists())
                throw new NotFoundException(String.format("%s not found", p));
            else if (!f.isFile())
                throw new ServerException("getResourceData opertion is not applicable on non-files");

            byte[] file_data = new byte[(int) f.length()];
            BufferedInputStream is = new BufferedInputStream(new FileInputStream(p));
            is.read(file_data);
            is.close();
            return file_data;
        }
    }

    public void deleteResource(EntityManager em, String project, String user, String branch,
            String path) throws ServerException, IOException {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");
        User u = getUser(em, user);

        String p = String.format("%s/%s/%d/%s%s", branchRoot, project, u.getId(), branch, path);
        File f = new File(p);
        // NOTE: We need to cache isFile() here.
        boolean isFile = f.isFile();

        if (!f.exists())
            throw new NotFoundException(String.format("%s not found", p));

        boolean recursive = false;
        if (f.isDirectory())
            recursive = true;

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        Git git = new Git();
        boolean force = true;
        git.rm(branch_path, path.substring(1), recursive, force); // NOTE: Remove / from path

        // The code is somewhat strange
        // We always recreate to parent directory for a file
        // in order to avoid implicit deletion of directories. This behavior confuses
        // eclipse. We also need to remove the directory manually as git won't remove
        // empty directories. (git doesn't track empty directories). This is direct
        // consequence of the first behavior (recreating directories)
        if (isFile) {
            String localPath = FilenameUtils.getFullPath(p);
            File localPathFile = new File(localPath);
            localPathFile.mkdirs();
        } else {
            f.delete();
        }
    }

    public void renameResource(EntityManager em, String project, String user, String branch,
            String source, String destination) throws ServerException, IOException {
        ensureProjectBranch(em, project, user, branch);
        if (source == null)
            throw new ServerException("null source");
        User u = getUser(em, user);

        String source_path = String.format("%s/%s/%d/%s%s", branchRoot, project, u.getId(), branch, source);
        File f = new File(source_path);

        if (!f.exists())
            throw new NotFoundException(String.format("%s not found", source_path));

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        Git git = new Git();
        git.mv(branch_path, source.substring(1), destination.substring(1), true); // NOTE: Remove / from path
    }

    public void revertResource(EntityManager em, String project, String user, String branch,
            String path) throws IOException, ServerException {
        User u = getUser(em, user);
        String branch_path = String.format("%s/%s/%d/%s", branchRoot, project, u.getId(), branch);
        Git git = new Git();
        GitStatus status = git.getStatus(branch_path);
        String localPath = path.substring(1);
        GitStatus.Entry entry = null;
        for (GitStatus.Entry f : status.files) {
            if (f.file.equals(path.substring(1))) {
                entry = f;
                break;
            }
        }
        if (entry != null) {
            switch (entry.indexStatus) {
            case 'A':
                git.rm(branch_path, localPath, false, true);
                break;
            case 'R':
                git.rm(branch_path, localPath, false, true);
                git.reset(branch_path, entry.original);
                git.checkout(branch_path, entry.original, false);
                break;
            case 'D':
            case 'M':
                git.reset(branch_path, localPath);
            default:
                git.checkout(branch_path, localPath, false);
                break;
            }
        }
    }

    public void putResourceData(EntityManager em, String project, String user, String branch,
            String path, byte[] data) throws ServerException, IOException  {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
        File f = new File(p);
        File parent = f.getParentFile();

        if (!parent.exists())
            throw new NotFoundException(String.format("Directory %s not found", parent.getPath()));

        FileOutputStream os = new FileOutputStream(f);
        os.write(data);
        os.close();

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        Git git = new Git();
        try {
            git.add(branch_path, path.substring(1));  // NOTE: Remove / from path
        } catch (Throwable e) {
            // Rollback if git error
            f.delete();
            throw new ServerException("Failed to add file", e);
        }
    }

    public void mkdir(String project, String user, String branch, String path) throws ServerException {
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
        File f = new File(p);
        if (!f.exists()) {
            f.mkdirs();
        }
        else {
            if (!f.isDirectory())
                throw new ServerException(String.format("File %s already exists and is not a directory", p));
        }
    }

    public BranchStatus getBranchStatus(EntityManager em, String project, String user, String branch) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        GitState state = git.getState(p);
        GitStatus status = git.getStatus(p);

        Protocol.BranchStatus.State s;
        if (state == GitState.CLEAN)
            s = Protocol.BranchStatus.State.CLEAN;
        else if (state == GitState.DIRTY)
            s = Protocol.BranchStatus.State.DIRTY;
        else if (state == GitState.MERGE)
            s = Protocol.BranchStatus.State.MERGE;
        else
            throw new RuntimeException("Internal error. Unknown state: " + state);

        BranchStatus.Builder builder = Protocol.BranchStatus
        .newBuilder().setName(branch)
        .setBranchState(s)
        .setCommitsAhead(status.commitsAhead)
        .setCommitsBehind(status.commitsBehind);

        for (GitStatus.Entry e : status.files) {
            String fn = String.format("/%s", e.file);
            BranchStatus.Status.Builder status_builder = BranchStatus.Status.newBuilder().setName(fn).setIndexStatus("" + e.indexStatus).setWorkingTreeStatus("" + e.workingTreeStatus);
            if (e.original != null) {
                status_builder.setOriginal(String.format("/%s", e.original));
            }
            builder.addFileStatus(status_builder);
        }

        return builder.build();
    }

    public String[] getBranchNames(EntityManager em, String project, String user) {
        String p = String.format("%s/%s/%s", branchRoot, project, user);
        File d = new File(p);
        if (!d.isDirectory())
            return new String[] {};

        List<String> list = new ArrayList<String>();
        for (File f : d.listFiles()) {
            if (f.isDirectory()) {
                if (new File(f, ".git").isDirectory()) {
                    list.add(f.getName());
                }
            }
        }

        String[] ret = new String[list.size()];
        list.toArray(ret);

        return ret;
    }

    public void updateBranch(EntityManager em, String project, String user, String branch) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        git.pull(p);
    }

    public void commitBranch(EntityManager em, String project, String user, String branch, String message) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        git.commitAll(p, message);
    }

    public void resolveResource(EntityManager em, String project, String user, String branch, String path,
            ResolveStage stage) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        GitStage git_stage;
        if (stage == ResolveStage.BASE) {
            git_stage = GitStage.BASE;
        }
        else if (stage == ResolveStage.YOURS) {
            git_stage = GitStage.YOURS;
        }
        else if (stage == ResolveStage.THEIRS) {
            git_stage = GitStage.THEIRS;
        }
        else {
            throw new RuntimeException("Internal error. Unknown stage: " + stage);
        }

        git.resolve(p, path.substring(1), git_stage);  // NOTE: Remove / from path
    }

    public void commitMergeBranch(EntityManager em, String project, String user, String branch,
            String message) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        git.commit(p, message);
    }

    public void publishBranch(EntityManager em, String project, String user, String branch) throws IOException, ServerException {
        ensureProjectBranch(em, project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        git.push(p);
    }


    static class RuntimeBuildDesc {

        public RuntimeBuildDesc() {
            id = -1;
            progress = -1;
            workAmount = -1;
        }
        public int id;
        public Activity activity;
        public BuildDesc.Result buildResult;
        public int progress;
        public int workAmount;
        public String stdOut = "";
        public String stdErr = "";

    }

    int nextBuildNumber = 0;
    // TODO: We must remove old builds somehow...!
    Map<Integer, RuntimeBuildDesc> builds = new HashMap<Integer, RuntimeBuildDesc>();

    static class BuildRunnable implements Runnable, IListener {

        private int id;
        private Server server;
        private String branchRoot;
        private String project;
        private String user;
        private String branch;
        private Pattern workPattern;
		private boolean rebuild;
        private Configuration configuration;

        public BuildRunnable(Server server, int id, String branch_root, String project, String user, String branch, boolean rebuild) {
            this.id = id;
            this.server = server;
            this.branchRoot = branch_root;
            this.project = project;
            this.user = user;
            this.branch = branch;
            this.rebuild = rebuild;
            this.configuration = server.configuration;

            workPattern = Pattern.compile(server.configuration.getProgressRe());
        }

        @Override
        public void run() {
            String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
            try {
                Result r;
                String[] args = new String[configuration.getConfigureCommand().getArgsCount()];
                configuration.getConfigureCommand().getArgsList().toArray(args);
                r = CommandUtil.execCommand(p, null, args);
                if (r.exitValue != 0) {
                    this.server.updateBuild(id, Activity.IDLE, BuildDesc.Result.ERROR, r.stdOut.toString(), r.stdErr.toString());
                    return;
                }

                if (rebuild) {
                    args = new String[configuration.getRebuildCommand().getArgsCount()];
                    configuration.getRebuildCommand().getArgsList().toArray(args);
                } else {
                    args = new String[configuration.getBuildCommand().getArgsCount()];
                    configuration.getBuildCommand().getArgsList().toArray(args);
                }
                r = CommandUtil.execCommand(p, this, args);
                if (r.exitValue != 0) {
                    this.server.updateBuild(id, Activity.IDLE, BuildDesc.Result.ERROR, r.stdOut.toString(), r.stdErr.toString());
                    return;
                }

                this.server.updateBuild(id, Activity.IDLE, BuildDesc.Result.OK, r.stdOut.toString(), r.stdErr.toString());
            } catch (IOException e) {
                Activator.getLogger().log(Level.WARNING, e.getMessage(), e);
            }
        }

        @Override
        public void onStdErr(StringBuffer buffer) {
            Matcher m = workPattern.matcher(buffer.toString());

            int progress = -1;
            int workAmount = -1;
            while (m.find()) {
                progress = Integer.valueOf(m.group(1));
                workAmount = Integer.valueOf(m.group(2));
            }

            if (progress != -1) {
                server.updateBuildProgress(id, progress, workAmount);
            }
        }

        @Override
        public void onStdOut(StringBuffer buffer) {
            // TODO Auto-generated method stub

        }
    }

    public BuildDesc build(String project, String user, String branch, boolean rebuild) {
        int build_number;
        synchronized(this) {
            build_number = nextBuildNumber++;
        }

        BuildDesc bd = BuildDesc.newBuilder()
            .setId(build_number)
            .setBuildActivity(Activity.BUILDING)
            .setBuildResult(BuildDesc.Result.OK)
            .setProgress(-1)
            .setWorkAmount(-1)
            .build();

        RuntimeBuildDesc rtbd = new RuntimeBuildDesc();
        rtbd.id = build_number;
        rtbd.activity = Activity.BUILDING;
        rtbd.buildResult = BuildDesc.Result.OK;
        builds.put(build_number, rtbd);

        Thread thread = new Thread(new BuildRunnable(this, build_number, branchRoot, project, user, branch, rebuild));
        thread.start();

        return bd;
    }

    public synchronized void updateBuild(int id, Activity activity, BuildDesc.Result result, String std_out, String std_err) {
        RuntimeBuildDesc rtbd = builds.get(id);
        rtbd.activity = activity;
        rtbd.buildResult = result;
        rtbd.stdOut = std_out;
        rtbd.stdErr = std_err;
    }

    private synchronized void updateBuildProgress(int id, int progress, int workAmount) {
        RuntimeBuildDesc rtbd = builds.get(id);
        rtbd.progress = progress;
        rtbd.workAmount = workAmount;
    }

    public BuildDesc buildStatus(String project, String user, String branch, int id) {
        RuntimeBuildDesc rtbd = builds.get(id);

        BuildDesc bd = BuildDesc.newBuilder()
            .setId(rtbd.id)
            .setBuildActivity(rtbd.activity)
            .setBuildResult(rtbd.buildResult)
            .setProgress(rtbd.progress)
            .setWorkAmount(rtbd.workAmount)
            .build();

        return bd;
    }

    public BuildLog buildLog(String project, String user, String branch, int id) {
        RuntimeBuildDesc rtbd = builds.get(id);
        return BuildLog.newBuilder()
            .setStdOut(rtbd.stdOut)
            .setStdErr(rtbd.stdErr).build();
    }


    public LaunchInfo getLaunchInfo(EntityManager em, String project) throws ServerException {
        // We check that the project really exists here
        getProject(em, project);

        LaunchInfo.Builder b = LaunchInfo.newBuilder();
        for (String s : configuration.getExeCommand().getArgsList()) {
            b.addArgs(s);
        }

        return b.build();
    }

    public EntityManagerFactory getEntityManagerFactory() {
        return emf;
    }

    public Configuration getConfiguration() {
        return configuration;
    }

}
