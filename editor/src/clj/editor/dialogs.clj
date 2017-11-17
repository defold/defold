(ns editor.dialogs
  (:require [clojure.core.reducers :as r]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.ui :as ui]
            [editor.handler :as handler]
            [editor.code :as code]
            [editor.core :as core]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.jfx :as jfx]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.defold-project :as project]
            [editor.github :as github]
            [service.log :as log])
  (:import [java.io File]
           [java.nio.file Path Paths]
           [java.util.regex Pattern]
           [javafx.event Event ActionEvent]
           [javafx.geometry Point2D Pos]
           [javafx.scene Node Parent Scene]
           [javafx.scene.control Button Label ListView ProgressBar TextArea TextField TreeItem TreeView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.input KeyEvent]
           [javafx.scene.layout HBox Region]
           [javafx.scene.text Text TextFlow]
           [javafx.stage Stage StageStyle Modality DirectoryChooser]))

(set! *warn-on-reflection* true)

(defprotocol Dialog
  (show! [this functions])
  (close! [this])
  (return! [this r])
  (dialog-root [this])
  (error! [this msg])
  (progress-bar [this])
  (task! [this fn]))

(defrecord TaskDialog []
  Dialog
  (show! [this functions]
    (swap! (:functions this) merge functions)
    ((:refresh this))
    (ui/show-and-wait! (:stage this))
    @(:return this))
  (close! [this] (ui/close! (:stage this)))
  (return! [this r] (reset! (:return this) r))
  (dialog-root [this] (:dialog-root this))
  (error! [this msg]
    ((:set-error this) msg))
  (progress-bar [this] (:progress-bar this))
  (task! [this fn]
    (future
      (try
        (ui/run-later (ui/disable! (:root this) true))
        (fn)
        (catch Throwable e
          (log/error :exception e)
          (ui/run-later (error! this (.getMessage e))))
        (finally
          (ui/run-later (ui/disable! (:root this) false)))))))

(defn make-progress-dialog [title message]
  (let [root     ^Parent (ui/load-fxml "progress.fxml")
        stage    (ui/make-dialog-stage)
        scene    (Scene. root)
        controls (ui/collect-controls root ["title" "message" "progress"])]
    (ui/title! stage title)
    (ui/text! (:title controls) title)
    (ui/text! (:message controls) message)
    (.setProgress ^ProgressBar (:progress controls) 0)
    (.setScene stage scene)
    (.setAlwaysOnTop stage true)
    (.show stage)
    {:stage              stage
     :render-progress-fn (fn [progress]
                           (ui/update-progress-controls! progress (:progress controls) (:message controls)))}))

(defn ^:dynamic make-alert-dialog [text]
  (let [root ^Parent (ui/load-fxml "alert.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)]
    (ui/title! stage "Alert")
    (ui/with-controls root [^TextArea message ^Button ok]
      (ui/text! message text)
      (ui/on-action! ok (fn [_] (.close stage)))
      (.setOnShown stage (ui/event-handler _ (.setScrollTop message 0.0))))
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-message-box [title text]
  (let [root ^Parent (ui/load-fxml "message.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)]
    (ui/title! stage title)
    (ui/with-controls root [message ok]
      (ui/text! message text)
      (ui/on-action! ok (fn [_] (.close stage))))
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-confirm-dialog
  ([text]
   (make-confirm-dialog text {}))
  ([text options]
   (let [root     ^Region (ui/load-fxml "confirm.fxml")
         stage    (if-let [owner-window (:owner-window options)]
                    (ui/make-dialog-stage owner-window)
                    (ui/make-dialog-stage))
         scene    (Scene. root)
         result   (atom false)]
     (ui/with-controls root [^Label message ^Button ok ^Button cancel]
       (ui/text! message text)
       (ui/text! ok (get options :ok-label "OK"))
       (ui/text! cancel (get options :cancel-label "Cancel"))
       (ui/on-action! ok (fn [_]
                           (reset! result true)
                           (.close stage)))
       (ui/on-action! cancel (fn [_]
                               (.close stage))))
     (when-let [pref-width (:pref-width options)]
       (.setPrefWidth root pref-width))
     (ui/title! stage (get options :title "Please Confirm"))
     (.setScene stage scene)
     (ui/show-and-wait! stage)
     @result)))

(defn make-pending-update-dialog
  [^Stage owner]
  (let [root ^Parent (ui/load-fxml "update-alert.fxml")
        stage (ui/make-dialog-stage owner)
        scene (Scene. root)
        result (atom false)]
    (ui/title! stage "Update Available")
    (ui/with-controls root [ok cancel]
      (ui/on-action! ok (fn on-ok! [_] (reset! result true) (.close stage)))
      (ui/on-action! cancel (fn on-cancel! [_] (.close stage))))
    (.setScene stage scene)
    (ui/show-and-wait! stage)
    @result))

(handler/defhandler ::report-error :dialog
  (run [sentry-id-promise]
    (let [sentry-id (deref sentry-id-promise 100 nil)
          fields (cond-> {}
                   sentry-id
                   (assoc "Error" (format "<a href='https://sentry.io/defold/editor2/?query=id%%3A\"%s\"'>%s</a>"
                                          sentry-id sentry-id)))]
      (ui/open-url (github/new-issue-link fields)))))

(defn- messages
  [ex-map]
  (->> (tree-seq :via :via ex-map)
       (drop 1)
       (map (fn [{:keys [message ^Class type]}]
              (format "%s: %s" (.getName type) (or message "Unknown"))))
       (str/join "\n")))

(defn make-error-dialog
  [ex-map sentry-id-promise]
  (let [root     ^Parent (ui/load-fxml "error.fxml")
        stage    (ui/make-dialog-stage)
        scene    (Scene. root)
        controls (ui/collect-controls root ["message" "dismiss" "report"])]
    (ui/context! root :dialog {:stage stage :sentry-id-promise sentry-id-promise} nil)
    (ui/title! stage "Error")
    (ui/text! (:message controls) (messages ex-map))
    (ui/bind-action! (:dismiss controls) ::close)
    (ui/bind-action! (:report controls) ::report-error)
    (ui/bind-keys! root {KeyCode/ESCAPE ::close})
    (ui/request-focus! (:report controls))
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-task-dialog [dialog-fxml options]
  (let [root ^Parent (ui/load-fxml "task-dialog.fxml")
        dialog-root ^Parent (ui/load-fxml dialog-fxml)
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["error" "ok" "dialog-area" "error-group" "progress-bar"])

        set-error (fn [msg]
                    (let [visible (not (nil? msg))
                          changed (not= msg (ui/text (:error controls)))]
                      (when changed
                        (ui/text! (:error controls) msg)
                        (ui/managed! (:error-group controls) visible)
                        (ui/visible! (:error-group controls) visible)
                        (.sizeToScene stage))))]
    (ui/text! (:ok controls) (get options :ok-label "OK"))
    (ui/title! stage (or (:title options) ""))
    (ui/children! (:dialog-area controls) [dialog-root])
    (ui/fill-control dialog-root)

    (ui/visible! (:error-group controls) false)
    (ui/managed! (:error-group controls) false)

    (.setScene stage scene)
    (let [functions (atom {:ready? (fn [] false)
                           :on-ok (fn [] nil)})
          dialog (map->TaskDialog (merge {:root root
                                          :return (atom nil)
                                          :dialog-root dialog-root
                                          :stage stage
                                          :set-error set-error
                                          :functions functions} controls))
          refresh (fn []
                    (set-error nil)
                    (ui/disable! (:ok controls) (not ((:ready? @functions)))))
          h (ui/event-handler event (refresh))]
      (ui/on-action! (:ok controls) (fn [_] ((:on-ok @functions))))
      (.addEventFilter scene ActionEvent/ACTION h)
      (.addEventFilter scene KeyEvent/KEY_TYPED h)

      (doseq [tf (.lookupAll root "TextField")]
        (.addListener (.textProperty ^TextField tf)
          (reify javafx.beans.value.ChangeListener
            (changed [this observable old-value new-value]
              (when (not= old-value new-value)
                (refresh))))))

      (assoc dialog :refresh refresh))))

(handler/defhandler ::confirm :dialog
  (enabled? [selection]
            (seq selection))
  (run [^Stage stage selection]
       (ui/user-data! stage ::selected-items selection)
       (ui/close! stage)))

(handler/defhandler ::close :dialog
  (run [^Stage stage]
       (ui/close! stage)))

(handler/defhandler ::focus :dialog
  (active? [user-data] (if-let [active-fn (:active-fn user-data)]
                         (active-fn nil)
                         true))
  (run [^Stage stage user-data]
       (when-let [^Node node (:node user-data)]
         (ui/request-focus! node))))

(defn- default-filter-fn [cell-fn text items]
  (let [text (str/lower-case text)
        str-fn (comp str/lower-case :text cell-fn)]
    (filter (fn [item] (str/starts-with? (str-fn item) text)) items)))

(defn make-select-list-dialog [items options]
  (let [^Parent root (ui/load-fxml "select-list.fxml")
        scene (Scene. root)
        ^Stage stage (doto (ui/make-dialog-stage (ui/main-stage))
                       (ui/title! (or (:title options) "Select Item"))
                       (.setScene scene))
        controls (ui/collect-controls root ["filter" "item-list" "ok"])
        ok-label (:ok-label options "OK")
        ^TextField filter-field (:filter controls)
        filter-value (:filter options "")
        cell-fn (:cell-fn options identity)
        ^ListView item-list (doto ^ListView (:item-list controls)
                              (.setFixedCellSize 27.0) ; Fixes missing cells in VirtualFlow
                              (ui/cell-factory! cell-fn)
                              (ui/selection-mode! (:selection options :single)))]
    (doto item-list
      (ui/observe-list (ui/items item-list)
                       (fn [_ items]
                         (when (not (empty? items))
                           (ui/select-index! item-list 0))))
      (ui/items! (if (str/blank? filter-value) items [])))
    (let [filter-fn (or (:filter-fn options) (partial default-filter-fn cell-fn))]
      (ui/observe (.textProperty filter-field)
                  (fn [_ _ ^String new]
                    (let [filtered-items (filter-fn new items)]
                      (ui/items! item-list filtered-items)))))
    (doto filter-field
      (.setText filter-value)
      (.setPromptText (:prompt options "")))

    (ui/context! root :dialog {:stage stage} (ui/->selection-provider item-list))
    (ui/text! (:ok controls) ok-label)
    (ui/bind-action! (:ok controls) ::confirm)
    (ui/observe-selection item-list (fn [_ _] (ui/refresh-bound-action-enabled! (:ok controls))))
    (ui/bind-double-click! item-list ::confirm)
    (ui/bind-keys! root {KeyCode/ENTER ::confirm
                         KeyCode/ESCAPE ::close
                         KeyCode/DOWN [::focus {:active-fn (fn [_] (and (seq (ui/items item-list))
                                                                        (ui/focus? filter-field)))
                                                :node item-list}]
                         KeyCode/UP [::focus {:active-fn (fn [_] (= 0 (.getSelectedIndex (.getSelectionModel item-list))))
                                              :node filter-field}]})

    (ui/show-and-wait! stage)

    (ui/user-data stage ::selected-items)))

(defn- resource->fuzzy-matched-resource [pattern resource]
  (when-some [[score matching-indices] (fuzzy-text/match-path pattern (resource/proj-path resource))]
    (with-meta resource
               {:score score
                :matching-indices matching-indices})))

(defn- descending-order [a b]
  (compare b a))

(defn- fuzzy-resource-filter-fn [filter-value resources]
  (if (empty? filter-value)
    resources
    (sort-by (comp :score meta)
             descending-order
             (seq (r/foldcat (r/filter some?
                                       (r/map (partial resource->fuzzy-matched-resource filter-value)
                                              resources)))))))

(defn- override-seq [node-id]
  (tree-seq g/overrides g/overrides node-id))

(defn- file-scope [node-id]
  (last (take-while (fn [n] (and n (not (g/node-instance? project/Project n)))) (iterate core/scope node-id))))

(defn- refs-filter-fn [project filter-value items]
  ;; Temp limitation to avoid stalls
  ;; Optimally we would do the work in the background with a progress-bar
  (if-let [n (project/get-resource-node project filter-value)]
    (->>
      (let [all (override-seq n)]
        (mapcat (fn [n]
                  (keep (fn [[src src-label node-id label]]
                          (when-let [node-id (file-scope node-id)]
                            (when (and (not= n node-id)
                                       (g/node-instance? resource-node/ResourceNode node-id))
                              (when-let [r (g/node-value node-id :resource)]
                                (when (resource/exists? r)
                                  r)))))
                        (g/outputs n)))
                all))
      distinct)
    []))

(defn- sub-nodes [n]
  (g/node-value n :nodes))

(defn- sub-seq [n]
  (tree-seq (partial g/node-instance? resource-node/ResourceNode) sub-nodes n))

(defn- deps-filter-fn [project filter-value items]
  ;; Temp limitation to avoid stalls
  ;; Optimally we would do the work in the background with a progress-bar
  (if-let [node-id (project/get-resource-node project filter-value)]
    (->>
      (let [all (sub-seq node-id)]
        (mapcat
          (fn [n]
            (keep (fn [[src src-label tgt tgt-label]]
                    (when-let [src (file-scope src)]
                      (when (and (not= node-id src)
                                 (g/node-instance? resource-node/ResourceNode src))
                        (when-let [r (g/node-value src :resource)]
                          (when (resource/exists? r)
                            r)))))
                  (g/inputs n)))
          all))
      distinct)
    []))

(defn- make-text-run [text style-class]
  (let [text-view (Text. text)]
    (when (some? style-class)
      (.add (.getStyleClass text-view) style-class))
    text-view))

(defn- matched-text-runs [text matching-indices]
  (let [/ (or (some-> text (str/last-index-of \/) inc) 0)]
    (into []
          (mapcat (fn [[matched? start end]]
                    (cond
                      matched?
                      [(make-text-run (subs text start end) "matched")]

                      (< start / end)
                      [(make-text-run (subs text start /) "diminished")
                       (make-text-run (subs text / end) nil)]

                      (<= start end /)
                      [(make-text-run (subs text start end) "diminished")]

                      :else
                      [(make-text-run (subs text start end) nil)])))
          (fuzzy-text/runs (count text) matching-indices))))

(defn- make-matched-list-item-graphic [icon text matching-indices]
  (let [icon-view (jfx/get-image-view icon 16)
        text-view (TextFlow. (into-array Text (matched-text-runs text matching-indices)))]
    (doto (HBox. (ui/node-array [icon-view text-view]))
      (.setAlignment Pos/CENTER_LEFT)
      (.setSpacing 4.0))))

(defn make-resource-dialog [workspace project options]
  (let [exts         (let [ext (:ext options)] (if (string? ext) (list ext) (seq ext)))
        accepted-ext (if (seq exts) (set exts) (constantly true))
        items        (into []
                           (filter #(and (= :file (resource/source-type %)) (accepted-ext (resource/ext %))))
                           (g/node-value workspace :resource-list))
        options (-> {:title "Select Resource"
                     :prompt "Type to filter"
                     :filter ""
                     :cell-fn (fn [r]
                                (let [text (resource/proj-path r)
                                      icon (workspace/resource-icon r)
                                      style (resource/style-classes r)
                                      tooltip (when-let [tooltip-gen (:tooltip-gen options)] (tooltip-gen r))
                                      matching-indices (:matching-indices (meta r))]
                                  (cond-> {:style style
                                           :tooltip tooltip}

                                          (empty? matching-indices)
                                          (assoc :icon icon :text text)

                                          :else
                                          (assoc :graphic (make-matched-list-item-graphic icon text matching-indices)))))
                     :filter-fn (fn [filter-value items]
                                  (let [fns {"refs" (partial refs-filter-fn project)
                                             "deps" (partial deps-filter-fn project)}
                                        [command arg] (let [parts (str/split filter-value #":")]
                                                        (if (< 1 (count parts))
                                                          parts
                                                          [nil (first parts)]))
                                        f (get fns command fuzzy-resource-filter-fn)]
                                    (f arg items)))}
                  (merge options))]
    (make-select-list-dialog items options)))

(declare sanitize-folder-name)

(defn make-new-folder-dialog [base-dir {:keys [validate]}]
  (let [root ^Parent (ui/load-fxml "new-folder-dialog.fxml")
        stage (ui/make-dialog-stage (ui/main-stage))
        scene (Scene. root)
        controls (ui/collect-controls root ["name" "ok" "path"])
        return (atom nil)
        reset-return! (fn [] (reset! return (some-> (ui/text (:name controls)) sanitize-folder-name not-empty)))
        close (fn [] (reset-return!) (.close stage))
        validate (or validate (constantly nil))
        do-validation (fn []
                        (let [sanitized (some-> (not-empty (ui/text (:name controls))) sanitize-folder-name)
                              validation-msg (some-> sanitized validate)]
                        (if (or (nil? sanitized) validation-msg)
                          (do (ui/text! (:path controls) (or validation-msg ""))
                              (ui/enable! (:ok controls) false))
                          (do (ui/text! (:path controls) sanitized)
                              (ui/enable! (:ok controls) true)))))]
    (ui/title! stage "New Folder")

    (ui/on-action! (:ok controls) (fn [_] (close)))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (condp = code
                                                 KeyCode/ENTER (if (ui/enabled? (:ok controls)) (do (reset-return!) true) false)
                                                 KeyCode/ESCAPE true
                                                 false)
                                           (.close stage)))))

    (ui/on-edit! (:name controls) (fn [_old _new] (do-validation)))

    (.setScene stage scene)

    (do-validation)

    (ui/show-and-wait! stage)

    @return))

(defn make-target-ip-dialog [ip msg]
  (let [root     ^Parent (ui/load-fxml "target-ip-dialog.fxml")
        stage    (ui/make-dialog-stage (ui/main-stage))
        scene    (Scene. root)
        controls (ui/collect-controls root ["add" "cancel" "ip" "msg"])
        return   (atom nil)]
    (ui/text! (:msg controls) (or msg "Enter Target IP address"))
    (ui/text! (:ip controls) ip)
    (ui/title! stage "Target IP")

    (ui/on-action! (:add controls)
                   (fn [_]
                     (reset! return (ui/text (:ip controls)))
                     (.close stage)))
    (ui/on-action! (:cancel controls)
                   (fn [_] (.close stage)))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (condp = code
                                                 KeyCode/ENTER  (do (reset! return (ui/text (:ip controls))) true)
                                                 KeyCode/ESCAPE true
                                                 false)
                                           (.close stage)))))

    (.setScene stage scene)
    (ui/show-and-wait! stage)

    @return))

(defn- sanitize-common [name]
  (-> name
      str/trim
      (str/replace #"[/\\]" "") ; strip path separators
      (str/replace #"[\"']" "") ; strip quotes
      (str/replace #"^\.*" "") ; prevent hiding files (.dotfile)
      ))

(defn sanitize-file-name [extension name]
  (-> name
      sanitize-common
      (#(if (empty? extension) (str/replace % #"\..*" "" ) %)) ; disallow adding extension = resource type
      (#(if (and (seq extension) (seq %))
          (str % "." extension)
          %)))) ; append extension if there was one

(defn sanitize-folder-name [name]
  (sanitize-common name))

(defn make-rename-dialog ^String [name {:keys [title label validate sanitize] :as options}]
  (let [root     ^Parent (ui/load-fxml "rename-dialog.fxml")
        stage    (ui/make-dialog-stage (ui/main-stage))
        scene    (Scene. root)
        controls (ui/collect-controls root ["name" "path" "ok" "name-label"])
        return   (atom nil)
        reset-return! (fn [] (reset! return (some-> (ui/text (:name controls)) sanitize not-empty)))
        close    (fn [] (reset-return!) (.close stage))
        validate (or validate (constantly nil))
        do-validation (fn []
                        (let [sanitized (some-> (not-empty (ui/text (:name controls))) sanitize)
                              validation-msg (some-> sanitized validate)]
                          (if (or (empty? sanitized) validation-msg)
                            (do (ui/text! (:path controls) (or validation-msg ""))
                                (ui/enable! (:ok controls) false))
                            (do (ui/text! (:path controls) sanitized)
                                (ui/enable! (:ok controls) true)))))]
    (ui/title! stage title)
    (when label
      (ui/text! (:name-label controls) label))
    (when-not (empty? name)
      (ui/text! (:path controls) (sanitize name))
      (ui/text! (:name controls) name)
      (.selectAll ^TextField (:name controls)))

    (ui/on-action! (:ok controls) (fn [_] (close)))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (condp = code
                                                 KeyCode/ENTER  (if (ui/enabled? (:ok controls)) (do (reset-return!) true) false)
                                                 KeyCode/ESCAPE true
                                                 false)
                                           (.close stage)))))

    (ui/on-edit! (:name controls) (fn [_old _new] (do-validation)))

    (.setScene stage scene)

    (do-validation)

    (ui/show-and-wait! stage)

    @return))

(defn- relativize [^File base ^File path]
  (let [[^Path base ^Path path] (map #(Paths/get (.toURI ^File %)) [base path])]
    (str ""
         (when (.startsWith path base)
           (-> base
             (.relativize path)
             (.toString))))))

(defn make-new-file-dialog [^File base-dir ^File location type ext]
  (let [root ^Parent (ui/load-fxml "new-file-dialog.fxml")
        stage (ui/make-dialog-stage (ui/main-stage))
        scene (Scene. root)
        controls (ui/collect-controls root ["name" "location" "browse" "path" "ok"])
        return (atom nil)
        close (fn [perform?]
                (when perform?
                  (reset! return (File. base-dir (ui/text (:path controls)))))
                (.close stage))
        set-location (fn [location] (ui/text! (:location controls) (relativize base-dir location)))]
    (ui/title! stage (str "New " type))
    (set-location location)

    (.bind (.textProperty ^TextField (:path controls))
      (.concat (.concat (.textProperty ^TextField (:location controls)) "/") (.concat (.textProperty ^TextField (:name controls)) (str "." ext))))

    (ui/on-action! (:browse controls) (fn [_] (let [location (-> (doto (DirectoryChooser.)
                                                                   (.setInitialDirectory (File. (str base-dir "/" (ui/text (:location controls)))))
                                                                   (.setTitle "Set Path"))
                                                               (.showDialog nil))]
                                                (when location
                                                  (set-location location)))))
    (ui/on-action! (:ok controls) (fn [_] (close true)))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (condp = code
                                           KeyCode/ENTER (close true)
                                           KeyCode/ESCAPE (close false)
                                           false))))

    (.setScene stage scene)
    (ui/show-and-wait! stage)

    @return))

(defn make-goto-line-dialog [result]
  (let [root ^Parent (ui/load-fxml "goto-line-dialog.fxml")
        stage (ui/make-dialog-stage (ui/main-stage))
        scene (Scene. root)
        controls (ui/collect-controls root ["line"])
        close (fn [v] (do (deliver result v) (.close stage)))]
    (ui/title! stage "Go to line")
    (.setOnKeyPressed scene
                      (ui/event-handler e
                           (let [key (.getCode ^KeyEvent e)]
                             (when (= key KeyCode/ENTER)
                               (close (try
                                        (Integer/parseInt (ui/text (:line controls)))
                                        (catch Exception _))))
                             (when (= key KeyCode/ESCAPE)
                               (close nil)))))
    (.setScene stage scene)
    (ui/show! stage)
    stage))

(defn make-proposal-popup [result screen-point proposals target text-area]
  (let [root ^Parent (ui/load-fxml "text-proposals.fxml")
        stage (ui/make-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["proposals" "proposals-box"])
        close (fn [v] (do (deliver result v) (.close stage)))
        ^ListView list-view  (:proposals controls)
        filter-text (atom target)
        filter-fn (fn [i] (str/starts-with? (:name i) @filter-text))
        update-items (fn [] (try (let [new-items (filter filter-fn proposals)]
                                  (if (empty? new-items)
                                    (close nil)
                                    (do
                                      (ui/items! list-view new-items)
                                      (.select (.getSelectionModel list-view) 0))))
                                (catch Exception e
                                  (do
                                    (println "Proposal filter bad filter pattern " @filter-text)
                                    (swap! filter-text #(apply str (drop-last %)))))))]
    (.setFill scene nil)
    (.initStyle stage StageStyle/UNDECORATED)
    (.initStyle stage StageStyle/TRANSPARENT)
    (.setX stage (.getX ^Point2D screen-point))
    (.setY stage (.getY ^Point2D screen-point))
    (ui/items! list-view proposals)
    (.select (.getSelectionModel list-view) 0)
    (ui/cell-factory! list-view (fn [proposal] {:text (:display-string proposal)}))
    (ui/on-focus! list-view (fn [got-focus] (when-not got-focus (close nil))))
    (.setOnMouseClicked list-view (ui/event-handler e (close (ui/selection list-view))))
    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (cond
                                           (= code (KeyCode/UP)) (ui/request-focus! list-view)
                                           (= code (KeyCode/DOWN)) (ui/request-focus! list-view)
                                           (= code (KeyCode/ENTER)) (close (ui/selection list-view))
                                           (= code (KeyCode/TAB)) (close (ui/selection list-view))
                                           (= code (KeyCode/ESCAPE)) (close nil)

                                           (or (= code (KeyCode/LEFT)) (= code (KeyCode/RIGHT)))
                                           (do
                                             (Event/fireEvent text-area (.copyFor event (.getSource event) text-area))
                                             (close nil))

                                           (or (= code (KeyCode/BACK_SPACE)) (= code (KeyCode/DELETE)))
                                           (if (empty? @filter-text)
                                             (close nil)
                                             (do
                                               (swap! filter-text #(apply str (drop-last %)))
                                               (update-items)
                                               (Event/fireEvent text-area (.copyFor event (.getSource event) text-area))))

                                           :default true))))
    (.addEventFilter scene KeyEvent/KEY_TYPED
                     (ui/event-handler event
                                      (let [key-typed (.getCharacter ^KeyEvent event)]
                                        (cond

                                          (and (not-empty key-typed) (not (code/control-char-or-delete key-typed)))
                                          (do
                                            (swap! filter-text str key-typed)
                                            (update-items)
                                            (Event/fireEvent text-area (.copyFor event (.getSource event) text-area)))

                                          :default true))))

    (.initOwner stage (ui/main-stage))
    (.initModality stage Modality/NONE)
    (.setScene stage scene)
    (ui/show! stage)
    stage))


(handler/defhandler ::rename-conflicting-files :dialog
  (run [^Stage stage]
    (ui/user-data! stage ::file-conflict-resolution-strategy :rename)
    (ui/close! stage)))

(handler/defhandler ::overwrite-conflicting-files :dialog
  (run [^Stage stage]
    (ui/user-data! stage ::file-conflict-resolution-strategy :overwrite)
    (ui/close! stage)))

(defn ^:dynamic make-resolve-file-conflicts-dialog
  [src-dest-pairs]
  (let [^Parent root (ui/load-fxml "resolve-file-conflicts.fxml")
        scene (Scene. root)
        ^Stage stage (doto (ui/make-dialog-stage (ui/main-stage))
                       (ui/title! "Name Conflict")
                       (.setScene scene))
        controls (ui/collect-controls root ["message" "rename" "overwrite" "cancel"])]
    (ui/context! root :dialog {:stage stage} nil)
    (ui/bind-action! (:rename controls) ::rename-conflicting-files)
    (ui/bind-action! (:overwrite controls) ::overwrite-conflicting-files)
    (ui/bind-action! (:cancel controls) ::close)
    (ui/bind-keys! root {KeyCode/ESCAPE ::close})
    (ui/text! (:message controls) (let [conflict-count (count src-dest-pairs)]
                                    (if (= 1 conflict-count)
                                      "The destination has an entry with the same name."
                                      (format "The destination has %d entries with conflicting names." conflict-count))))
    (ui/show-and-wait! stage)
    (ui/user-data stage ::file-conflict-resolution-strategy)))
