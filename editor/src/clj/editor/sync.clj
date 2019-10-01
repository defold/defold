(ns editor.sync
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [editor.dialogs :as dialogs]
            [editor.diff-view :as diff-view]
            [editor.fs :as fs]
            [editor.fxui :as fxui]
            [editor.git :as git]
            [editor.git-credentials :as git-credentials]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.vcs-status :as vcs-status]
            [editor.progress :as progress]
            [service.log :as log])
  (:import [org.eclipse.jgit.api Git PullResult]
           [org.eclipse.jgit.api.errors StashApplyFailureException TransportException]
           [org.eclipse.jgit.errors MissingObjectException]
           [org.eclipse.jgit.revwalk RevCommit]
           [java.net URI]
           [javafx.scene Parent Scene]
           [javafx.scene.control Button ListView SelectionMode TextArea TextInputControl]
           [javafx.scene.input KeyCode KeyEvent]))

(set! *warn-on-reflection* true)

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

(defn- serialize-stash-info [stash-info]
  (some-> stash-info (update :ref serialize-ref)))

(defn- deserialize-stash-info [stash-info ^Git git]
  (some-> stash-info (update :ref deserialize-ref git)))

(defn- serialize-flow [flow]
  (-> flow
      (dissoc :git)
      (update :start-ref serialize-ref)
      (update :stash-info serialize-stash-info)))

(defn- deserialize-flow [serialized-flow ^Git git]
  (assert (instance? Git git))
  (-> serialized-flow
      (assoc :git git)
      (update :start-ref deserialize-ref git)
      (update :stash-info deserialize-stash-info git)))

(defn- make-flow [^Git git creds start-ref stash-info]
  (assert (instance? Git git))
  (assert (or (nil? creds) (git-credentials/encrypted-credentials? creds)))
  {:state      :pull/start
   :git        git
   :creds      creds
   :start-ref  start-ref
   :stash-info stash-info
   :progress   (progress/make "pull" 4)
   :conflicts  {}
   :resolved   {}
   :staged     #{}
   :modified   #{}})

(defn- should-update-journal? [old-flow new-flow]
  (when (or (:git old-flow) (:git new-flow))
    (let [simple-keys [:state :start-ref :stash-info :conflicts :resolved :staged :modified]]
      (or (not= (select-keys old-flow simple-keys)
                (select-keys new-flow simple-keys))
          (let [old-progress (:progress old-flow)
                new-progress (:progress new-flow)]
            (or (not= (:message old-progress) (:message new-progress))
                (and (= (:pos new-progress) (:size new-progress))
                     (not= (:pos old-progress) (:size old-progress)))))))))

(defn flow-journal-file ^java.io.File [^Git git]
  (some-> git .getRepository .getWorkTree (io/file ".internal/.sync-in-progress")))

(defn- write-flow-journal! [{:keys [git] :as flow}]
  (let [file (flow-journal-file git)
        data (serialize-flow flow)]
    (fs/create-file! file (pr-str data))))

(defn- on-flow-changed [_ _ old-flow new-flow]
  (when (should-update-journal? old-flow new-flow)
    (write-flow-journal! new-flow)))

(defn flow-in-progress? [^Git git]
  (if-let [file (flow-journal-file git)]
    (.exists file)
    false))

(defn read-journal [file]
  (with-open [reader (java.io.PushbackReader. (io/reader file))]
    (edn/read reader)))

(defn- try-load-flow [^Git git file]
  (let [data (try
               (read-journal file)
               (catch Throwable e
                 e))]
    (if (instance? Throwable data)
      ;; We failed to read or parse the data from the journal file. Possibly a
      ;; permissions issue, or it could be corrupt. We should notify the user,
      ;; but we cannot retry.
      {:type :error :code :read-error :exception data :can-retry? false}

      ;; The flow journal file parsed OK, but it might contain invalid refs.
      (let [flow (try
                   (deserialize-flow data git)
                   (catch Throwable e
                     e))]
        (cond
          (map? flow)
          ;; We got a map back. Verify it contains the required data.
          (if (and (instance? RevCommit (:start-ref flow))
                   (instance? RevCommit (:ref (:stash-info flow))))
            ;; The journal file looks good! We can proceed with reverting to the
            ;; pre-sync state. If that fails, we can retry.
            {:type :success :flow flow}

            ;; The journal file appears malformed. We cannot retry.
            {:type :error :code :invalid-data-error :data flow :can-retry? false})

          (instance? MissingObjectException flow)
          ;; One of the refs from the journal file are invalid. Presumably the
          ;; stash was deleted. We should notify the user, but we cannot retry.
          {:type :error :code :invalid-ref-error :exception flow :can-retry? false}

          (instance? Throwable flow)
          ;; We somehow failed to deserialize the data we read from the journal
          ;; file. We should notify the user, but we cannot retry.
          {:type :error :code :deserialize-error :exception flow :can-retry? false}

          :else
          ;; Programming error - should not happen.
          (throw (ex-info (str "Unhandled return value from deserialize-flow: " (pr-str flow))
                          {:data data
                           :return-value flow})))))))

(defn- try-revert-to-stashed! [^Git git start-ref stash-info]
  (or (when-some [locked-files (not-empty (git/locked-files git))]
        {:type :error :code :locked-files-error :locked-files locked-files :can-retry? true})
      (try
        (git/revert-to-revision! git start-ref)
        nil
        (catch Throwable e
          {:type :error :code :revert-to-start-ref-error :exception e :can-retry? true}))
      (when stash-info
        (try
          (git/stash-apply! git stash-info)
          nil
          (catch Throwable e
            {:type :error :code :stash-apply-error :exception e :can-retry? true})))
      (when stash-info
        (try
          (git/stash-drop! git stash-info)
          nil
          (catch Throwable e
            {:type :warning :code :stash-drop-error :exception e :can-retry? false})))
      {:type :success}))

(defn cancel-flow-in-progress! [^Git git]
  (let [file (flow-journal-file git)
        load-result (try-load-flow git file)]
    ;; Begin by deleting the flow journal file, since the revert could
    ;; potentially delete it if it is not .gitignored. In case something
    ;; goes wrong we will re-write it unless we think it is invalid.
    (fs/delete-file! file {:fail :silently})

    (if (not= :success (:type load-result))
      ;; There was an error loading the flow journal file. We cannot retry,
      ;; so we return the error info without restoring the journal file.
      load-result

      ;; The journal file looks good! Proceed with reverting to the
      ;; pre-sync state. In case something goes wrong, we restore the flow
      ;; journal file so we can retry the operation.
      (let [{:keys [start-ref stash-info] :as flow} (:flow load-result)
            revert-result (try-revert-to-stashed! git start-ref stash-info)]
        (when (and (not= :success (:type revert-result))
                   (:can-retry? revert-result))
          ;; Something went wrong. Re-write the journal file so we can retry.
          (write-flow-journal! flow))
        revert-result))))

(defn cancel-flow! [!flow]
  (remove-watch !flow ::on-flow-changed)
  (let [flow @!flow
        state (:state flow)]
    (case (namespace state)
      ("pull" "push")
      (let [{:keys [git start-ref stash-info]} flow
            file (flow-journal-file git)
            file-existed? (.exists file)]
        ;; Always delete the flow journal file, since the revert could potentially
        ;; delete it if it is not .gitignored. In case something goes wrong we will
        ;; re-write it unless we think it is invalid.
        (when file-existed?
          (fs/delete-file! file {:fail :silently}))

        ;; Proceed with reverting to the pre-sync state. In case something goes
        ;; wrong, we restore the flow journal file so we can retry the operation.
        (let [revert-result (try-revert-to-stashed! git start-ref stash-info)]
          (when (and file-existed?
                  (not= :success (:type revert-result))
                  (:can-retry? revert-result))
            ;; Something went wrong. Re-write the journal file so we can retry.
            (write-flow-journal! flow))
          revert-result)))))

(defn- cancel-result-message [cancel-result]
  (if (= :success (:type cancel-result))
    "Successfully restored the project to the pre-sync state."
    (case (:code cancel-result)
      :deserialize-error "The sync journal file is corrupt. Unable to restore the project to the pre-sync state. A Git stash with your changes might exist, should you want to attempt to restore it manually."
      :invalid-data-error "The sync journal file is malformed. Unable to restore the project to the pre-sync state. A Git stash with your changes might exist, should you want to attempt to restore it manually."
      :invalid-ref-error "The sync journal file references invalid Git objects. Unable to restore the project to the pre-sync state. A Git stash with your changes might exist, should you want to attempt to restore it manually."
      :locked-files-error (git/locked-files-error-message (:locked-files cancel-result))
      :read-error "Failed to read the sync journal file. Unable to restore the project to the pre-sync state. A Git stash with your changes might exist, should you want to attempt to restore it manually."
      :revert-to-start-ref-error "Failed to revert the project to the commit your changes were made on. Unable to restore the project to the pre-sync state. A Git stash with your changes might exist, should you want to attempt to restore it manually."
      :stash-apply-error "Failed to apply your stashed changes on top of the base commit. Unable to restore the project to the pre-sync state. A Git stash with your changes might exist, should you want to attempt to restore it manually."
      :stash-drop-error "Successfully restored the project to the pre-sync state, but was unable to drop the Git stash with your pre-sync changes. You might want to clean it up manually."

      (let [{:keys [code exception]} cancel-result]
        (if (instance? Throwable exception)
          (let [message (.getMessage ^Throwable exception)]
            (if (keyword? code)
              (str "Unknown error " code ". " message)
              (str "Unknown error. " message)))
          (str "Malformed sync cancellation result " (pr-str cancel-result)))))))

(defn interactive-cancel! [cancel-fn]
  (loop []
    (let [result (cancel-fn)
          dialog-props {:title "Unable to Cancel Sync"
                        :icon :icon/triangle-error
                        :header "An error occurred"
                        :content {:fx/type fxui/label
                                  :style-class "dialog-content-padding"
                                  :text (cancel-result-message result)}}]
      (when (not= :success (:type result))
        (if (:can-retry? result)
          (when (dialogs/make-info-dialog
                  (assoc dialog-props :buttons [{:text "Lose Changes"
                                                 :cancel-button true
                                                 :result false}
                                                {:text "Retry"
                                                 :default-button true
                                                 :result true}]))
            (recur))
          (dialogs/make-info-dialog dialog-props))))))

(defn begin-flow! [^Git git prefs]
  (let [start-ref (git/get-current-commit-ref git)
        stash-info (git/stash! git)
        creds (git-credentials/read-encrypted-credentials prefs git)
        flow (make-flow git creds start-ref stash-info)
        !flow (atom flow)]
    (try
      (write-flow-journal! flow)
      (add-watch !flow ::on-flow-changed on-flow-changed)
      !flow
      (catch Exception e
        (cancel-flow! !flow)
        (throw e)))))

(defn resume-flow [^Git git]
  (let [file (flow-journal-file git)
        data (with-open [reader (java.io.PushbackReader. (io/reader file))]
               (edn/read reader))
        flow (deserialize-flow data git)
        !flow (atom flow)]
    (add-watch !flow ::on-flow-changed on-flow-changed)
    !flow))

(defn finish-flow! [!flow]
  (remove-watch !flow ::on-flow-changed)
  (let [{:keys [git stash-info]} @!flow
        file (flow-journal-file git)]
    (fs/delete-file! file {:fail :silently})
    (when stash-info
      (git/stash-drop! git stash-info))))

(defn- tick
  ([flow new-state]
   (tick flow new-state 1))
  ([flow new-state n]
   (-> flow
       (assoc :state new-state)
       (update :progress #(progress/advance % n)))))

(defn find-git-state [{:keys [added changed removed]} unified-status]
  (reduce (fn [result {:keys [change-type old-path new-path] :as change}]
            (case change-type
              :add    (if (contains? added new-path)
                        (update result :staged conj (dissoc change :score))
                        (update result :modified conj (dissoc change :score)))
              :delete (if (contains? removed old-path)
                        (update result :staged conj (dissoc change :score))
                        (update result :modified conj (dissoc change :score)))
              :modify (if (contains? changed new-path)
                        (update result :staged conj (dissoc change :score))
                        (update result :modified conj (dissoc change :score)))
              :rename (let [add-staged (contains? added new-path)
                            delete-staged (contains? removed old-path)]
                        (cond
                          (and add-staged delete-staged)
                          (update result :staged conj (dissoc change :score))

                          add-staged
                          (-> result
                              (update :staged conj (git/make-add-change new-path))
                              (update :modified conj (git/make-delete-change old-path)))

                          delete-staged
                          (-> result
                              (update :staged conj (git/make-delete-change old-path))
                              (update :modified conj (git/make-add-change new-path)))

                          :else
                          (update result :modified conj (dissoc change :score))))))
          {:modified #{}
           :staged #{}}
          unified-status))

(defn refresh-git-state [{:keys [git] :as flow}]
  (let [status (git/status git)]
    (merge flow (find-git-state status
                                (git/unified-status git status)))))

(defn- matching-transport-exception? [re-pattern exception]
  (and (instance? TransportException exception)
       (some? (re-find re-pattern (ex-message exception)))))

(def ^:private https-not-authorized-exception?
  (partial matching-transport-exception? #"(?i)\bnot authorized\b"))

(defn advance-flow [{:keys [git state progress creds conflicts stash-info message] :as flow} render-progress]
  (render-progress progress)
  (condp = state
    :pull/start (if (nil? (:error flow))
                  (advance-flow (tick flow :pull/pulling) render-progress)
                  flow)
    :pull/pulling (let [dry-run-push-error
                        (when (:verify-creds flow)
                          ;; Pulling from a public repository will not use the
                          ;; credentials. Do a dry-run push here to check if our
                          ;; credentials are valid for the push state.
                          (try
                            (git/push! git {:encrypted-credentials creds :dry-run true})
                            nil
                            (catch Exception error
                              error)))]
                    (if (https-not-authorized-exception? dry-run-push-error)
                      (advance-flow (-> flow
                                        (tick :pull/start)
                                        (assoc :error :https-not-authorized))
                                    render-progress)
                      (let [pull-result-or-error
                            (try
                              (git/pull! git {:encrypted-credentials creds})
                              (catch Exception error
                                error))]
                        (cond
                          (and (instance? PullResult pull-result-or-error)
                               (.isSuccessful ^PullResult pull-result-or-error))
                          (advance-flow (tick flow :pull/applying) render-progress)

                          (https-not-authorized-exception? pull-result-or-error)
                          (advance-flow (-> flow
                                            (tick :pull/start)
                                            (assoc :error :https-not-authorized))
                                        render-progress)

                          :else
                          (do
                            (log/error :exception pull-result-or-error
                                       :msg (format "Error pulling during sync: %s"
                                                    (ex-message pull-result-or-error)))
                            (advance-flow (tick flow :pull/error) render-progress))))))
    :pull/applying (let [stash-res (when stash-info
                                     (try (git/stash-apply! git stash-info)
                                          (catch StashApplyFailureException _
                                            :conflict)
                                          (catch Exception e
                                            (println e))))
                         status    (git/status git)]
                     (cond
                       (nil? stash-info)       (advance-flow (tick flow :pull/done 2) render-progress)
                       (= :conflict stash-res) (advance-flow (-> flow
                                                                 (tick :pull/conflicts)
                                                                 (assoc :conflicts (:conflicting-stage-state status)))
                                                             render-progress)
                       stash-res               (advance-flow (tick flow :pull/done 2) render-progress)
                       :else                   (advance-flow (tick flow :pull/error) render-progress)))
    :pull/conflicts (if (empty? conflicts)
                      (advance-flow (tick flow :pull/done) render-progress)
                      flow)
    :pull/done flow
    :pull/error flow

    :push/start (advance-flow (tick (refresh-git-state flow) :push/staging) render-progress)
    :push/staging flow
    :push/committing (do
                       (git/commit git message)
                       (advance-flow (tick flow :push/pushing) render-progress))
    :push/pushing (try
                    (git/push! git {:encrypted-credentials creds})
                    (advance-flow (tick flow :push/done) render-progress)
                    (catch Exception e
                      (println e)
                      (advance-flow (tick flow :push/error) render-progress)))
    :push/done flow
    :push/error flow))

(ui/extend-menu ::conflicts-menu nil
                [{:label "View Diff"
                  :command :show-diff}
                 {:label "Use Ours"
                  :command :use-ours}
                 {:label "Use Theirs"
                  :command :use-theirs}])

(ui/extend-menu ::staging-menu nil
                [{:label "View Diff"
                  :command :show-change-diff}
                 {:label "Stage Change"
                  :command :stage-change}])

(ui/extend-menu ::unstaging-menu nil
                [{:label "View Diff"
                  :command :show-change-diff}
                 {:label "Unstage Change"
                  :command :unstage-change}])

(defn- get-theirs [{:keys [git] :as flow} file]
  (when-let [their-bytes (git/show-file git file)]
    (String. their-bytes)))

(defn- get-ours [{:keys [git stash-info] :as flow} file]
  (when-let [stash-ref ^RevCommit (:ref stash-info)]
    (when-let [our-bytes (git/show-file git file (.name stash-ref))]
      (String. our-bytes))))

(defn- resolve-file! [!flow file]
  (let [{:keys [^Git git conflicts]} @!flow]
    (when-let [entry (get conflicts file)]
      (if (.exists (git/file git file))
        (-> git .add (.addFilepattern file) .call)
        (-> git .rm (.addFilepattern file) .call))
      (-> git .reset (.addPath file) .call)
      (swap! !flow #(-> %
                        (update :conflicts dissoc file)
                        (update :resolved assoc file entry))))))

(defn use-ours! [!flow file]
  (if-let [ours (get-ours @!flow file)]
    (spit (git/file (:git @!flow) file) ours)
    (fs/delete-file! (git/file (:git @!flow) file) {:fail :silently}))
  (resolve-file! !flow file))

(defn use-theirs! [!flow file]
  (if-let [theirs (get-theirs @!flow file)]
    (spit (git/file (:git @!flow) file) theirs)
    (fs/delete-file! (git/file (:git @!flow) file) {:fail :silently}))
  (resolve-file! !flow file))

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
       (diff-view/present-diff-data (git/selection-diff-data (:git @!flow) selection))))

(handler/defhandler :use-ours :sync
  (enabled? [selection] (pos? (count selection)))
  (run [selection !flow]
    (doseq [f selection]
      (use-ours! !flow f))))

(handler/defhandler :use-theirs :sync
  (enabled? [selection] (pos? (count selection)))
  (run [selection !flow]
    (doseq [f selection]
      (use-theirs! !flow f))))

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

(defn- personal-access-token-uri
  ^URI [remote-info]
  (when (= :https (:scheme remote-info))
    (let [host (:host remote-info)
          remote-uri (git/remote-uri remote-info)]
      (cond
        (re-find #"\bgithub\b" host)
        (.resolve remote-uri "/settings/tokens")

        (re-find #"\bgitlab\b" host)
        (.resolve remote-uri "/profile/personal_access_tokens")

        (re-find #"\bbbitbucket\b" host)
        (URI. "https://confluence.atlassian.com/bitbucket/app-passwords-828781300.html")))))

(def ^:private sync-dialog-open-atom (atom false))

(defn sync-dialog-open? []
  @sync-dialog-open-atom)

(defn open-sync-dialog [!flow prefs]
  ;; Note: It would be really nice to rewrite this using cljfx. It is quite
  ;; cumbersome to make changes as it stands. The advance-flow function above
  ;; also contributes to the overall complexity. We actually might want to
  ;; revise the git workflow to separate the acts of committing and pushing
  ;; since there are sometimes issues with this simplified model.
  (let [root            ^Parent (ui/load-fxml "sync-dialog.fxml")
        pull-root       ^Parent (ui/load-fxml "sync-pull.fxml")
        push-root       ^Parent (ui/load-fxml "sync-push.fxml")
        stage           (ui/make-dialog-stage (ui/main-stage))
        scene           (Scene. root)
        dialog-controls (ui/collect-controls root ["ok" "push" "cancel" "dialog-area" "progress-bar"])
        pull-controls   (ui/collect-controls pull-root ["username-field" "password-field" "save-password-checkbox" "pull-start-box" "conflicting" "resolved" "conflict-box" "main-label"])
        push-controls   (ui/collect-controls push-root ["changed" "staged" "message" "committer-name-field" "committer-email-field" "content-box" "main-label" "diff" "stage" "unstage"])
        render-progress (fn [progress]
                          (when progress
                            (ui/run-later
                              (ui/render-progress-controls! progress (:progress-bar dialog-controls) nil))))
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
                            (case (namespace state)
                              "pull" (do
                                       (ui/title! stage "Get Remote Changes")
                                       (ui/text! (:ok dialog-controls) "Pull")
                                       (ui/children! (:dialog-area dialog-controls) [pull-root])
                                       (ui/fill-control pull-root)
                                       (.sizeToScene (.getWindow scene)))
                              "push" (do
                                       (ui/title! stage "Push Local Changes")
                                       (ui/visible! (:push dialog-controls) false)
                                       (ui/text! (:ok dialog-controls) "Push")
                                       (ui/children! (:dialog-area dialog-controls) [push-root])
                                       (ui/fill-control push-root)
                                       (.sizeToScene (.getWindow scene))))
                            (condp = state
                              :pull/start (let [error (:error flow)
                                                remote-info (git/remote-info (:git flow) :fetch)
                                                is-ssh-remote (= :ssh (:scheme remote-info))]
                                            (ui/text! (:main-label pull-controls)
                                                      (cond
                                                        is-ssh-remote
                                                        "Cannot Sync Over SSH"

                                                        (= :https-not-authorized error)
                                                        "Invalid Username or Password"

                                                        :else
                                                        "Get Remote Changes"))
                                            (ui/visible! (:pull-start-box pull-controls) true)
                                            (ui/visible! (:conflict-box pull-controls) false)
                                            (ui/enable! (:ok dialog-controls) (not is-ssh-remote)) ; Disallow sync over SSH.
                                            (ui/set-style! (:username-field pull-controls) "error" (= :https-not-authorized error))
                                            (ui/set-style! (:password-field pull-controls) "error" (= :https-not-authorized error)))
                              :pull/conflicts (do
                                                (ui/text! (:main-label pull-controls) "Resolve Conflicts")
                                                (ui/visible! (:pull-start-box pull-controls) false)
                                                (ui/visible! (:conflict-box pull-controls) true)
                                                (ui/items! (:conflicting pull-controls) (sort (keys conflicts)))
                                                (ui/items! (:resolved pull-controls) (sort (keys resolved)))

                                                (let [button (:ok dialog-controls)]
                                                  (ui/text! button "Apply")
                                                  (ui/disable! button (not (empty? conflicts)))))
                              :pull/done (do
                                           (ui/text! (:main-label pull-controls) "Done!")
                                           (ui/visible! (:push dialog-controls) true)
                                           (ui/visible! (:pull-start-box pull-controls) false)
                                           (ui/visible! (:conflict-box pull-controls) false)
                                           (ui/text! (:ok dialog-controls) "Done"))
                              :pull/error (do
                                            (ui/text! (:main-label pull-controls) "Error getting changes")
                                            (ui/visible! (:push dialog-controls) false)
                                            (ui/visible! (:pull-start-box pull-controls) false)
                                            (ui/visible! (:conflict-box pull-controls) false)
                                            (ui/text! (:ok dialog-controls) "Done")
                                            (ui/disable! (:ok dialog-controls) true))
                              :push/staging (let [changed-view ^ListView (:changed push-controls)
                                                  staged-view ^ListView (:staged push-controls)
                                                  changed-selection (vec (ui/selection changed-view))
                                                  staged-selection (vec (ui/selection staged-view))
                                                  empty-message (empty? (ui/text (:message push-controls)))
                                                  empty-committer-name (empty? (ui/text (:committer-name-field push-controls)))
                                                  empty-committer-email (empty? (ui/text (:committer-email-field push-controls)))]
                                              (ui/items! changed-view (sort-by git/change-path modified))
                                              (ui/items! staged-view (sort-by git/change-path staged))
                                              (ui/set-style! (:message push-controls) "info" empty-message)
                                              (ui/set-style! (:committer-name-field push-controls) "info" empty-committer-name)
                                              (ui/set-style! (:committer-email-field push-controls) "info" empty-committer-email)
                                              (ui/disable! (:ok dialog-controls)
                                                           (or (empty? staged)
                                                               empty-message
                                                               empty-committer-name
                                                               empty-committer-email))

                                              ;; The stage, unstage and diff buttons start off disabled, but
                                              ;; might be enabled by the event handler triggered by select!
                                              (ui/disable! (:diff push-controls) true)
                                              (ui/disable! (:stage push-controls) true)
                                              (ui/disable! (:unstage push-controls) true)
                                              (doseq [item changed-selection]
                                                (ui/select! changed-view item))
                                              (doseq [item staged-selection]
                                                (ui/select! staged-view item)))
                              :push/done (do
                                           (ui/text! (:main-label push-controls) "Done!")
                                           (ui/visible! (:content-box push-controls) false)
                                           (ui/text! (:ok dialog-controls) "Done"))

                              :push/error (do
                                            (ui/text! (:main-label push-controls) "Error pushing changes to server")
                                            (ui/visible! (:content-box push-controls) false)
                                            (ui/text! (:ok dialog-controls) "Done")
                                            (ui/disable! (:ok dialog-controls) true))

                              nil)))]
    (update-controls @!flow)
    (add-watch !flow :updater (fn [_ _ _ flow]
                                (update-controls flow)))

    ; Disable the window close button, since it is unclear what it means.
    ; This forces the user to make an active choice between Done or Cancel.
    (ui/on-closing! stage (fn [_] false))

    (ui/on-action! (:cancel dialog-controls) (fn [_]
                                               (interactive-cancel! (partial cancel-flow! !flow))
                                               (.close stage)))
    (ui/on-action! (:ok dialog-controls) (fn [_]
                                           (let [flow @!flow
                                                 state (:state flow)]
                                             (cond
                                               (= "done" (name state))
                                               (do
                                                 (finish-flow! !flow)
                                                 (.close stage))

                                               (= :pull/start state)
                                               (swap! !flow (fn [flow]
                                                              (let [username (ui/text (:username-field pull-controls))
                                                                    password (ui/text (:password-field pull-controls))
                                                                    save-password (ui/value (:save-password-checkbox pull-controls))]
                                                                (git-credentials/write-encrypted-credentials!
                                                                  prefs (:git flow)
                                                                  (git-credentials/encrypt-credentials
                                                                    (cond-> {:username username}
                                                                            save-password (assoc :password password))))
                                                                (advance-flow
                                                                  (-> flow
                                                                      (dissoc :error)
                                                                      (assoc :verify-creds (not (empty? password)))
                                                                      (assoc :creds (git-credentials/encrypt-credentials
                                                                                      {:username username
                                                                                       :password password})))
                                                                  render-progress))))

                                               (= :push/staging state)
                                               (swap! !flow (fn [flow]
                                                              (let [committer-name (ui/text (:committer-name-field push-controls))
                                                                    committer-email (ui/text (:committer-email-field push-controls))]
                                                                (git/set-user-info! (:git flow) {:name committer-name :email committer-email})
                                                                (advance-flow
                                                                  (merge flow
                                                                         {:state :push/committing
                                                                          :message (ui/text (:message push-controls))})
                                                                  render-progress))))

                                               :else
                                               (swap! !flow advance-flow render-progress)))))
    (ui/on-action! (:push dialog-controls) (fn [_]
                                             (swap! !flow #(merge %
                                                                  {:state :push/start
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

    (let [update-push-controls! (fn [_ _ _]
                                  (update-controls @!flow))]
      (doseq [field-name [:message :committer-name-field :committer-email-field]
              :let [^TextInputControl text-input-control (get push-controls field-name)]]
        (ui/observe (.textProperty text-input-control) update-push-controls!)))

    (ui/with-controls pull-root [https-box
                                 personal-access-token-link
                                 password-field
                                 save-password-checkbox
                                 ssh-box
                                 username-field]
      (let [{:keys [git creds]} @!flow
            {:keys [username password]} (git-credentials/decrypt-credentials creds)
            remote-info (git/remote-info git :fetch)]
        (ui/text! username-field (or username ""))
        (ui/text! password-field (or password ""))
        (ui/value! save-password-checkbox (not (empty? password)))
        (case (:scheme remote-info)
          :https
          (do
            (ui/visible! https-box true)
            (ui/managed! https-box true)
            (if-some [personal-access-token-uri (personal-access-token-uri remote-info)]
              (ui/on-action! personal-access-token-link
                (fn [_]
                  (ui/open-url personal-access-token-uri)))
              (ui/enable! personal-access-token-link false)))

          :ssh
          (do
            (ui/visible! ssh-box true)
            (ui/managed! ssh-box true))

          ;; Other protocols.
          nil)))

    (let [^ListView list-view (:conflicting pull-controls)]
      (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
      (ui/context! list-view :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/register-context-menu list-view ::conflicts-menu)
      (ui/cell-factory! list-view (fn [e] {:text e})))
    (ui/cell-factory! (:resolved pull-controls) (fn [e] {:text e}))

    (ui/with-controls push-root [committer-email-field committer-name-field]
      (let [{:keys [git]} @!flow
            {:keys [name email]} (git/user-info git)]
        (ui/text! committer-name-field name)
        (ui/text! committer-email-field email)))

    (let [^ListView list-view (:changed push-controls)]
      (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
      (ui/context! list-view :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/context! (:stage push-controls) :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/bind-action! (:stage push-controls) :stage-change)
      (ui/register-context-menu list-view ::staging-menu)
      (ui/cell-factory! list-view vcs-status/render-verbose))

    (let [^ListView list-view (:staged push-controls)]
      (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
      (ui/context! list-view :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/context! (:unstage push-controls) :sync {:!flow !flow} (ui/->selection-provider list-view))
      (ui/bind-action! (:unstage push-controls) :unstage-change)
      (ui/register-context-menu list-view ::unstaging-menu)
      (ui/cell-factory! list-view vcs-status/render-verbose))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (= code KeyCode/ESCAPE)
                                           (interactive-cancel! (partial cancel-flow! !flow))
                                           (.close stage)))))
    (.setScene stage scene)

    (try
      (reset! sync-dialog-open-atom true)
      (ui/show-and-wait-throwing! stage)
      (let [files-may-have-changed (not= :pull/start (:state @!flow))]
        files-may-have-changed)
      (catch Exception e
        (cancel-flow! !flow)
        (throw e))
      (finally
        (reset! sync-dialog-open-atom false)))))
