(ns editor.git
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [editor
             [prefs :as prefs]
             [ui :as ui]])
  (:import javafx.scene.control.ProgressBar
           [org.eclipse.jgit.api Git ResetCommand ResetCommand$ResetType]
           [org.eclipse.jgit.diff DiffEntry RenameDetector]
           [org.eclipse.jgit.lib BatchingProgressMonitor Repository]
           [org.eclipse.jgit.revwalk RevCommit RevWalk]
           org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider
           [org.eclipse.jgit.treewalk FileTreeIterator TreeWalk]
           [org.eclipse.jgit.treewalk.filter NotIgnoredFilter PathFilter]))

(set! *warn-on-reflection* true)

(defn- get-commit [^Repository repository revision]
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
    (filter (fn [e] (= (:new-path file))))
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
     :uncommited-changes      (set (.getUncommittedChanges s))
     :untracked               (set (.getUntracked s))
     :untracked-folders       (set (.getUntrackedFolders s))
     :conflicting-stage-state (apply hash-map
                                     (mapcat (fn [[k v]] [k (-> v str camel/->kebab-case keyword)])
                                             (.getConflictingStageState s)))}))

(defn unified-status
  "Get the actual status by comparing contents on disk and HEAD. The index, i.e. staged files, are ignored"
  [^Git git]
  (let [repository       (.getRepository git)
        tw               (TreeWalk. repository)
        rev-commit-index (.addTree tw (.getTree ^RevCommit (get-commit repository "HEAD")))
        file-tree-index  (.addTree tw (FileTreeIterator. repository))]
    (.setRecursive tw true)
    (.setFilter tw (NotIgnoredFilter. file-tree-index))
    (let [rd (RenameDetector. repository)]
      (.addAll rd (DiffEntry/scan tw))
      (->> (.compute rd (.getObjectReader tw) nil)
           (mapv diff-entry->map)))))

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

(defn stage-file [^Git git file]
  (-> (.add git)
      (.addFilepattern file)
      (.call)))

(defn unstage-file [^Git git file]
  (-> (.reset git)
      (.addPath file)
      (.call)))

(defn hard-reset [^Git git ^RevCommit start-ref]
  (-> (.reset git)
      (.setMode ResetCommand$ResetType/HARD)
      (.setRef (.name start-ref))
      (.call)))

(defn stash [^Git git]
  (-> (.stashCreate git)
      (.setIncludeUntracked true)
      (.call)))

(defn stash-apply [^Git git ^RevCommit stash-ref]
  (when stash-ref
    (-> (.stashApply git)
        (.setStashRef (.name stash-ref))
        (.call))))

(defn stash-drop [^Git git ^RevCommit stash-ref]
  (let [stashes (map-indexed vector (.call (.stashList git)))
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
        (io/delete-file (str (.getWorkTree (.getRepository git)) "/" f))))))

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
