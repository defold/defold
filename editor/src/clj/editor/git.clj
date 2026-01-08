;; Copyright 2020-2026 The Defold Foundation
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
            [clojure.string :as str]
            [editor.fs :as fs]
            [util.fn :as fn]
            [util.text-util :as text-util])
  (:import [java.io File]
           [java.util Collection]
           [org.eclipse.jgit.api Git]
           [org.eclipse.jgit.diff DiffEntry RenameDetector]
           [org.eclipse.jgit.errors MissingObjectException]
           [org.eclipse.jgit.lib BranchConfig ObjectId Repository]
           [org.eclipse.jgit.revwalk RevCommit RevWalk]
           [org.eclipse.jgit.transport RemoteConfig URIish]
           [org.eclipse.jgit.treewalk FileTreeIterator TreeWalk]
           [org.eclipse.jgit.treewalk.filter PathFilter PathFilterGroup]))

(set! *warn-on-reflection* true)

;; When opening a project, we ensure the .gitignore file contains every entry on this list.
(defonce required-gitignore-entries ["/.editor_settings"
                                     "/.internal"
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
