;; Copyright 2020-2024 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.git
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as str]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.git-credentials :as git-credentials]
            [editor.ui :as ui]
            [service.log :as log]
            [util.fn :as fn]
            [util.text-util :as text-util])
  (:import [com.jcraft.jsch Session]
           [java.io File IOException]
           [java.net URI]
           [java.nio.file FileVisitResult Files Path SimpleFileVisitor]
           [java.util Collection]
           [javafx.scene.control ProgressBar]
           [org.eclipse.jgit.api Git PushCommand ResetCommand$ResetType TransportCommand TransportConfigCallback]
           [org.eclipse.jgit.api.errors StashApplyFailureException]
           [org.eclipse.jgit.diff DiffEntry RenameDetector]
           [org.eclipse.jgit.errors MissingObjectException]
           [org.eclipse.jgit.lib BatchingProgressMonitor BranchConfig ObjectId ProgressMonitor Repository]
           [org.eclipse.jgit.revwalk RevCommit RevWalk]
           [org.eclipse.jgit.transport CredentialsProvider JschConfigSessionFactory RemoteConfig SshTransport URIish UsernamePasswordCredentialsProvider]
           [org.eclipse.jgit.treewalk FileTreeIterator TreeWalk]
           [org.eclipse.jgit.treewalk.filter PathFilter PathFilterGroup]))

(set! *warn-on-reflection* true)

;; When opening a project, we ensure the .gitignore file contains every entry on this list.
(defonce required-gitignore-entries ["/.internal"
                                     "/build"])

(defonce default-gitignore-entries (vec (concat required-gitignore-entries
                                                [".DS_Store"
                                                 "Thumbs.db"])))

(defn try-open
  ^Git [^File repo-path]
  (try
    (Git/open repo-path)
    (catch Exception _
      nil)))

(defn get-commit
  ^RevCommit [^Repository repository revision]
  (when-some [object-id (.resolve repository revision)]
    (let [walk (RevWalk. repository)]
      (.setRetainBody walk true)
      (.parseCommit walk object-id))))

(defn- diff-entry->map [^DiffEntry de]
  ; NOTE: We convert /dev/null paths to nil, and convert copies into adds.
  (let [f (fn [p] (when-not (= p "/dev/null") p))
        change-type (-> (.getChangeType de) str .toLowerCase keyword)]
    (if (= :copy change-type)
      {:score 0
       :change-type :add
       :old-path nil
       :new-path (f (.getNewPath de))}
      {:score (.getScore de)
       :change-type change-type
       :old-path (f (.getOldPath de))
       :new-path (f (.getNewPath de))})))

(defn- find-original-for-renamed [ustatus file]
  (->> ustatus
    (filter (fn [e] (= file (:new-path e))))
    (map :old-path)
    (first)))

;; =================================================================================

(defn- as-repository
  ^Repository [git-or-repository]
  (if (instance? Repository git-or-repository)
    git-or-repository
    (.getRepository ^Git git-or-repository)))

(defn user-info [git-or-repository]
  (let [repository (as-repository git-or-repository)
        config (.getConfig repository)
        name (or (.getString config "user" nil "name") "")
        email (or (.getString config "user" nil "email") "")]
    {:name name
     :email email}))

(defn set-user-info! [git-or-repository {new-name :name new-email :email :as user-info}]
  (assert (string? new-name))
  (assert (string? new-email))
  (when (not= user-info (user-info git-or-repository))

    ;; The new user info differs from the stored info.
    ;; Update user info in the repository config.
    (let [repository (as-repository git-or-repository)
          config (.getConfig repository)]
      (.setString config "user" nil "name" new-name)
      (.setString config "user" nil "email" new-email)

      ;; Attempt to save the updated repository config.
      ;; The in-memory config retains the modifications even if this fails.
      (try
        (.save config)
        (catch IOException error
          (log/warn :msg "Failed to save updated user info to Git repository config."
                    :exception error))))))

(defn- remote-name
  ^String [^Repository repository]
  (let [config (.getConfig repository)
        branch (.getBranch repository)
        branch-config (BranchConfig. config branch)
        remote-names (map #(.getName ^RemoteConfig %)
                          (RemoteConfig/getAllRemoteConfigs config))]
    (or (.getRemote branch-config)
        (some (fn [remote-name]
                (when (.equalsIgnoreCase "origin" remote-name)
                  remote-name))
              remote-names)
        (first remote-names))))

(defn remote-info
  ([git-or-repository purpose]
   (let [repository (as-repository git-or-repository)
         remote-name (or (remote-name repository) "origin")]
     (remote-info repository purpose remote-name)))
  ([git-or-repository purpose ^String remote-name]
   (let [repository (as-repository git-or-repository)
         config (.getConfig repository)
         remote (RemoteConfig. config remote-name)]
     (when-some [^URIish uri-ish (first
                                   (case purpose
                                     :fetch (.getURIs remote)
                                     :push (concat
                                             (.getPushURIs remote)
                                             (.getURIs remote))))]
       {:name remote-name
        :scheme (if-some [scheme (.getScheme uri-ish)]
                  (keyword scheme)
                  :ssh)
        :host (.getHost uri-ish)
        :port (.getPort uri-ish)
        :path (.getPath uri-ish)
        :user (.getUser uri-ish)
        :pass (.getPass uri-ish)}))))

(defn remote-uri
  ^URI [{:keys [scheme ^String host ^int port ^String path]
         :or {port -1}
         :as remote-info}]
  (let [^String user-info
        (if-some [user (not-empty (:user remote-info))]
          (if-some [pass (not-empty (:pass remote-info))]
            (str user ":" pass)
            user))]
    (URI. (name scheme) user-info host port path nil nil)))

;; Does the equivalent *config-wise* of:
;; > git config remote.origin.url url
;; > git push -u origin
;; Which is:
;; > git config remote.origin.url url
;; > git config remote.origin.fetch +refs/heads/*:refs/remotes/origin/*
;; > git config branch.master.remote origin
;; > git config branch.master.merge refs/heads/master
;; according to https://stackoverflow.com/questions/27823940/jgit-pushing-a-branch-and-add-upstream-u-option
(defn config-remote! [^Git git url]
  (let [config (.. git getRepository getConfig)]
    (doto config
      (.setString "remote" "origin" "url" url)
      (.setString "remote" "origin" "fetch" "+refs/heads/*:refs/remotes/origin/*")
      (.setString "branch" "master" "remote" "origin")
      (.setString "branch" "master" "merge" "refs/heads/master")
      (.save))))

(defn worktree [^Git git]
  (.getWorkTree (.getRepository git)))

(defn get-current-commit-ref [^Git git]
  (get-commit (.getRepository git) "HEAD"))

(defn status [^Git git]
  (let [s (-> git (.status) (.call))]
    {:added                   (set (.getAdded s))
     :changed                 (set (.getChanged s))
     :conflicting             (set (.getConflicting s))
     :ignored-not-in-index    (set (.getIgnoredNotInIndex s))
     :missing                 (set (.getMissing s))
     :modified                (set (.getModified s))
     :removed                 (set (.getRemoved s))
     :uncommitted-changes     (set (.getUncommittedChanges s))
     :untracked               (set (.getUntracked s))
     :untracked-folders       (set (.getUntrackedFolders s))
     :conflicting-stage-state (apply hash-map
                                     (mapcat (fn [[k v]] [k (-> v str camel/->kebab-case keyword)])
                                             (.getConflictingStageState s)))}))

(defn has-local-changes? [^Git git]
  (let [status (status git)]
    (boolean
      (some #(pos? (count (get status %)))
            [:added :changed :missing :modified :removed :untracked]))))

(defn- make-add-diff-entry [file-path]
  {:score 0 :change-type :add :old-path nil :new-path file-path})

(defn- make-delete-diff-entry [file-path]
  {:score 0 :change-type :delete :old-path file-path :new-path nil})

(defn- make-modify-diff-entry [file-path]
  {:score 0 :change-type :modify :old-path file-path :new-path file-path})

(defn- diff-entry-path
  ^String [{:keys [old-path new-path]}]
  (or new-path old-path))

(defn unified-status
  "Get the actual status by comparing contents on disk and HEAD. The state of
  the index (i.e. whether or not a file is staged) does not matter."
  ([^Git git]
   (unified-status git (status git)))
  ([^Git git git-status]
   (let [changed-paths (into #{}
                             (mapcat git-status)
                             [:added :changed :missing :modified :removed :untracked])]
     (if (empty? changed-paths)
       []
       (let [repository (.getRepository git)
             rename-detector (doto (RenameDetector. repository)
                               (.addAll (with-open [tree-walk (doto (TreeWalk. repository)
                                                                (.addTree (.getTree ^RevCommit (get-commit repository "HEAD")))
                                                                (.addTree (FileTreeIterator. repository))
                                                                (.setFilter (PathFilterGroup/createFromStrings ^Collection changed-paths))
                                                                (.setRecursive true))]
                                          (DiffEntry/scan tree-walk))))]
         (try
           (let [diff-entries (.compute rename-detector nil)]
             (mapv diff-entry->map diff-entries))
           (catch MissingObjectException _
             ;; TODO: Workaround for what appears to be a bug inside JGit.
             ;; The rename detector failed for some reason. Report the status
             ;; without considering renames. This results in separate :added and
             ;; :deleted entries for unstaged renames, but will resolve into a
             ;; single :renamed entry once staged.
             (sort-by diff-entry-path
                      (concat (map make-add-diff-entry (concat (:added git-status) (:untracked git-status)))
                              (map make-modify-diff-entry (concat (:changed git-status) (:modified git-status)))
                              (map make-delete-diff-entry (concat (:missing git-status) (:removed git-status))))))))))))

(defn file ^java.io.File [^Git git file-path]
  (io/file (str (worktree git) "/" file-path)))

(defn show-file
  (^bytes [^Git git name]
   (show-file git name "HEAD"))
  (^bytes [^Git git name ref]
   (let [repo (.getRepository git)]
     (with-open [rw (RevWalk. repo)]
       (let [last-commit-id (.resolve repo ref)
             commit (.parseCommit rw last-commit-id)
             tree (.getTree commit)]
         (with-open [tw (TreeWalk. repo)]
           (.addTree tw tree)
           (.setRecursive tw true)
           (.setFilter tw (PathFilter/create name))
           (.next tw)
           (let [id (.getObjectId tw 0)]
             (when (not= (ObjectId/zeroId) id)
               (let [loader (.open repo id)]
                 (.getBytes loader))))))))))

(defn locked-files
  "Returns a set of all files in the repository that could cause major operations
  on the work tree to fail due to permissions issues or file locks from external
  processes. Does not include ignored files or files below the .git directory."
  [^Git git]
  (let [locked-files (volatile! (transient #{}))
        repository (.getRepository git)
        work-directory-path (.toPath (.getWorkTree repository))
        dot-git-directory-path (.toPath (.getDirectory repository))
        {dirs true files false} (->> (.. git status call getIgnoredNotInIndex)
                                     (map #(.resolve work-directory-path ^String %))
                                     (group-by #(.isDirectory (.toFile ^Path %))))
        ignored-directory-paths (set dirs)
        ignored-file-paths (set files)]
    (Files/walkFileTree work-directory-path
                        (proxy [SimpleFileVisitor] []
                          (preVisitDirectory [^Path directory-path _attrs]
                            (if (contains? ignored-directory-paths directory-path)
                              FileVisitResult/SKIP_SUBTREE
                              FileVisitResult/CONTINUE))
                          (visitFile [^Path file-path _attrs]
                            (when (and (not (.startsWith file-path dot-git-directory-path))
                                       (not (contains? ignored-file-paths file-path)))
                              (let [file (.toFile file-path)]
                                (when (fs/locked-file? file)
                                  (vswap! locked-files conj! file))))
                            FileVisitResult/CONTINUE)
                          (visitFileFailed [^Path file-path _attrs]
                            (vswap! locked-files conj! (.toFile file-path))
                            FileVisitResult/CONTINUE)))
    (persistent! (deref locked-files))))

(defn locked-files-error-message [locked-files]
  (str/join "\n" (concat ["The following project files are locked or in use by another process:"
                          ""]
                         (map dialogs/indent-with-bullet
                              (sort locked-files))
                         [""
                          "Please ensure they are writable and quit other applications that reference files in the project before trying again."])))

(defn ensure-gitignore-configured!
  "When supplied a non-nil Git instance, ensures the repository has a .gitignore
  file that ignores our .internal and build output directories. Returns true if
  a change was made to the .gitignore file or a new .gitignore was created."
  [^Git git]
  (if (nil? git)
    false
    (let [gitignore-file (file git ".gitignore")
          ^String old-gitignore-text (when (.exists gitignore-file) (slurp gitignore-file :encoding "UTF-8"))
          old-gitignore-entries (some-> old-gitignore-text str/split-lines)
          old-gitignore-entries-set (into #{} (remove str/blank?) old-gitignore-entries)
          line-separator (text-util/guess-line-separator old-gitignore-text)]
      (if (empty? old-gitignore-entries-set)
        (do (spit gitignore-file (str/join line-separator default-gitignore-entries) :encoding "UTF-8")
            true)
        (let [new-gitignore-entries (into old-gitignore-entries
                                          (remove (fn [required-entry]
                                                    (or (contains? old-gitignore-entries-set required-entry)
                                                        (contains? old-gitignore-entries-set (fs/without-leading-slash required-entry)))))
                                          required-gitignore-entries)]
          (if (= old-gitignore-entries new-gitignore-entries)
            false
            (do (spit gitignore-file (str/join line-separator new-gitignore-entries) :encoding "UTF-8")
                true)))))))

(defn internal-files-are-tracked?
  "Returns true if the supplied Git instance is non-nil and we detect any
  tracked files under the .internal or build directories. This means a commit
  was made with an improperly configured .gitignore, and will likely cause
  issues during sync with collaborators."
  [^Git git]
  (if (nil? git)
    false
    (let [repo (.getRepository git)]
      (with-open [rw (RevWalk. repo)]
        (let [commit-id (.resolve repo "HEAD")
              commit (.parseCommit rw commit-id)
              tree (.getTree commit)
              ^Collection path-prefixes [".internal/" "build/"]]
          (with-open [tw (TreeWalk. repo)]
            (.addTree tw tree)
            (.setRecursive tw true)
            (.setFilter tw (PathFilterGroup/createFromStrings path-prefixes))
            (.next tw)))))))

(defn clone!
  "Clone a repository into the specified directory."
  [^CredentialsProvider creds ^String remote-url ^File directory ^ProgressMonitor progress-monitor]
  (try
    (with-open [_ (.call (doto (Git/cloneRepository)
                           (.setCredentialsProvider creds)
                           (.setProgressMonitor progress-monitor)
                           (.setURI remote-url)
                           (.setDirectory directory)))]
      nil)
    (catch Exception e
      ;; The .call method throws an exception if the operation was cancelled.
      ;; Sadly it appears there is not a specific exception type for that, so
      ;; we silence any exceptions if the operation was cancelled.
      (when-not (.isCancelled progress-monitor)
        (throw e)))))

(defn- make-transport-config-callback
  ^TransportConfigCallback [^String ssh-session-password]
  {:pre [(and (string? ssh-session-password)
              (not (empty? ssh-session-password)))]}
  (let [ssh-session-factory
        (proxy [JschConfigSessionFactory] []
          (configure [_host session]
            (.setPassword ^Session session ssh-session-password)))]
    (reify TransportConfigCallback
      (configure [_this transport]
        (.setSshSessionFactory ^SshTransport transport ssh-session-factory)))))

(defn make-credentials-provider
  ^CredentialsProvider [credentials]
  (let [^String username (or (:username credentials) "")
        ^String password (or (:password credentials) "")]
    (UsernamePasswordCredentialsProvider. username password)))

(defn- configure-transport-command!
  ^TransportCommand [^TransportCommand command
                     purpose
                     {:keys [encrypted-credentials
                             ^int timeout-seconds] :as _opts
                      :or {timeout-seconds -1}}]
  ;; Taking GitHub as an example, clones made over the https:// protocol
  ;; authenticate with a username & password in order to push changes. You can
  ;; also make a Personal Access Token on GitHub to use in place of a password.
  ;;
  ;; Clones made over the git:// protocol can only pull, not push. The files are
  ;; transferred unencrypted.
  ;;
  ;; Clones made over the ssh:// protocol use public key authentication. You
  ;; must generate a public / private key pair and upload the public key to your
  ;; GitHub account. The private key is loaded from the `.ssh` directory in the
  ;; HOME folder. It will look for files named `identity`, `id_rsa` and `id_dsa`
  ;; and it should "just work". However, if a passphrase was used to create the
  ;; keys, we need to override createDefaultJSch in a subclassed instance of the
  ;; JschConfigSessionFactory class in order to associate the passphrase with a
  ;; key file. We do not currently do this here.
  ;;
  ;; Most of this information was gathered from here:
  ;; https://www.codeaffine.com/2014/12/09/jgit-authentication/
  (case (:scheme (remote-info (.getRepository command) purpose))
    :https
    (let [credentials (git-credentials/decrypt-credentials encrypted-credentials)
          credentials-provider (make-credentials-provider credentials)]
      (.setCredentialsProvider command credentials-provider))

    :ssh
    (let [credentials (git-credentials/decrypt-credentials encrypted-credentials)]
      (when-some [ssh-session-password (not-empty (:ssh-session-password credentials))]
        (let [transport-config-callback (make-transport-config-callback ssh-session-password)]
          (.setTransportConfigCallback command transport-config-callback))))

    nil)
  (cond-> command
          (pos? timeout-seconds)
          (.setTimeout timeout-seconds)))

(defn pull! [^Git git opts]
  (-> (.pull git)
      (configure-transport-command! :fetch opts)
      (.call)))

(defn- make-batching-progress-monitor
  ^BatchingProgressMonitor [weights-by-task cancelled-atom on-progress!]
  (let [^double sum-weight (reduce + 0.0 (vals weights-by-task))
        normalized-weights-by-task (into {}
                                         (map (fn [[task ^double weight]]
                                                [task (/ weight sum-weight)]))
                                         weights-by-task)
        progress-by-task (atom (into {} (map #(vector % 0)) (keys weights-by-task)))
        set-progress (fn [task percent] (swap! progress-by-task assoc task percent))
        current-progress (fn []
                           (reduce +
                                   0.0
                                   (keep (fn [[task ^long percent]]
                                           (when-some [^double weight (normalized-weights-by-task task)]
                                             (* percent weight 0.01)))
                                         @progress-by-task)))]
    (proxy [BatchingProgressMonitor] []
      (onUpdate
        ([taskName workCurr])
        ([taskName workCurr workTotal percentDone]
          (set-progress taskName percentDone)
          (on-progress! (current-progress))))

      (onEndTask
        ([taskName workCurr])
        ([taskName workCurr workTotal percentDone]))

      (isCancelled []
        (boolean (some-> cancelled-atom deref))))))

(def ^:private ^:const push-tasks
  ;; TODO: Tweak these weights.
  {"Finding sources" 1
   "Writing objects" 1
   "remote: Updating references" 1})

(defn- configure-push-command!
  ^PushCommand [^PushCommand command {:keys [dry-run on-progress] :as _opts}]
  (cond-> command

          dry-run
          (.setDryRun true)

          (some? on-progress)
          (.setProgressMonitor (make-batching-progress-monitor push-tasks nil on-progress))))

(defn push! [^Git git opts]
  (-> (.push git)
      (configure-push-command! opts)
      (configure-transport-command! :push opts)
      (.call)))

(defn make-add-change [file-path]
  {:change-type :add :old-path nil :new-path file-path})

(defn make-delete-change [file-path]
  {:change-type :delete :old-path file-path :new-path nil})

(defn make-modify-change [file-path]
  {:change-type :modify :old-path file-path :new-path file-path})

(defn make-rename-change [old-path new-path]
  {:change-type :rename :old-path old-path :new-path new-path})

(defn change-path [{:keys [old-path new-path]}]
  (or new-path old-path))

(defn stage-change! [^Git git {:keys [change-type old-path new-path]}]
  (case change-type
    (:add :modify) (-> git .add (.addFilepattern new-path) .call)
    :delete        (-> git .rm (.addFilepattern old-path) (.setCached true) .call)
    :rename        (do (-> git .rm (.addFilepattern old-path) (.setCached true) .call)
                       (-> git .add (.addFilepattern new-path) .call))))

(defn unstage-change! [^Git git {:keys [change-type old-path new-path]}]
  (case change-type
    (:add :modify) (-> git .reset (.addPath new-path) .call)
    :delete        (-> git .reset (.addPath old-path) .call)
    :rename        (-> git .reset (.addPath old-path) (.addPath new-path) .call)))

(defn- find-case-changes [added-paths removed-paths]
  (into {}
        (keep (fn [^String added-path]
                (some (fn [^String removed-path]
                        (when (.equalsIgnoreCase added-path removed-path)
                          [added-path removed-path]))
                      removed-paths)))
        added-paths))

(defn- perform-case-changes! [^Git git case-changes]
  ;; In case we fail to perform a case change, attempt to undo the successfully
  ;; performed case changes before throwing.
  (let [performed-case-changes-atom (atom [])]
    (try
      (doseq [[new-path old-path] case-changes]
        (let [new-file (file git new-path)
              old-file (file git old-path)]
          (fs/move-file! old-file new-file)
          (swap! performed-case-changes-atom conj [new-file old-file])))
      (catch Throwable error
        ;; Attempt to undo the performed case changes before throwing.
        (doseq [[new-file old-file] (rseq @performed-case-changes-atom)]
          (fs/move-file! new-file old-file {:fail :silently}))
        (throw error)))))

(defn- undo-case-changes! [^Git git case-changes]
  (perform-case-changes! git (map reverse case-changes)))

(defn revert-to-revision!
  "High-level revert. Resets the working directory to the state it would have
  after a clean checkout of the specified start-ref. Performs the equivalent of
  git reset --hard
  git clean --force -d"
  [^Git git ^RevCommit start-ref]
  ;; On case-insensitive file systems, we must manually revert case changes.
  ;; Otherwise the new-cased files will be removed during the clean call.
  (when-not fs/case-sensitive?
    (let [{:keys [missing untracked]} (status git)
          case-changes (find-case-changes untracked missing)]
      (undo-case-changes! git case-changes)))
  (-> (.reset git)
      (.setMode ResetCommand$ResetType/HARD)
      (.setRef (.name start-ref))
      (.call))
  (-> (.clean git)
      (.setCleanDirectories true)
      (.call)))

(defn- revert-case-changes!
  "Finds and reverts any case changes in the supplied git status. Returns
  a map of the case changes that were reverted."
  [^Git git {:keys [added changed missing removed untracked] :as _status}]
  ;; If we're on a case-insensitive file system, we revert case-changes before
  ;; stashing. The case-changes will be restored when we apply the stash.
  (let [case-changes (find-case-changes (set/union added untracked)
                                        (set/union removed missing))]
    (when (seq case-changes)
      ;; Unstage everything so that we can safely undo the case changes.
      ;; This is fine, because we will stage everything before stashing.
      (when-some [staged-paths (not-empty (set/union added changed removed))]
        (let [reset-command (.reset git)]
          (doseq [path staged-paths]
            (.addPath reset-command path))
          (.call reset-command)))

      ;; Undo case-changes.
      (undo-case-changes! git case-changes))

    case-changes))

(defn- stage-removals! [^Git git status pred]
  (when-some [removed-paths (not-empty (into #{} (filter pred) (:missing status)))]
    (let [rm-command (.rm git)]
      (.setCached rm-command true)
      (doseq [path removed-paths]
        (.addFilepattern rm-command path))
      (.call rm-command)
      nil)))

(defn- stage-additions! [^Git git status pred]
  (when-some [changed-paths (not-empty (into #{} (filter pred) (concat (:modified status) (:untracked status))))]
    (let [add-command (.add git)]
      (doseq [path changed-paths]
        (.addFilepattern add-command path))
      (.call add-command)
      nil)))

(defn stage-all!
  "Stage all unstaged changes in the specified Git repo."
  [^Git git]
  (let [status (status git)
        include? fn/constantly-true]
    (stage-removals! git status include?)
    (stage-additions! git status include?)))

(defn stash!
  "High-level stash. Before stashing, stages all changes. We do this because
  stashes internally treat untracked files differently from tracked files.
  Normally untracked files are not stashed at all, but even when using
  the setIncludeUntracked flag, untracked files are dealt with separately from
  tracked files.

  When later applied, a conflict among the tracked files will abort the stash
  apply command before the untracked files are restored. For our purposes, we
  want a unified set of conflicts among all the tracked and untracked files.

  Before stashing, we also revert any case-changes if we're running on a
  case-insensitive file system. We will reapply the case-changes when the stash
  is applied. If we do not do this, we will lose any remote changes to case-
  changed files when applying the stash.

  Returns nil if there was nothing to stash, or a map with the stash ref and
  the set of file paths that were staged at the time the stash was made."
  [^Git git]
  (let [pre-stash-status (status git)
        reverted-case-changes (when-not fs/case-sensitive?
                                (revert-case-changes! git pre-stash-status))]
    (stage-all! git)
    (when-some [stash-ref (-> git .stashCreate .call)]
      {:ref stash-ref
       :case-changes reverted-case-changes
       :staged (set/union (:added pre-stash-status)
                          (:changed pre-stash-status)
                          (:removed pre-stash-status))})))

(defn stash-apply! [^Git git stash-info]
  (when (some? stash-info)
    ;; Apply the stash. The apply-result will be either an ObjectId returned
    ;; from (.call StashApplyCommand) or a StashApplyFailureException if thrown.
    (let [apply-result (try
                         (-> (.stashApply git)
                             (.setStashRef (.name ^RevCommit (:ref stash-info)))
                             (.call))
                         (catch StashApplyFailureException error
                           error))
          status-after-apply (status git)
          paths-with-conflicts (set (keys (:conflicting-stage-state status-after-apply)))
          paths-to-unstage (set/difference (set/union (:added status-after-apply)
                                                      (:changed status-after-apply)
                                                      (:removed status-after-apply))
                                           paths-with-conflicts)]

      ;; Unstage everything that is without conflict. Later we will stage
      ;; everything that was staged at the time the stash was made.
      (when-some [staged-paths (not-empty paths-to-unstage)]
        (let [reset-command (.reset git)]
          (doseq [path staged-paths]
            (.addPath reset-command path))
          (.call reset-command)))

      ;; Restore any reverted case changes. Skip over case changes that involve
      ;; conflicting paths just in case.
      (let [case-changes (remove (partial some paths-with-conflicts)
                                 (:case-changes stash-info))]
        (perform-case-changes! git case-changes))

      ;; Restore staged files state from before the stash.
      (when (empty? paths-with-conflicts)
        (let [status (status git)
              was-staged? (:staged stash-info)]
          (stage-removals! git status was-staged?)
          (stage-additions! git status was-staged?)))

      (if (instance? Throwable apply-result)
        (throw apply-result)
        apply-result))))

(defn stash-drop! [^Git git stash-info]
  (let [stash-ref ^RevCommit (:ref stash-info)
        stashes (map-indexed vector (.call (.stashList git)))
        matching-stash (first (filter #(= stash-ref (second %)) stashes))]
    (when matching-stash
      (-> (.stashDrop git)
          (.setStashRef (first matching-stash))
          (.call)))))

(defn commit [^Git git ^String message]
  (-> (.commit git)
      (.setMessage message)
      (.call)))

;; "High level" revert
;; * Changed files are checked out
;; * New files are removed
;; * Deleted files are checked out
;; NOTE: Not to be confused with "git revert"
(defn revert [^Git git files]
  (let [us (unified-status git)
        renames (into {}
                      (keep (fn [new-path]
                              (when-some [old-path (find-original-for-renamed us new-path)]
                                [new-path old-path])))
                      files)
        others (into [] (remove renames) files)]

    ;; Revert renames first.
    (doseq [[new-path old-path] renames]
      (-> git .rm (.addFilepattern new-path) (.setCached true) .call)
      (fs/delete-file! (file git new-path))
      (-> git .reset (.addPath old-path) .call)
      (-> git .checkout (.addPath old-path) .call))

    ;; Revert others.
    (when (seq others)
      (let [co (-> git (.checkout))
            reset (-> git (.reset) (.setRef "HEAD"))]
        (doseq [path others]
          (.addPath co path)
          (.addPath reset path))
        (.call reset)
        (.call co))

      ;; Delete all untracked files in "others" as a last step
      ;; JGit doesn't support this operation with RmCommand
      (let [s (status git)]
        (doseq [path others]
          (when (contains? (:untracked s) path)
            (fs/delete-file! (file git path) {:missing :fail})))))))

(def ^:private ^:const clone-tasks
  {"remote: Finding sources" 1
   "Receiving objects" 88
   "Resolving deltas" 10
   "Updating references" 1})

(defn make-clone-monitor
  ^ProgressMonitor [^ProgressBar progress-bar cancelled-atom]
  (make-batching-progress-monitor clone-tasks cancelled-atom
    (fn [progress]
      (ui/run-later (.setProgress progress-bar progress)))))

;; =================================================================================

(defn selection-diffable? [selection]
  (and (= 1 (count selection))
       (let [change-type (:change-type (first selection))]
         (and (keyword? change-type)
              (not= :add change-type)
              (not= :delete change-type)))))

(defn selection-diff-data [git selection]
  (let [change (first selection)
        old-path (or (:old-path change) (:new-path change) )
        new-path (or (:new-path change) (:old-path change) )
        old (String. ^bytes (show-file git old-path))
        new (slurp (file git new-path))
        binary? (not-every? text-util/text-char? new)]
    {:binary? binary?
     :new new
     :new-path new-path
     :old old
     :old-path old-path}))

(defn init
  ^Git [^String path]
  (-> (Git/init) (.setDirectory (File. path)) (.call)))
