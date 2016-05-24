(ns editor.sync
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.set :as set]
            [editor.dialogs :as dialogs]
            [editor.diff-view :as diff-view]
            [editor.git :as git]
            [editor.ui :as ui])
  (:import [org.eclipse.jgit.api Git]
           [org.eclipse.jgit.api ResetCommand$ResetType CheckoutCommand$Stage]
           [org.eclipse.jgit.api.errors CheckoutConflictException]
           [javafx.geometry Pos]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.control CheckBox Label MenuButton ContentDisplay]
           [javafx.scene.layout AnchorPane HBox BorderPane Pane Priority]))

"
TODO
* cancel must do a git reset --hard XYZ and where XYZ is the last commit prior to starting the sync process
*
"

"

Flow functions

* stage
* unstage
* commit
* resolve
* commit-merge
* push

Typical flow

* Stash current
* Select files to commit
* git commit
* git pull
* merge
* git push
* back to git pull if someone has pushed while merging
* drop stash

Cancel
* Reset hard
* Drop stash

Misc
* Ensure that we test a variety of combinations with files
- U + U
- U + D
- etc
* What about stashed files?
- Tests?

Questions
* Change files immediately or postpone while merging?
* Show U, D, etc?

"

"TODO: stash-rev can be null when nothing to stash
set :status to :done?"
(defn make-flow [git message]
  (let [stash-rev (git/stash git)]
    (-> (.stashApply git) .call)
    {:git git
     :unified-status (git/unified-status git)
     :staged #{}
     :message message
     :status :start
     :state :start
     :stash-rev stash-rev}))

(defn stage [flow entry]
  (update-in flow [:staged] conj entry))

(defn unstage [flow entry]
  (update-in flow [:staged] disj entry))

(defn cancel-flow [flow]
  (let [git (:git flow)]
    (-> (.reset git)
        (.setMode ResetCommand$ResetType/HARD)
        .call)
    (when (:stash-rev flow)
      (-> (.stashApply git) .call))))

(defn push-flow [flow to-commit]
  (let [git (:git flow)
        simple-status (git/simple-status git)]
    (when (seq to-commit)
      (doseq [f to-commit]
        (if (= :deleted (get simple-status f))
          (-> git (.rm) (.addFilepattern f) (.call))
          (-> git (.add) (.addFilepattern f) (.call))))
      (-> git (.commit) (.setMessage (:message flow)) (.call)))
    (try
      (let [pull-res (-> git (.pull) (.call))]
        (if (.isSuccessful pull-res)
          (assoc flow :status :success)
          (assoc flow :status :merging)))
      (catch CheckoutConflictException e
        (cancel-flow flow)
        (assoc flow
               :status :checkout-conflict
               :checkout-conficts (.getConflictingPaths e))))
    ; TODO: push here?
    #_(-> git (.push) (.call))))

(defn commit-merge [flow]
  (let [git (:git flow)]
    (-> git (.commit) (.call))))

(defn resolve-file [flow file resolution]
  (let [git (:git flow)
        status (git/status git)
        state (:conflicting-stage-state status)]
    (let [action (condp = [(get state file) resolution]
                   [:both-added :ours] :checkout
                   [:both-added :theirs] :checkout

                   [:both-modified :theirs] :checkout
                   [:both-modified :ours] :checkout

                   [:deleted-by-them :ours] :checkout
                   [:deleted-by-them :theirs] :rm

                   [:deleted-by-us :ours] :rm
                   [:deleted-by-us :theirs] :checkout)]
      (if (= action :rm)
        (-> (.rm git)
            (.addFilepattern file)
            (.call))
        (do
          (-> (.checkout git)
              (.addPath file)
              (.setStage (CheckoutCommand$Stage/valueOf (string/upper-case (name resolution))))
              (.call))
          (-> (.add git)
              (.addFilepattern file)
              (.call)))))))

(defn advance [flow]
  (let [git (:git flow)]
    (case (:state flow)
      :start (assoc flow
                    :state :commit
                    :action :commit
                    :value (git/simple-status git))
      :commit (do (throw (Exception. "foo")) flow )
      :done flow
      :checkout-conflict (assoc flow
                                :action :show-message
                                :value (str "Can't synchronize. Conflict with unselected files %s" (string/join "," (:checkout-conficts flow))))
      :merging 0
      :rejected 0)))

(defn- make-cell [text]
  (let [pane (BorderPane.)
        menu (MenuButton. "Action...")
        label (Label. text)]
    (BorderPane/setAlignment label Pos/CENTER)
    (BorderPane/setAlignment menu Pos/CENTER)
    (.setLeft pane label)
    (.setRight pane menu)
    (.setPrefHeight pane -1)
    pane))

(defn- refresh-stage [flow-atom controls]
  (prn "refresh" @flow-atom)
  (ui/items! (:changed controls) (set/difference (set (:unified-status @flow-atom)) (:staged @flow-atom)))
  (ui/items! (:staged controls) (:staged @flow-atom)))

(defn advance-dialog [flow-atom controls]
  (let [flow (swap! flow-atom advance)]
    (prn "advance" flow)
    (case (:state flow)
      :commit (refresh-stage flow-atom controls))))

(defn- show-diff [git status]
  (let [old-name (or (:old-path status) (:new-path status) )
        new-name (or (:new-path status) (:old-path status) )
        work-tree (.getWorkTree (.getRepository git))
        old (String. ^bytes (git/show-file git old-name))
        new (slurp (io/file work-tree new-name))]
    (diff-view/make-diff-viewer old-name old new-name new)))

(def short-status {:add "A" :modify "M" :delete "D" :rename "R"})

(defn- status-render [status]
  {:text (format "[%s]%s" ((:change-type status) short-status) (or (:new-path status) (:old-path status)))})

(defn open-sync-dialog [flow]
  (let [dialog (dialogs/make-task-dialog "sync.fxml"
                                         {:title "Synchronize Project"
                                          :ok-label "Sync"})
        ^Parent root (dialogs/dialog-root dialog)
        controls (ui/collect-controls root ["conflicting" "changed" "staged"])
        flow-atom (atom flow)
        ad (fn [] (try
                    (advance-dialog flow-atom controls)
                    (catch Throwable e
                      (dialogs/error! dialog (.getMessage e)))))]
    (ui/cell-factory! (:changed controls) (fn [e] (status-render e)))
    (ui/cell-factory! (:staged controls) (fn [e] (status-render e)))
    (ui/cell-factory! (:conflicting controls) (fn [p] {:text (str p)
                                                       :content-display ContentDisplay/GRAPHIC_ONLY
                                                       :gfx (make-cell (str p))}))

    (ui/on-double! (:changed controls) (fn [evt]
                                         (when-let [e (first (ui/selection (:changed controls)))]
                                           (swap! flow-atom stage e)
                                           (refresh-stage flow-atom controls))))

    (ui/on-double! (:staged controls) (fn [evt]
                                        (when-let [e (first (ui/selection (:staged controls)))]
                                          (swap! flow-atom unstage e)
                                          (prn @flow-atom)
                                          (refresh-stage flow-atom controls))))

    (ui/on-key-pressed! (:changed controls) (fn [e]
                                              (when (= (.getCode ^KeyEvent e) KeyCode/ENTER)
                                                (when-let [status (first (ui/selection (:changed controls)))]
                                                  (show-diff (:git flow) status)))))

    (refresh-stage flow-atom controls)
    (ad)
    (dialogs/show! dialog {:on-ok ad
                           :ready? (fn [] true)})))
