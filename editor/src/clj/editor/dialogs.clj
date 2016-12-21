(ns editor.dialogs
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.ui :as ui]
            [editor.handler :as handler]
            [editor.code :as code]
            [editor.core :as core]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.defold-project :as project]
            [editor.defold-project-search :as project-search]
            [editor.github :as github]
            [service.log :as log])
  (:import [java.io File]
           [java.nio.file Path Paths]
           [java.util Collection List]
           [javafx.animation AnimationTimer]
           [javafx.event Event ActionEvent]
           [javafx.geometry Point2D]
           [javafx.scene Parent Scene]
           [javafx.scene.control TextField TreeView TreeItem ListView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.input KeyEvent]
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

(defonce focus-state (atom nil))

(defn record-focus-change!
  [focused?]
  (reset! focus-state {:focused? focused?
                       :t (System/currentTimeMillis)}))

(defn observe-focus
  [^Stage stage]
  (ui/observe (.focusedProperty stage)
              (fn [property old-val new-val]
                (record-focus-change! new-val))))

(defn make-alert-dialog [message]
  (let [root     ^Parent (ui/load-fxml "alert.fxml")
        stage    (ui/make-stage)
        scene    (Scene. root)
        controls (ui/collect-controls root ["message" "ok"])]
    (observe-focus stage)
    (ui/title! stage "Alert")
    (ui/text! (:message controls) message)
    (ui/on-action! (:ok controls) (fn [_] (.close stage)))

    (.initModality stage Modality/APPLICATION_MODAL)
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-confirm-dialog [message]
  (let [root     ^Parent (ui/load-fxml "confirm.fxml")
        stage    (ui/make-stage)
        scene    (Scene. root)
        controls (ui/collect-controls root ["message" "ok" "cancel"])
        result   (atom false)]
    (observe-focus stage)
    (ui/title! stage "Please confirm")
    (ui/text! (:message controls) message)
    (ui/on-action! (:ok controls) (fn [_]
                                    (reset! result true)
                                    (.close stage)))
    (ui/on-action! (:cancel controls) (fn [_]
                                        (.close stage)))

    (.initModality stage Modality/APPLICATION_MODAL)
    (.setScene stage scene)
    (ui/show-and-wait! stage)
    @result))

(handler/defhandler ::report-error :dialog
  (run [sentry-id-promise]
    (let [sentry-id (deref sentry-id-promise 100 nil)
          fields (cond-> {}
                   sentry-id
                   (assoc "Error" (format "<a href='https://sentry.io/defold/editor2/?query=%s'>%s</a>"
                                          sentry-id sentry-id)))]
      (ui/browse-url (github/new-issue-link fields)))))

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
        stage    (ui/make-stage)
        scene    (Scene. root)
        controls (ui/collect-controls root ["message" "dismiss" "report"])]
    (observe-focus stage)
    (ui/context! root :dialog {:stage stage :sentry-id-promise sentry-id-promise} nil)
    (ui/title! stage "Error")
    (ui/text! (:message controls) (messages ex-map))
    (ui/bind-action! (:dismiss controls) ::close)
    (ui/bind-action! (:report controls) ::report-error)
    (ui/bind-keys! root {KeyCode/ESCAPE ::close})
    (ui/request-focus! (:report controls))
    (.initModality stage Modality/APPLICATION_MODAL)
    (.setResizable stage false)
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-task-dialog [dialog-fxml options]
  (let [root ^Parent (ui/load-fxml "task-dialog.fxml")
        dialog-root ^Parent (ui/load-fxml dialog-fxml)
        stage (ui/make-stage)
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
    (observe-focus stage)
    (ui/text! (:ok controls) (or (:ok-label options) "Ok"))
    (ui/title! stage (or (:title options) ""))
    (ui/children! (:dialog-area controls) [dialog-root])
    (ui/fill-control dialog-root)

    (ui/visible! (:error-group controls) false)
    (ui/managed! (:error-group controls) false)

    (.initModality stage Modality/APPLICATION_MODAL)
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
        ^Stage stage (doto (ui/make-stage)
                       (observe-focus)
                       (.initOwner (ui/main-stage))
                       (.initModality Modality/WINDOW_MODAL)
                       (ui/title! (or (:title options) "Select Item"))
                       (.setScene scene))
        controls (ui/collect-controls root ["filter" "item-list" "ok"])
        ^TextField filter-field (:filter controls)
        filter-value (:filter options "")
        cell-fn (:cell-fn options identity)
        ^ListView item-list (doto (:item-list controls)
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

(defn- text-filter-fn [filter-value items]
  (let [search-str (str/lower-case filter-value)
        parts (-> search-str
                (str/replace #"\*" "")
                (str/split #"\."))
        pattern-str (if (> (count parts) 1)
                      (apply str (concat ["^.*"]
                                         (butlast parts)
                                         [".*\\." (last parts) ".*$"]))
                      (str "^" (str/replace search-str #"\*" ".*")))
                                        pattern (re-pattern pattern-str)]
    (filter (fn [r] (re-find pattern (resource/resource-name r))) items)))

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
                                       (g/node-instance? project/ResourceNode node-id))
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
  (tree-seq (partial g/node-instance? project/ResourceNode) sub-nodes n))

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
                                 (g/node-instance? project/ResourceNode src))
                        (when-let [r (g/node-value src :resource)]
                          (when (resource/exists? r)
                            r)))))
                  (g/inputs n)))
          all))
      distinct)
    []))

(defn make-resource-dialog [workspace project options]
  (let [exts         (let [ext (:ext options)] (if (string? ext) (list ext) (seq ext)))
        accepted-ext (if (seq exts) (set exts) (constantly true))
        items        (filter #(and (= :file (resource/source-type %)) (accepted-ext (:ext (resource/resource-type %))))
                             (g/node-value workspace :resource-list))
        options (-> {:title "Select Resource"
                     :prompt "filter resources - '*' to match any string, '.' to filter file extensions"
                     :filter ""
                     :cell-fn (fn [r] {:text (resource/proj-path r)
                                       :icon (workspace/resource-icon r)
                                       :tooltip (when-let [tooltip-gen (:tooltip-gen options)]
                                                  (tooltip-gen r))})
                     :filter-fn (fn [filter-value items]
                                  (let [fns {"refs" (partial refs-filter-fn project)
                                             "deps" (partial deps-filter-fn project)}
                                        [command arg] (let [parts (str/split filter-value #":")]
                                                        (if (< 1 (count parts))
                                                          parts
                                                          [nil (first parts)]))
                                        f (get fns command text-filter-fn)]
                                    (f arg items)))}
                  (merge options))]
    (make-select-list-dialog items options)))

(defrecord MatchContextResource [parent-resource line caret-position match]
  resource/Resource
  (children [this]      [])
  (ext [this]           (resource/ext parent-resource))
  (resource-name [this] (format "%d: %s" (inc line) match))
  (resource-type [this] (resource/resource-type parent-resource))
  (source-type [this]   (resource/source-type parent-resource))
  (read-only? [this]    (resource/read-only? parent-resource))
  (path [this]          (resource/path parent-resource))
  (abs-path [this]      (resource/abs-path parent-resource))
  (proj-path [this]     (resource/proj-path parent-resource))
  (url [this]           (resource/url parent-resource))
  (workspace [this]     (resource/workspace parent-resource))
  (resource-hash [this] (resource/resource-hash parent-resource)))

(defn- make-match-tree-item [resource {:keys [line caret-position match]}]
  (TreeItem. (->MatchContextResource resource line caret-position match)))

(defn- insert-search-result [^List tree-items search-result]
  (let [{:keys [resource matches]} search-result
        match-items (map (partial make-match-tree-item resource) matches)
        resource-item (TreeItem. resource)]
    (.setExpanded resource-item true)
    (-> resource-item .getChildren (.addAll ^Collection match-items))
    (.add tree-items resource-item)))

(defn- seconds->nanoseconds [seconds]
  (long (* 1000000000 seconds)))

(defn- start-tree-update-timer! [^TreeView tree-view poll-fn]
  (let [tree-items (.getChildren (.getRoot tree-view))
        first-match? (volatile! true)
        timer (ui/->timer "tree-update-timer"
                          (fn [^AnimationTimer timer _dt]
                            (let [start-time (System/nanoTime)
                                  end-time (+ start-time (seconds->nanoseconds (/ 1 90)))]
                              (loop [poll-time 0]
                                (when (> end-time poll-time)
                                  (when-let [entry (poll-fn)]
                                    (if (= ::project-search/done entry)
                                      (.stop timer)
                                      (do (insert-search-result tree-items entry)
                                          (when first-match?
                                            (-> tree-view .getSelectionModel (.clearAndSelect 1))
                                            (vreset! first-match? false))
                                          (recur (System/nanoTime))))))))))]
    (.clear tree-items)
    (ui/timer-start! timer)
    timer))

(defn make-search-in-files-dialog [project]
  (let [root      ^Parent (ui/load-fxml "search-in-files-dialog.fxml")
        stage     (ui/make-stage)
        scene     (Scene. root)
        controls  (ui/collect-controls root ["resources-tree" "ok" "search" "types"])
        return    (atom nil)
        close     (fn [] (reset! return (ui/selection (:resources-tree controls))) (.close stage))
        term-field ^TextField (:search controls)
        exts-field ^TextField (:types controls)
        tree-view ^TreeView (:resources-tree controls)
        start-consumer! (partial start-tree-update-timer! tree-view)
        stop-consumer! ui/timer-stop!
        report-error! (fn [error] (ui/run-later (throw error)))
        file-resource-save-data-future (project-search/make-file-resource-save-data-future report-error! project)
        {:keys [abort-search! start-search!]} (project-search/make-file-searcher file-resource-save-data-future start-consumer! stop-consumer! report-error!)
        on-input-changed! (fn [_ _ _] (start-search! (.getText term-field) (.getText exts-field)))]
    (observe-focus stage)
    (.initOwner stage (ui/main-stage))
    (ui/title! stage "Search in Files")
    (.setShowRoot tree-view false)
    (.setRoot tree-view (doto (TreeItem.)
                          (.setExpanded true)))

    (ui/on-closed! stage (fn on-closed! [_] (abort-search!)))
    (ui/on-action! (:ok controls) (fn on-action! [_] (close)))
    (ui/on-double! (:resources-tree controls) (fn on-double! [_] (close)))

    (ui/cell-factory! (:resources-tree controls) (fn [r] (if (instance? MatchContextResource r)
                                                           {:text (resource/resource-name r)
                                                            :icon (workspace/resource-icon r)}
                                                           {:text (resource/proj-path r)
                                                            :icon (workspace/resource-icon r)})))

    (ui/observe (.textProperty term-field) on-input-changed!)
    (ui/observe (.textProperty exts-field) on-input-changed!)

    (.addEventFilter scene KeyEvent/KEY_PRESSED
      (ui/event-handler event
                        (let [code (.getCode ^KeyEvent event)]
                          (when (cond
                                  (= code KeyCode/DOWN)   (ui/request-focus! (:resources-tree controls))
                                  (= code KeyCode/ESCAPE) true
                                  (= code KeyCode/ENTER)  (do (reset! return (ui/selection (:resources-tree controls)))
                                                              true)
                                  :else                   false)
                            (.close stage)))))

    (.initModality stage Modality/WINDOW_MODAL)
    (.setScene stage scene)
    (ui/show-and-wait-throwing! stage)

    (let [resource (first @return)]
      (cond
        (instance? MatchContextResource resource)
        [(:parent-resource resource) {:caret-position (:caret-position resource)}]

        :else
        [resource {}]))))

(defn make-new-folder-dialog [base-dir]
  (let [root ^Parent (ui/load-fxml "new-folder-dialog.fxml")
        stage (ui/make-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["name" "ok"])
        return (atom nil)
        close (fn [] (reset! return (ui/text (:name controls))) (.close stage))]
    (observe-focus stage)
    (.initOwner stage (ui/main-stage))
    (ui/title! stage "New Folder")

    (ui/on-action! (:ok controls) (fn [_] (close)))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (condp = code
                                                 KeyCode/ENTER (do (reset! return (ui/text (:name controls))) true)
                                                 KeyCode/ESCAPE true
                                                 false)
                                           (.close stage)))))

    (.initModality stage Modality/WINDOW_MODAL)
    (.setScene stage scene)
    (ui/show-and-wait! stage)

    @return))

(defn make-target-ip-dialog []
  (let [root     ^Parent (ui/load-fxml "target-ip-dialog.fxml")
        stage    (ui/make-stage)
        scene    (Scene. root)
        controls (ui/collect-controls root ["add" "cancel" "ip"])
        return   (atom nil)]
    (observe-focus stage)
    (.initOwner stage (ui/main-stage))
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

    (.initModality stage Modality/WINDOW_MODAL)
    (.setScene stage scene)
    (ui/show-and-wait! stage)

    @return))

(defn make-rename-dialog [title label placeholder typ]
  (let [root     ^Parent (ui/load-fxml "rename-dialog.fxml")
        stage    (ui/make-stage)
        scene    (Scene. root)
        controls (ui/collect-controls root ["name" "path" "ok" "name-label"])
        return   (atom nil)
        close    (fn [] (reset! return (ui/text (:name controls))) (.close stage))
        full-name (fn [^String n]
                    (-> n
                        (str/replace #"/" "")
                        (str/replace #"\\" "")
                        (str (when typ (str "." typ)))))]
    (observe-focus stage)
    (.initOwner stage (ui/main-stage))

    (ui/title! stage title)
    (when label
      (ui/text! (:name-label controls) label))
    (when-not (empty? placeholder)
      (ui/text! (:path controls) (full-name placeholder))
      (ui/text! (:name controls) placeholder)
      (.selectAll ^TextField (:name controls)))

    (ui/on-action! (:ok controls) (fn [_] (close)))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
                     (ui/event-handler event
                                       (let [code (.getCode ^KeyEvent event)]
                                         (when (condp = code
                                                 KeyCode/ENTER  (do (reset! return
                                                                            (when-let [txt (not-empty (ui/text (:name controls)))]
                                                                              (full-name txt)))
                                                                    true)
                                                 KeyCode/ESCAPE true
                                                 false)
                                           (.close stage)))))
    (.addEventFilter scene KeyEvent/KEY_RELEASED
                     (ui/event-handler event
                                       (if-let [txt (not-empty (ui/text (:name controls)))]
                                         (ui/text! (:path controls) (full-name txt))
                                         (ui/text! (:path controls) ""))))

    (.initModality stage Modality/WINDOW_MODAL)
    (.setScene stage scene)
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
        stage (ui/make-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["name" "location" "browse" "path" "ok"])
        return (atom nil)
        close (fn [perform?]
                (when perform?
                  (reset! return (File. base-dir (ui/text (:path controls)))))
                (.close stage))
        set-location (fn [location] (ui/text! (:location controls) (relativize base-dir location)))]
    (observe-focus stage)
    (.initOwner stage (ui/main-stage))
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

    (.initModality stage Modality/WINDOW_MODAL)
    (.setScene stage scene)
    (ui/show-and-wait! stage)

    @return))

(defn make-goto-line-dialog [result]
  (let [root ^Parent (ui/load-fxml "goto-line-dialog.fxml")
        stage (ui/make-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["line"])
        close (fn [v] (do (deliver result v) (.close stage)))]
    (observe-focus stage)
    (.initOwner stage (ui/main-stage))
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
    (.initModality stage Modality/NONE)
    (.setScene stage scene)
    (ui/show! stage)
    stage))

(defn make-find-text-dialog [result]
  (let [root ^Parent (ui/load-fxml "find-text-dialog.fxml")
        stage (ui/make-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["text"])
        close (fn [v] (do (deliver result v) (.close stage)))]
    (observe-focus stage)
    (.initOwner stage (ui/main-stage))
    (ui/title! stage "Find Text")
    (.setOnKeyPressed scene
                      (ui/event-handler e
                           (let [key (.getCode ^KeyEvent e)]
                             (when (= key KeyCode/ENTER)
                               (close (ui/text (:text controls))))
                             (when (= key KeyCode/ESCAPE)
                               (close nil)))))
    (.initModality stage Modality/NONE)
    (.setScene stage scene)
    (ui/show! stage)
    stage))

(defn make-replace-text-dialog [result]
  (let [root ^Parent (ui/load-fxml "replace-text-dialog.fxml")
        stage (ui/make-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["find-text" "replace-text"])
        close (fn [v] (do (deliver result v) (.close stage)))]
    (observe-focus stage)
    (.initOwner stage (ui/main-stage))
    (ui/title! stage "Find/Replace Text")
    (.setOnKeyPressed scene
                      (ui/event-handler e
                           (let [key (.getCode ^KeyEvent e)]
                             (when (= key KeyCode/ENTER)
                               (close {:find-text (ui/text (:find-text controls))
                                       :replace-text (ui/text (:replace-text controls))}))
                             (when (= key KeyCode/ESCAPE)
                               (close nil)))))
    (.initModality stage Modality/NONE)
    (.setScene stage scene)
    (ui/show! stage)
    stage))

(defn make-proposal-dialog [result screen-point proposals target text-area]
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
    (observe-focus stage)
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

(handler/defhandler ::replace-conflicting-files :dialog
  (run [^Stage stage]
    (ui/user-data! stage ::file-conflict-resolution-strategy :replace)
    (ui/close! stage)))

(defn make-resolve-file-conflicts-dialog
  [src-dest-pairs]
  (let [^Parent root (ui/load-fxml "resolve-file-conflicts.fxml")
        scene (Scene. root)
        ^Stage stage (doto (ui/make-stage)
                       (observe-focus)
                       (.initOwner (ui/main-stage))
                       (.initModality Modality/WINDOW_MODAL)
                       (ui/title! "Resolve file name conflicts")
                       (.setScene scene))
        controls (ui/collect-controls root ["message" "rename" "replace" "cancel"])]
    (ui/context! root :dialog {:stage stage} nil)
    (ui/bind-action! (:rename controls) ::rename-conflicting-files)
    (ui/bind-action! (:replace controls) ::replace-conflicting-files)
    (ui/bind-action! (:cancel controls) ::close)
    (ui/bind-keys! root {KeyCode/ESCAPE ::close})
    (ui/text! (:message controls) (format "The destination has %d file(s) with the same names" (count src-dest-pairs)))
    (ui/show-and-wait! stage)
    (ui/user-data stage ::file-conflict-resolution-strategy)))
