(ns editor.sync
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [editor
             [dialogs :as dialogs]
             [diff-view :as diff-view]
             [git :as git]
             [handler :as handler]
             [login :as login]
             [ui :as ui]]
            [editor.progress :as progress])
  (:import [org.eclipse.jgit.api Git PullResult]
           [org.eclipse.jgit.revwalk RevCommit]
           [org.eclipse.jgit.api.errors StashApplyFailureException]
           [javafx.scene Parent Scene]
           [javafx.scene.control Button SelectionMode ListView TextArea]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.stage Modality]))

(set! *warn-on-reflection* true)

(def ^:dynamic *login* true)

;; =================================================================================

;; Flow state-diagram

;; 1. Pull
;;
;;    :start -> :pulling -> ---------> :done
;;                    \          /
;;                     -> :resolve
;;                               \
;;                                -> :cancel

;; 2. Push
;;
;;    <PULL-FLOW> -> :start -> :staging -> :committing -> :pushing -> :done
;;         \                                                /  \
;;          <-----------------------------------------------    -> :cancel


(defn- serialize-ref [^RevCommit ref]
  (some->> ref .getName))

(defn- deserialize-ref [revision ^Git git]
  (some->> revision (git/get-commit (.getRepository git))))

(defn- serialize-flow [flow]
  (-> flow
      (dissoc :git)
      (dissoc :creds)
      (update :start-ref serialize-ref)
      (update :stash-ref serialize-ref)))

(defn- deserialize-flow [serialized-flow ^Git git creds]
  (-> serialized-flow
      (assoc :git git)
      (assoc :creds creds)
      (update :start-ref deserialize-ref git)
      (update :stash-ref deserialize-ref git)))

(defn- make-flow [^Git git creds start-ref stash-ref]
  {:state     :pull/start
   :git       git
   :creds     creds
   :start-ref start-ref
   :stash-ref stash-ref
   :progress  (progress/make "pull" 4)
   :conflicts {}
   :resolved  {}
   :staged    #{}
   :modified  #{}})

(defn- should-update-journal? [old-flow new-flow]
  (let [simple-keys [:state :start-ref :stash-ref :conflicts :resolved :staged :modified]]
    (or (not= (select-keys old-flow simple-keys)
              (select-keys new-flow simple-keys))
        (let [old-progress (:progress old-flow)
              new-progress (:progress new-flow)]
          (or (not= (:message old-progress) (:message new-progress))
              (and (= (:pos new-progress) (:size new-progress))
                   (not= (:pos old-progress) (:size old-progress))))))))

(defn- flow-journal-file ^java.io.File [^Git git]
  (some-> git .getRepository .getWorkTree (io/file ".internal/.sync-in-progress")))

(defn- write-flow-journal! [{:keys [git] :as flow}]
  (let [file (flow-journal-file git)
        data (serialize-flow flow)]
    (io/make-parents file)
    (spit file (pr-str data))))

(defn- on-flow-changed [_ _ old-flow new-flow]
  (when (should-update-journal? old-flow new-flow)
    (write-flow-journal! new-flow)))

(defn flow-in-progress? [^Git git]
  (if-let [file (flow-journal-file git)]
    (.exists file)
    false))

(defn begin-flow! [^Git git prefs]
  (when *login*
    (login/login prefs))
  (let [creds (git/credentials prefs)
        start-ref (git/get-current-commit-ref git)
        stash-ref (git/stash git)
        flow  (make-flow git creds start-ref stash-ref)
        !flow (atom flow)]
    (write-flow-journal! flow)
    (add-watch !flow ::on-flow-changed on-flow-changed)
    !flow))

(defn resume-flow [^Git git prefs]
  (when *login*
    (login/login prefs))
  (let [creds (git/credentials prefs)
        file  (flow-journal-file git)
        data  (with-open [reader (java.io.PushbackReader. (io/reader file))]
                (edn/read reader))
        flow  (deserialize-flow data git creds)
        !flow (atom flow)]
    (add-watch !flow ::on-flow-changed on-flow-changed)
    !flow))

(defn cancel-flow! [!flow]
  (remove-watch !flow ::on-flow-changed)
  (let [{:keys [git start-ref stash-ref]} @!flow
        file (flow-journal-file git)]
    (when (.exists file)
      (io/delete-file file :silently))
    (git/hard-reset git start-ref)
    (when stash-ref
      (git/stash-apply git stash-ref)
      (git/stash-drop git stash-ref))))

(defn finish-flow! [!flow]
  (remove-watch !flow ::on-flow-changed)
  (let [{:keys [git stash-ref]} @!flow
        file (flow-journal-file git)]
    (when (.exists file)
      (io/delete-file file :silently))
    (when stash-ref
      (git/stash-drop git stash-ref))))

(defn- tick
  ([flow new-state]
   (tick flow new-state 1))
  ([flow new-state n]
   (-> flow
       (assoc :state new-state)
       (update :progress #(progress/advance % n)))))

(defn- find-changes [status unified-status status-keys]
  (into #{}
        (comp (filter (fn [change]
                          (some #(contains? (status %)
                                            (git/change-path change))
                                status-keys)))
              (map #(dissoc % :score)))
        unified-status))

(defn find-git-state [status unified-status]
  {:staged   (find-changes status unified-status [:added :changed :removed])
   :modified (find-changes status unified-status [:missing :modified :untracked])})

(defn refresh-git-state [{:keys [git] :as flow}]
  (merge flow (find-git-state (git/status git)
                              (git/unified-status git))))

(defn advance-flow [{:keys [git state progress creds conflicts stash-ref message] :as flow} render-progress]
  (render-progress progress)
  (condp = state
    :pull/start     (advance-flow (tick flow :pull/pulling) render-progress)
    :pull/pulling   (let [^PullResult pull-res (try (git/pull git creds)
                                                    (catch Exception e
                                                      (println e)))]
                      (if (and pull-res (.isSuccessful pull-res))
                        (advance-flow (tick flow :pull/applying) render-progress)
                        (advance-flow (tick flow :pull/error) render-progress)))
    :pull/applying  (let [stash-res (when stash-ref
                                      (try (git/stash-apply git stash-ref)
                                           (catch StashApplyFailureException e
                                             :conflict)
                                           (catch Exception e
                                             (println e))))
                          status    (git/status git)]
                      (cond
                        (nil? stash-ref)        (advance-flow (tick flow :pull/done 2) render-progress)
                        (= :conflict stash-res) (advance-flow (-> flow
                                                                  (tick :pull/conflicts)
                                                                  (assoc :conflicts (:conflicting-stage-state status)))
                                                              render-progress)
                        stash-res               (advance-flow (tick flow :pull/done 2) render-progress)
                        :else                   (advance-flow (tick flow :pull/error) render-progress)))
    :pull/conflicts (if (empty? conflicts)
                      (advance-flow (tick flow :pull/done) render-progress)
                      flow)
    :pull/done      flow
    :pull/error     flow

    :push/start     (advance-flow (tick (refresh-git-state flow) :push/staging) render-progress)
    :push/staging   flow
    :push/committing (do
                       (git/commit git message)
                       (advance-flow (tick flow :push/pushing) render-progress))
    :push/pushing   (do
                      (try
                        (git/push git creds)
                        (advance-flow (tick flow :push/done) render-progress)
                        (catch Exception e
                          (println e)
                          (advance-flow (tick flow :pull/start) render-progress))))
    :push/done      flow))

(ui/extend-menu ::conflicts-menu nil
                [{:label "Show Diff"
                  :command :show-diff}
                 {:label "Use Ours"
                  :command :use-ours}
                 {:label "Use Theirs"
                  :command :use-theirs}])

(ui/extend-menu ::staging-menu nil
                [{:label "Show Diff"
                  :command :show-change-diff}
                 {:label "Stage Change"
                  :command :stage-change}])

(ui/extend-menu ::unstaging-menu nil
                [{:label "Show Diff"
                  :command :show-change-diff}
                 {:label "Unstage Change"
                  :command :unstage-change}])

(defn get-theirs [{:keys [git] :as flow} file]
  (String. ^bytes (git/show-file git file)))

(defn get-ours [{:keys [git ^RevCommit stash-ref] :as flow} file]
  (when stash-ref
    (String. ^bytes (git/show-file git file (.name stash-ref)))))

(defn resolve-file! [!flow file]
  (let [{:keys [^Git git conflicts]} @!flow]
    (when-let [entry (get conflicts file)]
      (if (.exists (git/file git file))
        (-> git .add (.addFilepattern file) .call)
        (-> git .rm (.addFilepattern file) .call))
      (-> git .reset (.addPath file) .call)
      (swap! !flow #(-> %
                        (update :conflicts dissoc file)
                        (update :resolved assoc file entry))))))

(handler/defhandler :show-diff :sync
  (enabled? [selection] (= 1 (count selection)))
  (run [selection !flow]
    (let [file   (first selection)
          ours   (get-ours @!flow file)
          theirs (get-theirs @!flow file)]
      (when (and ours theirs)
        (diff-view/make-diff-viewer (str "Theirs '" file "'") theirs
                                    (str "Ours '" file "'") ours)))))

(handler/defhandler :show-change-diff :sync
  (enabled? [selection] (git/selection-diffable? selection))
  (run [selection !flow]
       (let [{:keys [new new-path old old-path]} (git/selection-diff-data (:git @!flow) selection)]
         (diff-view/make-diff-viewer old-path old new-path new))))

(handler/defhandler :use-ours :sync
  (enabled? [selection] (pos? (count selection)))
  (run [selection !flow]
    (doseq [f selection]
      (when-let [ours (get-ours @!flow f)]
        (spit (io/file (git/worktree (:git @!flow)) f) ours)
        (resolve-file! !flow f)))))

(handler/defhandler :use-theirs :sync
  (enabled? [selection] (pos? (count selection)))
  (run [selection !flow]
    (doseq [f selection]
      (when-let [theirs (get-theirs @!flow f)]
        (spit (io/file (git/worktree (:git @!flow)) f) theirs)
        (resolve-file! !flow f)))))

(handler/defhandler :stage-change :sync
  (enabled? [selection] (pos? (count selection)))
  (run [selection !flow]
    (doseq [change selection]
      (git/stage-change! (:git @!flow) change))
    (swap! !flow refresh-git-state)))

(handler/defhandler :unstage-change :sync
  (enabled? [selection] (pos? (count selection)))
  (run [selection !flow]
    (doseq [change selection]
      (git/unstage-change! (:git @!flow) change))
    (swap! !flow refresh-git-state)))

;; =================================================================================

(defn open-sync-dialog [!flow]
  (let [root            ^Parent (ui/load-fxml "sync-dialog.fxml")
        pull-root       ^Parent (ui/load-fxml "sync-pull.fxml")
        push-root       ^Parent (ui/load-fxml "sync-push.fxml")
        stage           (ui/make-stage)
        scene           (Scene. root)
        dialog-controls (ui/collect-controls root [ "ok" "push" "cancel" "dialog-area" "progress-bar"])
        pull-controls   (ui/collect-controls pull-root ["conflicting" "resolved" "conflict-box" "main-label"])
        push-controls   (ui/collect-controls push-root ["changed" "staged" "message" "content-box" "main-label" "diff" "stage" "unstage"])
        render-progress (fn [progress]
                          (ui/run-later
                            (ui/update-progress-controls! progress (:progress-bar dialog-controls) nil)))
        update-push-buttons! (fn []
                               ;; The stage, unstage and diff buttons are enabled
                               ;; if the changed or staged views have input focus
                               ;; and something selected. The diff button is
                               ;; disabled if more than one item is selected.
                               (let [changed-view      (:changed push-controls)
                                     changed-selection (ui/selection changed-view)
                                     staged-view       (:staged push-controls)
                                     staged-selection  (ui/selection staged-view)
                                     enabled           (cond
                                                         (and (ui/focus? changed-view)
                                                              (seq changed-selection))
                                                         (if (git/selection-diffable? changed-selection)
                                                           #{:stage :diff}
                                                           #{:stage})

                                                         (and (ui/focus? staged-view)
                                                              (seq staged-selection))
                                                         (if (git/selection-diffable? staged-selection)
                                                           #{:unstage :diff}
                                                           #{:unstage})

                                                         :else
                                                         #{})]
                                 (ui/disable! (:diff push-controls) (not (:diff enabled)))
                                 (ui/disable! (:stage push-controls) (not (:stage enabled)))
                                 (ui/disable! (:unstage push-controls) (not (:unstage enabled)))

                                 (when (:diff enabled)
                                   (if-let [focused-list-view (cond (ui/focus? changed-view) changed-view
                                                                    (ui/focus? staged-view) staged-view
                                                                    :else nil)]
                                     (ui/context! (:diff push-controls) :sync {:!flow !flow} (ui/->selection-provider focused-list-view))))))
        update-controls (fn [{:keys [state conflicts resolved modified staged] :as flow}]
                          (ui/run-later
                           (if (= "pull" (namespace state))
                             (do
                               (ui/title! stage "Get Remote Changes")
                               (ui/text! (:ok dialog-controls) "Pull")
                               (ui/children! (:dialog-area dialog-controls) [pull-root])
                               (ui/fill-control pull-root)
                               (.sizeToScene (.getWindow scene)))
                             (do
                               (ui/title! stage "Push Local Changes")
                               (ui/visible! (:push dialog-controls) false)
                               (ui/text! (:ok dialog-controls) "Push")
                               (ui/children! (:dialog-area dialog-controls) [push-root])
                               (ui/fill-control push-root)
                               (.sizeToScene (.getWindow scene))))
                           (condp = state
                             :pull/conflicts (do
                                               (ui/text! (:main-label pull-controls) "Resolve Conflicts")
                                               (ui/visible! (:conflict-box pull-controls) true)
                                               (ui/items! (:conflicting pull-controls) (sort (keys conflicts)))
                                               (ui/items! (:resolved pull-controls) (sort (keys resolved)))

                                               (let [button (:ok dialog-controls)]
                                                 (ui/text! button "Apply")
                                                 (ui/disable! button (not (empty? conflicts)))))
                             :pull/done      (do
                                               (ui/text! (:main-label pull-controls) "Done!")
                                               (ui/visible! (:push dialog-controls) true)
                                               (ui/visible! (:conflict-box pull-controls) false)
                                               (ui/text! (:ok dialog-controls) "Done"))
                             :pull/error      (do
                                                (ui/text! (:main-label pull-controls) "Error getting changes")
                                                (ui/visible! (:push dialog-controls) false)
                                                (ui/visible! (:conflict-box pull-controls) false)
                                                (ui/text! (:ok dialog-controls) "Close"))
                             :push/staging   (let [changed-view ^ListView (:changed push-controls)
                                                   staged-view ^ListView (:staged push-controls)
                                                   changed-selection (vec (ui/selection changed-view))
                                                   staged-selection (vec (ui/selection staged-view))]
                                               (ui/items! changed-view (sort-by git/change-path modified))
                                               (ui/items! staged-view (sort-by git/change-path staged))
                                               (ui/disable! (:ok dialog-controls)
                                                            (or (empty? staged)
                                                                (empty? (ui/text (:message push-controls)))))

                                               ;; The stage, unstage and diff buttons start off disabled, but
                                               ;; might be enabled by the event handler triggered by select!
                                               (ui/disable! (:diff push-controls) true)
                                               (ui/disable! (:stage push-controls) true)
                                               (ui/disable! (:unstage push-controls) true)
                                               (doseq [item changed-selection]
                                                 (ui/select! changed-view item))
                                               (doseq [item staged-selection]
                                                 (ui/select! staged-view item)))
                             :push/done    (do
                                             (ui/text! (:main-label push-controls) "Done!")
                                             (ui/visible! (:content-box push-controls) false)
                                             (ui/text! (:ok dialog-controls) "Done"))

                             nil)))]
    (dialogs/observe-focus stage)
    (.initOwner stage (ui/main-stage))
    (update-controls @!flow)
    (add-watch !flow :updater (fn [_ _ _ flow]
                                (update-controls flow)))

    (ui/on-action! (:cancel dialog-controls) (fn [_]
                                               (cancel-flow! !flow)
                                               (.close stage)))
    (ui/on-action! (:ok dialog-controls) (fn [_]
                                           (cond
                                             (= "done" (name (:state @!flow)))
                                             (do
                                               (finish-flow! !flow)
                                               (.close stage))

                                             (= :push/staging (:state @!flow))
                                             (swap! !flow #(advance-flow
                                                            (merge %
                                                                   {:state   :push/committing
                                                                    :message (ui/text (:message push-controls))})
                                                            render-progress))

                                             :else
                                             (swap! !flow advance-flow render-progress))))
    (ui/on-action! (:push dialog-controls) (fn [_]
                                             (swap! !flow #(merge %
                                                                  {:state    :push/start
                                                                   :progress (progress/make "push" 4)}))
                                             (swap! !flow advance-flow render-progress)))

    (ui/bind-action! (:diff push-controls) :show-change-diff)

    (ui/observe (.focusOwnerProperty scene)
                (fn [_ _ new]
                  (when-not (instance? Button new)
                    (update-push-buttons!))))

    (ui/observe-selection ^ListView (:changed push-controls)
                          (fn [_ _]
                            (update-push-buttons!)))

    (ui/observe-selection ^ListView (:staged push-controls)
                          (fn [_ _]
                            (update-push-buttons!)))

    (ui/observe (.textProperty ^TextArea (:message push-controls))
                (fn [_ _ _]
                  (update-controls @!flow)))

    (let [^ListView list-view (:conflicting pull-controls)]
      (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
      (ui/context! list-view :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/register-context-menu list-view ::conflicts-menu)
      (ui/cell-factory! list-view (fn [e] {:text e})))
    (ui/cell-factory! (:resolved pull-controls) (fn [e] {:text e}))

    (let [^ListView list-view (:changed push-controls)]
      (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
      (ui/context! list-view :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/context! (:stage push-controls) :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/bind-action! (:stage push-controls) :stage-change)
      (ui/register-context-menu list-view ::staging-menu)
      (ui/cell-factory! list-view (fn [e] {:text (git/change-path e)})))

    (let [^ListView list-view (:staged push-controls)]
      (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
      (ui/context! list-view :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/context! (:unstage push-controls) :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/bind-action! (:unstage push-controls) :unstage-change)
      (ui/register-context-menu list-view ::unstaging-menu)
      (ui/cell-factory! list-view (fn [e] {:text (git/change-path e)})))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (= code KeyCode/ESCAPE) true
                                               (cancel-flow! !flow)
                                               (.close stage)))))

    (.initModality stage Modality/APPLICATION_MODAL)
    (.setScene stage scene)

    (try
      (ui/show-and-wait-throwing! stage)
      (catch Exception e
        (cancel-flow! !flow)
        (throw e)))))
