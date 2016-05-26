(ns editor.sync
  (:require [clojure
             [set :as set]
             [string :as string]]
            [clojure.java.io :as io]
            [editor
             [console :as console]
             [dialogs :as dialogs]
             [diff-view :as diff-view]
             [git :as git]
             [handler :as handler]
             [ui :as ui]]
            [editor.progress :as progress])
  (:import javafx.geometry.Pos
           [org.eclipse.jgit.api Git PullResult]
           [org.eclipse.jgit.api.errors StashApplyFailureException]
           [javafx.scene Parent Scene Node]
           [javafx.scene.control ContentDisplay Label MenuButton]
           [javafx.scene.input KeyCode KeyEvent]
           javafx.scene.layout.BorderPane
           [javafx.stage Modality Stage]
           [org.eclipse.jgit.api ResetCommand$ResetType CheckoutCommand$Stage]
           org.eclipse.jgit.api.errors.CheckoutConflictException))

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


(defn make-flow [^Git git prefs]
  (let [start-ref (git/get-current-commit-ref git)
        stash-ref (git/stash git)]
    {:state     :pull/start
     :git       git
     :creds     (git/credentials prefs)
     :start-ref start-ref
     :stash-ref stash-ref
     :progress  (progress/make "pull" 5)
     :staged    #{}
     :conflicts #{}
     }))

(defn cancel-flow [{:keys [git start-ref stash-ref] :as flow}]
  (git/hard-reset git start-ref)
  (when stash-ref
    (git/stash-apply git stash-ref)
    (git/stash-drop git stash-ref)))

(defmacro with-error-in-console [& body]
  `(try
     ~@body
     (catch Exception e#
       (console/append-console-message! (.toString e#)))))

(defn- tick [flow new-state]
  (-> flow
      (assoc :state new-state)
      (update :progress progress/advance)))

(defn advance-flow [{:keys [git state progress creds conflicts stash-ref] :as flow} render-progress]
  (println "advance" :pull state)
  (render-progress progress)
  (condp = state
    :pull/start     (advance-flow (tick flow :pull/pulling) render-progress)
    :pull/pulling   (let [^PullResult pull-res (with-error-in-console (git/pull git creds))]
                      (if (and pull-res (.isSuccessful pull-res))
                        (advance-flow (tick flow :pull/applying) render-progress)
                        (advance-flow (tick flow :pull/error))))
    :pull/applying  (let [stash-res (with-error-in-console
                                      (try (git/stash-apply git stash-ref)
                                           (catch StashApplyFailureException e
                                             :conflict)))
                          status    (git/status git)]
                      (cond
                        (= :conflict stash-res) (advance-flow (-> flow
                                                                  (tick :pull/conflicts)
                                                                  (assoc :conflicts (:conflicting-stage-state status)))
                                                              render-progress)
                        stash-res (advance-flow (tick flow :pull/done) render-progress)
                        :else (advance-flow (tick flow :pull/error))))
    :pull/conflicts (if (empty? conflicts)
                      (advance-flow (tick flow :pull/done) render-progress)
                      flow)
    :pull/done      flow
    :pull/error     flow

    :push/start     :push/staging
    :push/staging   :push/comitting
    :push/comitting :push/pushing
    :push/pushing   (rand-nth [:pull/start :push/done])
    :push/done      :push/done))

(defn resolve-file [{:keys [^Git git conflicts] :as flow} file resolution]
  (let [action (condp = [(get conflicts file) resolution]
                 [:both-added :ours]        :checkout
                 [:both-added :theirs]      :checkout

                 [:both-modified :theirs]   :checkout
                 [:both-modified :ours]     :checkout

                 [:deleted-by-them :ours]   :checkout
                 [:deleted-by-them :theirs] :rm

                 [:deleted-by-us :ours]     :rm
                 [:deleted-by-us :theirs]   :checkout)]
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
            (.call))))))

(defn- stage-file [flow entry]
  (update flow :staged conj entry))

(defn- unstage-file [flow entry]
  (update flow :staged disj entry))

;; =================================================================================

(defn push-flow [{:keys [^Git git] :as flow} to-commit]
  (let [simple-status (git/simple-status git)]
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

(defn commit-merge [{:keys [^Git git] :as flow}]
  (-> git (.commit) (.call)))

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

(defn- show-diff [^Git git status]
  (let [old-name (or (:old-path status) (:new-path status) )
        new-name (or (:new-path status) (:old-path status) )
        work-tree (.getWorkTree (.getRepository git))
        old (String. ^bytes (git/show-file git old-name))
        new (slurp (io/file work-tree new-name))]
    (diff-view/make-diff-viewer old-name old new-name new)))

(def short-status {:add "A" :modify "M" :delete "D" :rename "R"})

(defn- status-render [status]
  {:text (format "[%s]%s" ((:change-type status) short-status) (or (:new-path status) (:old-path status)))})

(ui/extend-menu ::conflicts-menu nil
                [{:label "Show diff"
                  :command :show-diff}
                 {:label "Use ours"
                  :command :use-ours}
                 {:label "Use theirs"
                  :command :use-theirs}])

(handler/defhandler :show-diff :sync
  (enabled? [selection] (= 1 (count selection)))
  (run []))

(handler/defhandler :use-ours :sync
  (enabled? [selection] (pos? (count selection)))
  (run [selection]))

(handler/defhandler :use-theirs :sync
  (enabled? [selection] (pos? (count selection)))
  (run [selection]))

(defn open-sync-dialog [flow]
  (let [root            ^Parent (ui/load-fxml "sync-dialog.fxml")
        pull-root       ^Parent (ui/load-fxml "sync-pull.fxml")
        push-root       ^Parent (ui/load-fxml "sync-push.fxml")
        stage           (Stage.)
        scene           (Scene. root)
        dialog-controls (ui/collect-controls root [ "ok" "cancel" "dialog-area" "progress-bar"])
        pull-controls   (ui/collect-controls pull-root ["conflicting" "conflict-box"])
        push-controls   (ui/collect-controls push-root [])
        !flow           (atom flow)
        render-progress (fn [progress]
                          (ui/run-later
                           (ui/update-progress-controls! progress (:progress-bar dialog-controls) nil)))
        update-controls (fn [{:keys [state conflicts] :as flow}]
                          (condp = state
                            :pull/conflicts (do
                                              (println "* conflicts" conflicts)
                                              (.setVisible ^Node (:conflict-box pull-controls) true)
                                              (ui/items! (:conflicting pull-controls) (sort (keys conflicts))))
                            nil))]
    (add-watch !flow :updater (fn [_ _ _ flow]
                                (update-controls flow)))
    (def flw !flow)
    (ui/title! stage "Sync")
    (ui/text! (:ok dialog-controls) "Pull")
    (ui/children! (:dialog-area dialog-controls) [pull-root])
    (ui/fill-control pull-root)
    (ui/on-action! (:cancel dialog-controls) (fn [_]
                                               (cancel-flow @!flow)
                                               (.close stage)))
    (ui/on-action! (:ok dialog-controls) (fn [_]
                                           (swap! !flow advance-flow render-progress)))

    (ui/register-context-menu (:conflicting pull-controls) ::conflicts-menu)
    (ui/cell-factory! (:conflicting pull-controls) (fn [e] {:text e}))

    (comment
      (ui/cell-factory! (:changed controls) (fn [e] (status-render e)))
      (ui/cell-factory! (:staged controls) (fn [e] (status-render e)))
      (ui/cell-factory! (:conflicting controls) (fn [p] {:text            (str p)
                                                         :content-display ContentDisplay/GRAPHIC_ONLY
                                                         :gfx             (make-cell (str p))}))

      (ui/on-double! (:changed controls) (fn [evt]
                                           (when-let [e (first (ui/selection (:changed controls)))]
                                             (swap! flow-atom git-stage e)
                                             (refresh-stage flow-atom controls))))

      (ui/on-double! (:staged controls) (fn [evt]
                                          (when-let [e (first (ui/selection (:staged controls)))]
                                            (swap! flow-atom git-unstage e)
                                            (prn @flow-atom)
                                            (refresh-stage flow-atom controls))))

      (ui/on-key-pressed! (:changed controls) (fn [e]
                                                (when (= (.getCode ^KeyEvent e) KeyCode/ENTER)
                                                  (when-let [status (first (ui/selection (:changed controls)))]
                                                    (show-diff (:git flow) status)))))

      (refresh-stage flow-atom controls)
      (ad))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (= code KeyCode/ESCAPE) true
                                               (cancel-flow @!flow)
                                               (.close stage)))))

    (.initModality stage Modality/APPLICATION_MODAL)
    (.setScene stage scene)
    (println "opening")
    (ui/show-and-wait! stage))
  (println "closing"))
