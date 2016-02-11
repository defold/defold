(ns editor.dialogs
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [service.log :as log]
            [editor.defold-project :as project])
  (:import [java.io File]
           [java.nio.file Path Paths]
           [javafx.beans.binding StringBinding]
           [javafx.event ActionEvent EventHandler]
           [javafx.collections FXCollections ObservableList]
           [javafx.fxml FXMLLoader]
           [javafx.scene Parent Scene]
           [javafx.scene.control Button ProgressBar TextField TreeView TreeItem ListView SelectionMode]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.input KeyEvent]
           [javafx.scene.web WebView]
           [javafx.stage Stage Modality DirectoryChooser]
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
  (let [root ^Parent (ui/load-fxml "resource-dialog.fxml")
        stage (Stage.)
        scene (Scene. root)
        controls (ui/collect-controls root ["resources" "ok" "search"])
        return (atom nil)
        exts (let [ext (:ext options)] (if (string? ext) (list ext) (seq ext)))
        accepted-ext (if (seq exts) (set exts) (constantly true))
        items (filter #(and (= :file (resource/source-type %)) (accepted-ext (:ext (resource/resource-type %)))) (g/node-value workspace :resource-list))
        close (fn [] (reset! return (ui/selection (:resources controls))) (.close stage))]

    (.initOwner stage (ui/main-stage))
    (ui/title! stage (or (:title options) "Select Resource"))
    (ui/items! (:resources controls) items)

    (when (= (:selection options) :multiple)
      (-> ^ListView (:resources controls)
          (.getSelectionModel)
          (.setSelectionMode SelectionMode/MULTIPLE)))

    (ui/cell-factory! (:resources controls) (fn [r] {:text (resource/resource-name r)
                                                    :icon (workspace/resource-icon r)
                                                    :tooltip (when-let [tooltip-gen (:tooltip-gen options)]
                                                               (tooltip-gen r))}))

    (ui/on-action! (:ok controls) (fn [_] (close)))
    (ui/on-double! (:resources controls) (fn [_] (close)))

    (ui/observe (.textProperty ^TextField (:search controls))
                (fn [_ _ ^String new] (let [pattern-str (str "^" (.replaceAll new "\\*" ".\\*?"))
                                    pattern (re-pattern pattern-str)
                                    filtered-items (filter (fn [r] (re-find pattern (resource/resource-name r))) items)]
                                (ui/items! (:resources controls) filtered-items))))

    (.addEventFilter scene KeyEvent/KEY_PRESSED
      (ui/event-handler event
                        (let [code (.getCode ^KeyEvent event)]
                          (when (cond
                                  (= code KeyCode/DOWN) (ui/request-focus! (:resources controls))
                                  (= code KeyCode/ESCAPE) true
                                  (= code KeyCode/ENTER) (do (reset! return (ui/selection (:resources controls))) true)
                                  true false)
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

(defn- update-search-dialog [^TreeView tree-view workspace project exts term]
  (let [matching-resources (project/search-in-files project exts term)
        resource-tree      (g/node-value workspace :resource-tree)
        [_ new-tree]       (workspace/filter-resource-tree resource-tree (set (map :resource matching-resources)))
        tree-with-hits     (append-match-snippet-nodes new-tree (group-by :resource matching-resources))]
    (update-tree-view tree-view tree-with-hits)
    (doseq [^TreeItem item (ui/tree-item-seq (.getRoot tree-view))]
      (.setExpanded item true))
    (let [first-match (->> (ui/tree-item-seq (.getRoot tree-view))
                           (filter (fn [^TreeItem item]
                                     (instance? MatchContextResource (.getValue item))))
                           first)]
      (.select (.getSelectionModel tree-view) first-match))))

(defn make-search-in-files-dialog [workspace project]
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
                  (update-search-dialog tree-view workspace project @exts @term)))

    (ui/observe (.textProperty ^TextField (:types controls))
                (fn observe [_ _ ^String new]
                  (reset! exts new)
                  (update-search-dialog tree-view workspace project @exts @term)))

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
