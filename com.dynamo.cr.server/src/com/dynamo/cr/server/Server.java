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

import org.eclipse.persistence.config.PersistenceUnitProperties;
import org.eclipse.persistence.jpa.osgi.PersistenceProvider;

import com.dynamo.cr.proto.Config;
import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.proto.Config.ProjectConfiguration;
import com.dynamo.cr.protocol.proto.Protocol;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc;
import com.dynamo.cr.protocol.proto.Protocol.BuildDesc.Activity;
import com.dynamo.cr.protocol.proto.Protocol.BuildLog;
import com.dynamo.cr.protocol.proto.Protocol.ResolveStage;
import com.dynamo.cr.protocol.proto.Protocol.ResourceInfo.Builder;
import com.dynamo.cr.server.auth.SecurityFilter;
import com.dynamo.cr.server.model.ModelUtil;
import com.dynamo.cr.server.model.User;
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
    private Project[] projects;
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
                { "cgmurray@gmail.com", "chmu"},
                { "ragnar.svensson@gmail.com", "rasv"},
                { "egeberg.fredrik@gmail.com", "freg"},
                { "gustafberg80@gmail.com", "gube" } };
        for (String[] email_pass : users) {
            if (ModelUtil.findUserByEmail(em, email_pass[0]) == null) {
                User u = new User();
                u.setEmail(email_pass[0]);
                u.setFirstName("undefined");
                u.setLastName("undefined");
                u.setPassword(email_pass[1]);
                Activator.getLogger().log(Level.INFO, "Creating user " + u);
                em.persist(u);
            }
        }
        em.getTransaction().commit();
    }

    void loadConfig(String file_name) throws IOException {

        FileReader fr = new FileReader(file_name);
        Config.Configuration.Builder conf_builder = Config.Configuration.newBuilder();
        TextFormat.merge(fr, conf_builder);

        configuration = conf_builder.build();

        projects = new Project[configuration.getProjectsCount()];
        int i = 0;
        for (ProjectConfiguration pc : configuration.getProjectsList()) {
            projects[i++] = new Project(pc);
        }

        branchRoot = configuration.getBranchRoot();

        filterPatterns = new Pattern[configuration.getFiltersCount()];
        i = 0;
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

    public Project getProject(String id) {
        for (Project p : projects) {
            if (id.equals(p.getId()))
                return p;
        }
        return null;
    }

    public Project[] getProjects() {
        return projects;
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

    void ensureProjectBranch(String project, String user, String branch) throws ServerException {
        Project proj = getProject(project);
        if (proj == null)
            throw new ServerException(String.format("No such project %s", project));

        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        if (!new File(p).exists()) {
            throw new ServerException(String.format("No such branch %s/%s", user, branch));
        }
    }

    public void createBranch(String project, String user, String branch) throws ServerException, IOException {

        Project proj = getProject(project);
        if (proj == null)
            throw new ServerException(String.format("No such project %s", project));

        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        File f = new File(p);
        if (f.exists()) {
            throw new ServerException(String.format("Branch %s already exists", p));
        }

        f.getParentFile().mkdirs();

        Git git = new Git();
        git.cloneRepo(proj.getRoot(), p);
    }

    public String getBranchRoot() {
        return branchRoot;
    }

    public boolean containsBranch(String project, String user, String branch) {
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        return new File(p).exists();
    }

    public void deleteBranch(String project, String user, String branch) throws ServerException  {
        ensureProjectBranch(project, user, branch);

        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        try {
            FileUtil.removeDir(new File(p));
        } catch (IOException e) {
            throw new ServerException("", e);
        }
    }

    public Protocol.ResourceInfo getResourceInfo(String project, String user, String branch,
            String path) throws IOException, ServerException {
        ensureProjectBranch(project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
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
                for (Pattern pf : filterPatterns) {
                    if (pf.matcher(subf.getPath()).matches()) {
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

    public byte[] getResourceData(String project, String user, String branch,
            String path) throws IOException, ServerException {
        ensureProjectBranch(project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
        File f = new File(p);

        if (!f.exists())
            throw new NotFoundException(String.format("%s not found", p));
        else if (!f.isFile()) {
            throw new ServerException("getResourceData opertion is not applicable on non-files");
        }

        byte[] file_data = new byte[(int) f.length()];
        BufferedInputStream is = new BufferedInputStream(new FileInputStream(p));
        is.read(file_data);
        is.close();
        return file_data;
    }

    public void deleteResource(String project, String user, String branch,
            String path) throws ServerException, IOException {
        ensureProjectBranch(project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
        File f = new File(p);

        if (!f.exists())
            throw new NotFoundException(String.format("%s not found", p));

        boolean recursive = false;
        if (f.isDirectory())
            recursive = true;

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        Git git = new Git();
        boolean force = true;
        git.rm(branch_path, path.substring(1), recursive, force); // NOTE: Remove / from path
    }

    public void renameResource(String project, String user, String branch,
            String source, String destination) throws ServerException, IOException {
        ensureProjectBranch(project, user, branch);
        if (source == null)
            throw new ServerException("null source");

        String source_path = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, source);
        File f = new File(source_path);

        if (!f.exists())
            throw new NotFoundException(String.format("%s not found", source_path));

        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
        Git git = new Git();
        git.mv(branch_path, source.substring(1), destination.substring(1), true); // NOTE: Remove / from path
    }

    public void revertResource(String project, String user, String branch,
            String path) throws IOException {
        String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
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

    public void putResourceData(String project, String user, String branch,
            String path, byte[] data) throws ServerException, IOException  {
        ensureProjectBranch(project, user, branch);
        if (path == null)
            throw new ServerException("null path");

        String p = String.format("%s/%s/%s/%s%s", branchRoot, project, user, branch, path);
        File f = new File(p);
        boolean new_file = !f.exists();
        File parent = f.getParentFile();

        if (!parent.exists())
            throw new NotFoundException(String.format("Directory %s not found", parent.getPath()));

        FileOutputStream os = new FileOutputStream(f);
        os.write(data);
        os.close();

        if (new_file) {
            String branch_path = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
            Git git = new Git();
            try {
                git.add(branch_path, path.substring(1));  // NOTE: Remove / from path
            }
            catch (Throwable e) {
                // Rollback if git error
                f.delete();
                throw new ServerException("Failed to add file", e);
            }
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

    public BranchStatus getBranchStatus(String project, String user, String branch) throws IOException, ServerException {
        ensureProjectBranch(project, user, branch);
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
            BranchStatus.Status.Builder status_builder = BranchStatus.Status.newBuilder().setName(fn).setStatus("" + (e.indexStatus == ' ' ? e.workStatus : e.indexStatus));
            if (e.original != null) {
                status_builder.setOriginal(String.format("/%s", e.original));
            }
            builder.addFileStatus(status_builder);
        }

        return builder.build();
    }

    public String[] getBranchNames(String project, String user) {
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

    public void updateBranch(String project, String user, String branch) throws IOException, ServerException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        git.pull(p);
    }

    public void commitBranch(String project, String user, String branch, String message) throws IOException, ServerException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        git.commitAll(p, message);
    }

    public void resolveResource(String project, String user, String branch, String path,
            ResolveStage stage) throws IOException, ServerException {
        ensureProjectBranch(project, user, branch);
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

    public void commitMergeBranch(String project, String user, String branch,
            String message) throws IOException, ServerException {
        ensureProjectBranch(project, user, branch);
        String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);

        Git git = new Git();
        git.commit(p, message);
    }

    public void publishBranch(String project, String user, String branch) throws IOException, ServerException {
        ensureProjectBranch(project, user, branch);
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
        private ProjectConfiguration projectConfiguration;
		private boolean rebuild;

        public BuildRunnable(Server server, int id, String branch_root, String project, String user, String branch, boolean rebuild) {
            this.id = id;
            this.server = server;
            this.branchRoot = branch_root;
            this.project = project;
            this.user = user;
            this.branch = branch;
            this.rebuild = rebuild;

            projectConfiguration = server.getProject(project).configuration;
            workPattern = Pattern.compile(projectConfiguration.getProgressRe());
        }

        @Override
        public void run() {
            String p = String.format("%s/%s/%s/%s", branchRoot, project, user, branch);
            try {
                Result r;

                String[] args = new String[projectConfiguration.getConfigureCommand().getArgsCount()];
                projectConfiguration.getConfigureCommand().getArgsList().toArray(args);
                r = CommandUtil.execCommand(p, null, args);
                if (r.exitValue != 0) {
                    this.server.updateBuild(id, Activity.IDLE, BuildDesc.Result.ERROR, r.stdOut.toString(), r.stdErr.toString());
                    return;
                }

                if (rebuild) {
                    args = new String[projectConfiguration.getRebuildCommand().getArgsCount()];
                    projectConfiguration.getRebuildCommand().getArgsList().toArray(args);
                } else {
                    args = new String[projectConfiguration.getBuildCommand().getArgsCount()];
                    projectConfiguration.getBuildCommand().getArgsList().toArray(args);
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

    public ProjectConfiguration getProjectConfiguration(String project) {
        Project p = getProject(project);
        if (p == null)
            throw new NotFoundException(String.format("Project %s not found", project));

        return p.configuration;
    }

    public EntityManagerFactory getEntityManagerFactory() {
        return emf;
    }
}
