(ns editor.ui
  (:require [clojure.java.io :as io]
            [editor.workspace :as workspace]
            [editor.jfx :as jfx]
            [editor.handler :as handler]
            [service.log :as log])
  (:import [javafx.beans.value ChangeListener ObservableValue]
           [javafx.scene Parent Node Scene Group]
           [javafx.stage Stage Modality Window]
           [javafx.scene.control ButtonBase ComboBox Control ContextMenu SeparatorMenuItem Label Labeled ListView ListCell ToggleButton TextInputControl TreeView TreeItem Toggle Menu MenuBar MenuItem ProgressBar]
           [javafx.scene.layout AnchorPane Pane]
           [javafx.stage DirectoryChooser FileChooser FileChooser$ExtensionFilter]
           [javafx.application Platform]
           [javafx.fxml FXMLLoader]
           [javafx.util Callback]
           [javafx.event ActionEvent EventHandler]
           [javafx.scene.input KeyCombination ContextMenuEvent MouseEvent]))

(set! *warn-on-reflection* true)

(defonce ^:dynamic *menus* (atom {}))
(defonce ^:dynamic *main-stage* (atom nil))

; NOTE: This one might change from welcome to actual project window
(defn set-main-stage [main-stage]
  (reset! *main-stage* main-stage))

(defn main-stage []
  @*main-stage*)

(defn choose-file [title ^String ext-descr exts]
  (let [chooser (FileChooser.)
        ext-array (into-array exts)]
    (.setTitle chooser title)
    (.add (.getExtensionFilters chooser) (FileChooser$ExtensionFilter. ext-descr ^"[Ljava.lang.String;" ext-array))
    (let [file (.showOpenDialog chooser nil)]
      (if file (.getAbsolutePath file)))))

(defn choose-directory
  ([title] (choose-directory title @*main-stage*))
  ([title parent]
    (let [chooser (DirectoryChooser.)]
      (.setTitle chooser title)
      (let [file (.showDialog chooser parent)]
        (when file (.getAbsolutePath file))))))

(defn collect-controls [^Parent root keys]
  (let [controls (zipmap (map keyword keys) (map #(.lookup root (str "#" %)) keys))
        missing (->> controls
                  (filter (fn [[k v]] (when (nil? v) k)))
                  (map first))]
    (when (seq missing)
      (throw (Exception. (format "controls %s are missing" missing))))
    controls))

; TODO: Better name?
(defn fill-control [control]
  (AnchorPane/setTopAnchor control 0.0)
  (AnchorPane/setBottomAnchor control 0.0)
  (AnchorPane/setLeftAnchor control 0.0)
  (AnchorPane/setRightAnchor control 0.0))

(defn local-bounds [^Node node]
  (let [b (.getBoundsInLocal node)]
    {:minx (.getMinX b)
     :miny (.getMinY b)
     :maxx (.getMaxX b)
     :maxy (.getMaxY b)
     :width (.getWidth b)
     :height (.getHeight b)}))

(defn observe [^ObservableValue observable listen-fn]
  (.addListener observable (reify ChangeListener
                        (changed [this observable old new]
                          (listen-fn observable old new)))))

(defn do-run-now [f]
  (if (Platform/isFxApplicationThread)
    (f)
    (let [p (promise)]
      (Platform/runLater
        (fn []
          (try
            (deliver p (f))
            (catch Throwable e
              (deliver p e)))))
      (let [val @p]
        (if (instance? Throwable val)
          (throw val)
          val)))))

(defmacro run-now
  [& body]
  `(do-run-now
    (fn [] ~@body)))

(defmacro run-later
  [& body]
  `(Platform/runLater
    (fn [] ~@body)))

(defn user-data! [^Node node key val]
  (let [ud (or (.getUserData node) {})]
    (.setUserData node (assoc ud key val))))

(defn user-data [^Node node key]
  (get (.getUserData node) key))

(defmacro event-handler [event & body]
  `(reify EventHandler
     (handle [this ~event]
       ~@body)))

(defn scene [^Node node]
  (.getScene node))

(defn add-style! [^Node node ^String class]
  (.add (.getStyleClass node) class))

(defn visible! [^Node node v]
  (.setVisible node v))

(defn managed! [^Node node m]
  (.setManaged node m))

(defn disable! [^Node node d]
  (.setDisable node d))

(defn window [^Scene scene]
  (.getWindow scene))

(defn close! [^Stage window]
  (.close window))

(defn title! [^Stage window t]
  (.setTitle window t))

(defn show! [^Stage stage]
  (.show stage))

(defn show-and-wait! [^Stage stage]
  (.showAndWait stage))

(defn request-focus! [^Node node]
  (.requestFocus node))

(defn on-double! [^Node node fn]
  (.setOnMouseClicked node (event-handler e (when (= 2 (.getClickCount ^MouseEvent e))
                                        (fn e)))))

(defprotocol Text
  (text [this])
  (text! [this val]))

(defprotocol HasAction
  (on-action! [this fn]))

(defprotocol HasChildren
  (children! [this c]))

(defprotocol CollectionView
  (selection [this])
  (items [this])
  (items! [this items])
  (cell-factory! [this render-fn]))

(extend-type TextInputControl
  Text
  (text [this] (.getText this))
  (text! [this val] (.setText this val)))

(extend-type TextInputControl
  HasAction
  (on-action! [this fn] (.setOnAction this (event-handler e (fn e)))))

(extend-type Labeled
  Text
  (text [this] (.getText this))
  (text! [this val] (.setText this val)))

(extend-type Pane
  HasChildren
  (children! [this c]
    (doto
      (.getChildren this)
      (.clear)
      (.addAll ^"[Ljavafx.scene.Node;" (into-array Node c)))))

(extend-type Group
  HasChildren
  (children! [this c]
    (doto
      (.getChildren this)
      (.clear)
      (.addAll ^"[Ljavafx.scene.Node;" (into-array Node c)))))

(defn- make-list-cell [render-fn]
  (proxy [ListCell] []
    (updateItem [object empty]
      (let [this ^ListCell this
            render-data (and object (render-fn object))]
        (proxy-super updateItem (and object (:text render-data)) empty)
        (let [name (or (and (not empty) (:text render-data)) nil)]
          (proxy-super setText name))
        (proxy-super setGraphic (jfx/get-image-view (:icon render-data)))))))

(defn- make-list-cell-factory [render-fn]
  (reify Callback (call ^ListCell [this view] (make-list-cell render-fn))))

(extend-type ButtonBase
  HasAction
  (on-action! [this fn] (.setOnAction this (event-handler e (fn e)))))

(extend-type ComboBox
  CollectionView
  (selection [this] (when-let [item (.getSelectedItem (.getSelectionModel this))] [item]))
  (items [this] (.getItems this))
  (items! [this ^java.util.Collection items] (let [l (.getItems this)]
                                               (.clear l)
                                               (.addAll l items)))
  (cell-factory! [this render-fn]
    (.setButtonCell this (make-list-cell render-fn))
    (.setCellFactory this (make-list-cell-factory render-fn))))

(extend-type ListView
  CollectionView
  (selection [this] (when-let [items (.getSelectedItems (.getSelectionModel this))] items))
  (items [this] (.getItems this))
  (items! [this ^java.util.Collection items] (let [l (.getItems this)]
                                               (.clear l)
                                               (.addAll l items)))
  (cell-factory! [this render-fn]
    (.setCellFactory this (make-list-cell-factory render-fn))))

(extend-type TreeView
  workspace/SelectionProvider
  (selection [this] (->> this
                      (.getSelectionModel)
                      (.getSelectedItems)
                      (map #(.getValue ^TreeItem %)))))

(extend-type ListView
  workspace/SelectionProvider
  (selection [this] (->> this
                      (.getSelectionModel)
                      (.getSelectedItems)
                      (vec))))

(defn extend-menu [id location menu]
  (swap! *menus* assoc id {:location location
                           :menu menu}))

(defn- collect-menu-extensions []
  (->>
    (vals @*menus*)
    (filter :location)
    (reduce (fn [acc x] (update-in acc [(:location x)] concat (:menu x))) {})))

(defn- do-realize-menu [menu exts]
  (->> menu
     (map (fn [x] (if (:children x)
                    (update-in x [:children] do-realize-menu exts)
                    x)))
     (mapcat (fn [x] (concat [x] (get exts (:id x)))))))

(defn- realize-menu [id]
  (let [exts (collect-menu-extensions)]
    (do-realize-menu (:menu (get @*menus* id)) exts)))

(def ^:private make-menu nil)

(defn- make-desc [control menu-id command-context selection-provider]
  {:control control
   :menu-id menu-id
   :command-context command-context
   :selection-provider selection-provider})

(defn- make-command-context [desc]
  (assoc (:command-context desc) :selection (workspace/selection (:selection-provider desc))))

(defn- create-menu-item [desc item]
  (if (:children item)
    (let [menu (Menu. (:label item))]
      (doseq [i (make-menu desc (:children item))]
        (.add (.getItems menu) i))
      menu)
    (let [menu-item (MenuItem. (:label item))
          command (:command item)]
      (when command
        (.setId menu-item (name command)))
      (when (:acc item)
        (.setAccelerator menu-item (KeyCombination/keyCombination (:acc item))))
      (when (:icon item) (.setGraphic menu-item (jfx/get-image-view (:icon item))))
      (.setDisable menu-item (not (handler/enabled? command (make-command-context desc))))
      (.setOnAction menu-item (event-handler event (handler/run command (make-command-context desc)) ))
      menu-item)))

(defn- ^MenuItem make-menu [desc menu]
  (let [command-context (make-command-context desc)]
    (->> menu
      (mapv (fn [item] (if (= :separator (:label item))
                         (SeparatorMenuItem.)
                         (create-menu-item desc item)))))))

(defn- populate-context-menu [^ContextMenu context-menu desc menu]
  (let [items (make-menu desc menu)]
    (.addAll (.getItems context-menu) (to-array items))))

(defn register-context-menu [^Control control command-context selection-provider menu-id]
  (let [desc (make-desc control menu-id command-context selection-provider)]
    (.addEventHandler control ContextMenuEvent/CONTEXT_MENU_REQUESTED
      (event-handler event
                     (when-not (.isConsumed event)
                       (let [^ContextMenu cm (ContextMenu.) ]
                         (populate-context-menu cm desc (realize-menu menu-id))
                         ; Required for autohide to work when the event originates from the anchor/source control
                         ; See RT-15160 and Control.java
                         (.setImpl_showRelativeToWindow cm true)
                         (.show cm control (.getScreenX ^ContextMenuEvent event) (.getScreenY ^ContextMenuEvent event))
                         (.consume event)))))))

(defn register-menubar [^Scene scene command-context selection-provider menubar-id menu-id ]
  ; TODO: See comment below about top-level items. Should be enforced here
 (let [root (.getRoot scene)]
   (if-let [menubar (.lookup root menubar-id)]
     (let [desc (make-desc menubar menu-id command-context selection-provider)]
       (user-data! root ::menubar desc))
     (log/warn :message (format "menubar %s not found" menubar-id)))))

(defn- refresh-menubar [md]
 (let [menu (realize-menu (:menu-id md))
       control ^MenuBar (:control md)]
   (when-not (= menu (user-data control ::menu))
     (.clear (.getMenus control))
     ; TODO: We must ensure that top-level element are of type Menu and note MenuItem here, i.e. top-level items with ":children"
     (.addAll (.getMenus control) (to-array (make-menu md menu)))
     (user-data! control ::menu menu))))

(defn- refresh-menu-state [^Menu menu command-context]
  (doseq [m (.getItems menu)]
    (if (instance? Menu m)
      (refresh-menu-state m command-context)
      (.setDisable ^Menu m (not (handler/enabled? (keyword (.getId ^Menu m)) command-context))))))

(defn- refresh-menubar-state [^MenuBar menubar command-context]
  (doseq [m (.getMenus menubar)]
    (refresh-menu-state m command-context)))

(defn register-toolbar [^Scene scene command-context selection-provider toolbar-id menu-id ]
  (let [root (.getRoot scene)]
    (if-let [toolbar (.lookup root toolbar-id)]
      (let [desc (make-desc toolbar menu-id command-context selection-provider)]
        (user-data! root ::toolbars (assoc-in (user-data root ::toolbars) [toolbar-id] desc)))
      (log/warn :message (format "toolbar %s not found" toolbar-id)))))

(defn- refresh-toolbar [td]
 (let [menu (realize-menu (:menu-id td))
       ^Pane control (:control td)]
   (when-not (= menu (user-data control ::menu))
     (.clear (.getChildren control))
     (user-data! control ::menu menu)
     (doseq [menu-item menu]
       (let [button (ToggleButton. (:label menu-item))
             icon (:icon menu-item)
             selection-provider (:selection-provider td)]
         (when icon
           (.setGraphic button (jfx/get-image-view icon)))
         (when (:command menu-item)
           (.setId button (name (:command menu-item)))
           (.setOnAction button (event-handler event (handler/run (:command menu-item)
                                                                  (make-command-context td)))))
         (.add (.getChildren control) button))))))

(defn- refresh-toolbar-state [^Pane toolbar command-context]
  (let [nodes (.getChildren toolbar)
        ids (map #(.getId ^Node %) nodes)]
    (doseq [^Node n nodes]
      (.setDisable n (not (handler/enabled? (keyword (.getId n)) command-context)))
      (when (instance? ToggleButton n)
        (if (handler/state (keyword (.getId n)) command-context)
         (.setSelected ^Toggle n true)
         (.setSelected ^Toggle n false))))))

(defn refresh [^Scene scene]
 (let [root (.getRoot scene)
       toolbar-descs (vals (user-data root ::toolbars))]
   (when-let [md (user-data root ::menubar)]
     (refresh-menubar md)
     (refresh-menubar-state (:control md) (make-command-context md)))
   (doseq [td toolbar-descs]
     (refresh-toolbar td)
     (refresh-toolbar-state (:control td) (make-command-context td)))))

(defn modal-progress [title total-work worker-fn]
  (run-now
    (let [root ^Parent (FXMLLoader/load (io/resource "progress.fxml"))
          stage (Stage.)
          scene (Scene. root)
          title-control ^Label (.lookup root "#title")
          progress-control ^ProgressBar (.lookup root "#progress")
          message-control ^Label (.lookup root "#message")
          worked (atom 0)
          return (atom nil)
          report-fn (fn [work msg]
                      (when (> work 0)
                        (swap! worked + work))
                      (run-later
                        (.setText message-control (str msg))
                        (if (> work 0)
                          (.setProgress progress-control (/ @worked (float total-work)))
                          (.setProgress progress-control -1))))]
      (.setText title-control title)
      (.setProgress progress-control 0)
      (.initOwner stage @*main-stage*)
      (.initModality stage Modality/WINDOW_MODAL)
      (.setScene stage scene)
      (future
        (try
          (reset! return (worker-fn report-fn))
          (catch Throwable e
            (reset! return e)))
        (run-later (.close stage)))
      (.showAndWait stage)
      (if (instance? Throwable @return)
          (throw @return)
          @return))))
