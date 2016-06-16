(ns editor.dialogs
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [service.log :as log]
            [clojure.string :as str])
  (:import [java.io File]
           [java.nio.file Path Paths]
           [javafx.beans.binding StringBinding]
           [javafx.event ActionEvent EventHandler]
           [javafx.collections FXCollections ObservableList]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Point2D]
           [javafx.scene Parent Scene]
           [javafx.scene.control Button ProgressBar TextField TreeView TreeItem ListView SelectionMode]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.input KeyEvent]
           [javafx.scene.web WebView]
           [javafx.stage Stage StageStyle Modality DirectoryChooser]
           [org.apache.commons.io FilenameUtils]))

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

(defn make-alert-dialog [message]
  (let [root     ^Parent (ui/load-fxml "alert.fxml")
        stage    (Stage.)
        scene    (Scene. root)
        controls (ui/collect-controls root ["message" "ok"])]
    (ui/title! stage "Alert")
    (ui/text! (:message controls) message)
    (ui/on-action! (:ok controls) (fn [_] (.close stage)))

    (.initModality stage Modality/APPLICATION_MODAL)
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-confirm-dialog [message]
  (let [root     ^Parent (ui/load-fxml "confirm.fxml")
        stage    (Stage.)
        scene    (Scene. root)
        controls (ui/collect-controls root ["message" "ok" "cancel"])
        result   (atom false)]
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

(defn make-task-dialog [dialog-fxml options]
  (let [root ^Parent (ui/load-fxml "task-dialog.fxml")
        dialog-root ^Parent (ui/load-fxml dialog-fxml)
        stage (Stage.)
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

(defn make-resource-dialog [workspace options]
  (let [root         ^Parent (ui/load-fxml "resource-dialog.fxml")
        stage        (Stage.)
        scene        (Scene. root)
        controls     (ui/collect-controls root ["resources" "ok" "search"])
        return       (atom nil)
        exts         (let [ext (:ext options)] (if (string? ext) (list ext) (seq ext)))
        accepted-ext (if (seq exts) (set exts) (constantly true))
        items        (filter #(and (= :file (resource/source-type %)) (accepted-ext (:ext (resource/resource-type %))))
                             (g/node-value workspace :resource-list))
        close        (fn [] (reset! return (ui/selection (:resources controls))) (.close stage))]

    (.initOwner stage (ui/main-stage))
    (ui/title! stage (or (:title options) "Select Resource"))
    (ui/items! (:resources controls) items)

    (when (= (:selection options) :multiple)
      (-> ^ListView (:resources controls)
          (.getSelectionModel)
          (.setSelectionMode SelectionMode/MULTIPLE)))

    (ui/cell-factory! (:resources controls) (fn [r] {:text    (resource/proj-path r)
                                                     :icon    (workspace/resource-icon r)
                                                     :tooltip (when-let [tooltip-gen (:tooltip-gen options)]
                                                                (tooltip-gen r))}))

    (ui/on-action! (:ok controls) (fn [_] (close)))
    (ui/on-double! (:resources controls) (fn [_] (close)))

    (ui/observe (.textProperty ^TextField (:search controls))
                (fn [_ _ ^String new]
                  (let [search-str     (str/lower-case new)
                        parts          (-> search-str
                                           (str/replace #"\*" "")
                                           (str/split #"\."))
                        pattern-str    (if (> (count parts) 1)
                                         (apply str (concat ["^.*"]
                                                            (butlast parts)
                                                            [".*\\." (last parts) ".*$"]))
                                         (str "^" (str/replace search-str #"\*" ".*")))
                        pattern        (re-pattern pattern-str)
                        filtered-items (filter (fn [r] (re-find pattern (resource/resource-name r))) items)
                        list-view      ^ListView (:resources controls)]
                    (ui/items! list-view filtered-items)
                    (when-let [first-match (first filtered-items)]
                      (.select (.getSelectionModel list-view) first-match)))))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
      (ui/event-handler event
                        (let [code (.getCode ^KeyEvent event)]
                          (when (cond
                                  (= code KeyCode/DOWN)   (ui/request-focus! (:resources controls))
                                  (= code KeyCode/ESCAPE) true
                                  (= code KeyCode/ENTER)  (do (reset! return (ui/selection (:resources controls)))
                                                              true)
                                  true                    false)
                            (.close stage)))))

    (.initModality stage Modality/WINDOW_MODAL)
    (.setScene stage scene)
    (ui/show-and-wait! stage)

    @return))

(declare tree-item)

(defn- ^ObservableList list-children [parent]
  (let [children (:children parent)
        items    (into-array TreeItem (mapv tree-item children))]
    (if (empty? children)
      (FXCollections/emptyObservableList)
      (doto (FXCollections/observableArrayList)
        (.addAll ^"[Ljavafx.scene.control.TreeItem;" items)))))

(defn- tree-item [parent]
  (let [cached (atom false)]
    (proxy [TreeItem] [parent]
      (isLeaf []
        (empty? (:children (.getValue ^TreeItem this))))
      (getChildren []
        (let [this ^TreeItem this
              ^ObservableList children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children (.getValue this))))
          children)))))

(defn- update-tree-view [^TreeView tree-view resource-tree]
  (let [root (.getRoot tree-view)
        ^TreeItem new-root (tree-item resource-tree)]
    (if (.getValue new-root)
      (.setRoot tree-view new-root)
      (.setRoot tree-view nil))
    tree-view))

(defrecord MatchContextResource [parent-resource line caret-position match]
  resource/Resource
  (children [this]      [])
  (resource-name [this] (format "%d: %s" line match))
  (resource-type [this] (resource/resource-type parent-resource))
  (source-type [this]   (resource/source-type parent-resource))
  (read-only? [this]    (resource/read-only? parent-resource))
  (path [this]          (resource/path parent-resource))
  (abs-path [this]      (resource/abs-path parent-resource))
  (proj-path [this]     (resource/proj-path parent-resource))
  (url [this]           (resource/url parent-resource))
  (workspace [this]     (resource/workspace parent-resource))
  (resource-hash [this] (resource/resource-hash parent-resource)))

(defn- append-match-snippet-nodes [tree matching-resources]
  (when tree
    (if (empty? (:children tree))
      (assoc tree :children (map #(->MatchContextResource tree (:line %) (:caret-position %) (:match %))
                                 (:matches (first (get matching-resources tree)))))
      (update tree :children (fn [children]
                               (map #(append-match-snippet-nodes % matching-resources) children))))))

(defn- update-search-dialog [^TreeView tree-view workspace matching-resources]
  (let [resource-tree  (g/node-value workspace :resource-tree)
        [_ new-tree]   (workspace/filter-resource-tree resource-tree (set (map :resource matching-resources)))
        tree-with-hits (append-match-snippet-nodes new-tree (group-by :resource matching-resources))]
    (update-tree-view tree-view tree-with-hits)
    (doseq [^TreeItem item (ui/tree-item-seq (.getRoot tree-view))]
      (.setExpanded item true))
    (when-let [first-match (->> (ui/tree-item-seq (.getRoot tree-view))
                                (filter (fn [^TreeItem item]
                                          (instance? MatchContextResource (.getValue item))))
                                first)]
      (.select (.getSelectionModel tree-view) first-match))))

(defn make-search-in-files-dialog [workspace search-fn]
  (let [root      ^Parent (ui/load-fxml "search-in-files-dialog.fxml")
        stage     (Stage.)
        scene     (Scene. root)
        controls  (ui/collect-controls root ["resources-tree" "ok" "search" "types"])
        return    (atom nil)
        term      (atom nil)
        exts      (atom nil)
        close     (fn [] (reset! return (ui/selection (:resources-tree controls))) (.close stage))
        tree-view ^TreeView (:resources-tree controls)]

    (.initOwner stage (ui/main-stage))
    (ui/title! stage "Search in files")

    (ui/on-action! (:ok controls) (fn on-action! [_] (close)))
    (ui/on-double! (:resources-tree controls) (fn on-double! [_] (close)))

    (ui/cell-factory! (:resources-tree controls) (fn [r] {:text (resource/resource-name r)
                                                          :icon (workspace/resource-icon r)}))

    (ui/observe (.textProperty ^TextField (:search controls))
                (fn observe [_ _ ^String new]
                  (reset! term new)
                  (update-search-dialog tree-view workspace (search-fn @exts @term))))

    (ui/observe (.textProperty ^TextField (:types controls))
                (fn observe [_ _ ^String new]
                  (reset! exts new)
                  (update-search-dialog tree-view workspace (search-fn @exts @term))))

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
    (ui/show-and-wait! stage)

    (let [resource (and @return
                        (update @return :children
                                (fn [children] (remove #(instance? MatchContextResource %) children))))]
      (cond
        (instance? MatchContextResource resource)
        [(:parent-resource resource) {:caret-position (:caret-position resource)}]

        (not-empty (:children resource))
        nil

        :else
        [resource {}]))))

(defn make-new-folder-dialog [base-dir]
  (let [root ^Parent (ui/load-fxml "new-folder-dialog.fxml")
        stage (Stage.)
        scene (Scene. root)
        controls (ui/collect-controls root ["name" "ok"])
        return (atom nil)
        close (fn [] (reset! return (ui/text (:name controls))) (.close stage))]
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

(defn make-rename-dialog [title label placeholder typ]
  (let [root     ^Parent (ui/load-fxml "rename-dialog.fxml")
        stage    (Stage.)
        scene    (Scene. root)
        controls (ui/collect-controls root ["name" "path" "ok" "name-label"])
        return   (atom nil)
        close    (fn [] (reset! return (ui/text (:name controls))) (.close stage))
        full-name (fn [^String n]
                    (-> n
                        (str/replace #"/" "")
                        (str/replace #"\\" "")
                        (str (when typ (str "." typ)))))]
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
        stage (Stage.)
        scene (Scene. root)
        controls (ui/collect-controls root ["name" "location" "browse" "path" "ok"])
        return (atom nil)
        close (fn [perform?]
                (when perform?
                  (reset! return (File. base-dir (ui/text (:path controls)))))
                (.close stage))
        set-location (fn [location] (ui/text! (:location controls) (relativize base-dir location)))]
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
        stage (Stage.)
        scene (Scene. root)
        controls (ui/collect-controls root ["line"])
        close (fn [v] (do (deliver result v) (.close stage)))]
    (.initOwner stage (ui/main-stage))
    (ui/title! stage "Go to line")
    (.setOnKeyPressed scene
                      (ui/event-handler e
                           (let [key (.getCode ^KeyEvent e)]
                             (when (= key KeyCode/ENTER)
                               (close (ui/text (:line controls))))
                             (when (= key KeyCode/ESCAPE)
                               (close nil)))))
    (.initModality stage Modality/NONE)
    (.setScene stage scene)
    (ui/show! stage)
    stage))

(defn make-find-text-dialog [result]
  (let [root ^Parent (ui/load-fxml "find-text-dialog.fxml")
        stage (Stage.)
        scene (Scene. root)
        controls (ui/collect-controls root ["text"])
        close (fn [v] (do (deliver result v) (.close stage)))]
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
        stage (Stage.)
        scene (Scene. root)
        controls (ui/collect-controls root ["find-text" "replace-text"])
        close (fn [v] (do (deliver result v) (.close stage)))]
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

(defn make-proposal-dialog [result caret screen-point proposals line]
  (let [root ^Parent (ui/load-fxml "text-proposals.fxml")
        stage (Stage.)
        scene (Scene. root)
        controls (ui/collect-controls root ["proposals" "proposals-box"])
        close (fn [v] (do (deliver result v) (.close stage)))
        ^ListView list-view  (:proposals controls)
        filter-text (atom line)
        filter-fn (fn [i] (string/starts-with? (:name i) @filter-text))
        update-items (fn [] (try
                              (ui/items! list-view (filter filter-fn proposals))
                              (.select (.getSelectionModel list-view) 0)
                              (catch Exception e (do
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
                                           (= code (KeyCode/SHIFT)) true

                                           (= code (KeyCode/BACK_SPACE))
                                           (if (empty? @filter-text)
                                             (close nil)
                                             (do
                                               (swap! filter-text #(apply str (drop-last %)))
                                               (update-items)))

                                           (or (.isLetterKey code) (.isDigitKey code) (= code (KeyCode/MINUS)) (= code (KeyCode/MINUS)))
                                           (do
                                             (swap! filter-text str (.getText ^KeyEvent event))
                                             (update-items))

                                           :default (close nil)))))

    (.initOwner stage (ui/main-stage))
    (.initModality stage Modality/WINDOW_MODAL)
    (.setScene stage scene)
    (ui/show! stage)
    stage))
