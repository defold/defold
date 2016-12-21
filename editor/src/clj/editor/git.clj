(ns editor.git
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [editor
             [prefs :as prefs]
             [ui :as ui]])
  (:import javafx.scene.control.ProgressBar
           [java.io File]
           [org.eclipse.jgit.api Git ResetCommand$ResetType]
           [org.eclipse.jgit.api.errors StashApplyFailureException]
           [org.eclipse.jgit.diff DiffEntry RenameDetector]
           [org.eclipse.jgit.lib BatchingProgressMonitor Repository]
           [org.eclipse.jgit.revwalk RevCommit RevWalk]
           org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider
           [org.eclipse.jgit.treewalk FileTreeIterator TreeWalk]
           [org.eclipse.jgit.treewalk.filter NotIgnoredFilter PathFilter]))

(set! *warn-on-reflection* true)

(defn get-commit [^Repository repository revision]
  (let [walk (RevWalk. repository)]
    (.setRetainBody walk true)
    (.parseCommit walk (.resolve repository revision))))

(defn- diff-entry->map [^DiffEntry de]
  ; NOTE: We convert /dev/null to nil.
  (let [f (fn [p] (when-not (= p "/dev/null") p))]
    {:score (.getScore de)
     :change-type (-> (.getChangeType de) str .toLowerCase keyword)
     :old-path (f (.getOldPath de))
     :new-path (f (.getNewPath de))}))

(defn- find-original-for-renamed [ustatus file]
  (->> ustatus
    (filter (fn [e] (= file (:new-path e))))
    (map :old-path)
    (first)))

;; =================================================================================

(defn credentials [prefs]
  (let [email (prefs/get-prefs prefs "email" nil)
        token (prefs/get-prefs prefs "token" nil)]
    (UsernamePasswordCredentialsProvider. ^String email ^String token)))

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

(defmacro with-autocrlf-disabled
  [repo & body]
  `(let [config# (.getConfig ~repo)
         autocrlf# (.getString config# "core" nil "autocrlf")]
     (try
       (.setString config# "core" nil "autocrlf" "false")
       ~@body
       (finally
         (.setString config# "core" nil "autocrlf" autocrlf#)))))

(defn unified-status
  "Get the actual status by comparing contents on disk and HEAD. The index, i.e. staged files, are ignored"
  [^Git git]
  (let [repository (.getRepository git)]
    ;; without this, whitespace conversion will happen when diffing
    (with-autocrlf-disabled repository
      (let [tw               (TreeWalk. repository)
            rev-commit-index (.addTree tw (.getTree ^RevCommit (get-commit repository "HEAD")))
            file-tree-index  (.addTree tw (FileTreeIterator. repository))]
        (.setRecursive tw true)
        (.setFilter tw (NotIgnoredFilter. file-tree-index))
        (let [rd (RenameDetector. repository)]
          (.addAll rd (DiffEntry/scan tw))
          (->> (.compute rd (.getObjectReader tw) nil)
               (mapv diff-entry->map)))))))

(defn file ^java.io.File [^Git git file-path]
  (io/file (str (worktree git) "/" file-path)))

(defn show-file
  ([^Git git name]
   (show-file git name "HEAD"))
  ([^Git git name ref]
   (let [repo         (.getRepository git)
         lastCommitId (.resolve repo ref)
         rw           (RevWalk. repo)
         commit       (.parseCommit rw lastCommitId)
         tree         (.getTree commit)
         tw           (TreeWalk. repo)]
     (.addTree tw tree)
     (.setRecursive tw true)
     (.setFilter tw (PathFilter/create name))
     (.next tw)
     (let [id     (.getObjectId tw 0)
           loader (.open repo id)
           ret    (.getBytes loader)]
       (.dispose rw)
       (.close repo)
       ret))))

(defn pull [^Git git ^UsernamePasswordCredentialsProvider creds]
  (-> (.pull git)
      (.setCredentialsProvider creds)
      (.call)))

(defn push [^Git git ^UsernamePasswordCredentialsProvider creds]
  (-> (.push git)
      (.setCredentialsProvider creds)
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
    :delete        (-> git .rm (.addFilepattern old-path) .call)
    :rename        (do (-> git .add (.addFilepattern new-path) .call)
                       (-> git .rm (.addFilepattern old-path) .call))))

(defn unstage-change! [^Git git {:keys [change-type old-path new-path]}]
  (case change-type
    (:add :modify) (-> git .reset (.addPath new-path) .call)
    :delete        (-> git .reset (.addPath old-path) .call)
    :rename        (do (-> git .reset (.addPath new-path) .call)
                       (-> git .reset (.addPath old-path) .call))))

(defn revert-to-revision! [^Git git ^RevCommit start-ref]
  "High-level revert. Resets the working directory to the state it would have
  after a clean checkout of the specified start-ref. Performs the equivalent of
  git reset --hard
  git clean --force -d"
  (-> (.reset git)
      (.setMode ResetCommand$ResetType/HARD)
      (.setRef (.name start-ref))
      (.call))
  (-> (.clean git)
      (.setCleanDirectories true)
      (.call)))

(defn stash!
  "High-level stash. Before stashing, stages all untracked files. We do this
  because stashes internally treat untracked files differently from tracked
  files. Normally untracked files are not stashed at all, but even when using
  the setIncludeUntracked flag, untracked files are dealt with separately from
  tracked files.

  When later applied, a conflict among the tracked files will abort the stash
  apply command before the untracked files are restored. For our purposes, we
  want a unified set of conflicts among all the tracked and untracked files.

  Returns nil if there was nothing to stash, or a map with the stash ref and
  the set of file paths that were untracked at the time the stash was made."
  [^Git git]
  (let [untracked (:untracked (status git))]
    ; Stage any untracked files.
    (when (seq untracked)
      (let [add-command (.add git)]
        (doseq [path untracked]
          (.addFilepattern add-command path))
        (.call add-command)))

    ; Stash local changes and return stash info.
    (when-let [stash-ref (-> git .stashCreate .call)]
      {:ref stash-ref
       :untracked untracked})))

(defn stash-apply!
  [^Git git stash-info]
  (when stash-info
    ; Apply the stash. The apply-result will be either an ObjectId returned from
    ; (.call StashApplyCommand) or a StashApplyFailureException if thrown.
    (let [apply-result (try
                         (-> (.stashApply git)
                             (.setStashRef (.name ^RevCommit (:ref stash-info)))
                             (.call))
                         (catch StashApplyFailureException error
                           error))]
      ; Restore untracked state of all non-conflicting files
      ; that were untracked at the time the stash was made.
      (let [paths-with-conflicts (set (keys (:conflicting-stage-state (status git))))
            paths-to-unstage (into #{}
                                   (remove paths-with-conflicts)
                                   (:untracked stash-info))]
        (when (seq paths-to-unstage)
          (let [reset-command (.reset git)]
            (doseq [path paths-to-unstage]
              (.addPath reset-command path))
            (.call reset-command))))
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
        extra (->> files
                (map (fn [f] (find-original-for-renamed  us f)))
                (remove nil?))
        co (-> git (.checkout))
        reset (-> git (.reset) (.setRef "HEAD"))]

    (doseq [f (concat files extra)]
      (.addPath co f)
      (.addPath reset f))
    (.call reset)
    (.call co))

  ;; Delete all untracked files in "files" as a last step
  ;; JGit doesn't support this operation with RmCommand
  (let [s (status git)]
    (doseq [f files]
      (when (contains? (:untracked s) f)
        (io/delete-file (file git f))))))

(defn make-clone-monitor [^ProgressBar progress-bar]
  (let [tasks (atom {"remote: Finding sources" 0
                     "remote: Getting sizes" 0
                     "remote: Compressing objects" 0
                     "Receiving objects" 0
                     "Resolving deltas" 0})
        set-progress (fn [task percent] (swap! tasks assoc task percent))
        current-progress (fn [] (let [n (count @tasks)]
                                  (if (zero? n) 0 (/ (reduce + (vals @tasks)) (float n) 100.0))))]
    (proxy [BatchingProgressMonitor] []
     (onUpdate
       ([taskName workCurr])
       ([taskName workCurr workTotal percentDone]
         (set-progress taskName percentDone)
         (ui/run-later (.setProgress progress-bar (current-progress)))))

     (onEndTask
       ([taskName workCurr])
       ([taskName workCurr workTotal percentDone])))))

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
        new (slurp (file git new-path))]
    {:new new
     :new-path new-path
     :old old
     :old-path old-path}))

(defn init [^String path]
  (-> (Git/init) (.setDirectory (File. path)) (.call)))
