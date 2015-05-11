(ns editor.ui
  (:require [clojure.java.io :as io]
            [editor.workspace :as workspace]
            [editor.jfx :as jfx]
            [editor.handler :as handler]
            [service.log :as log])
  (:import [javafx.scene Node Scene]
           [javafx.stage Stage Modality]
           [javafx.scene.control ButtonBase Control ContextMenu SeparatorMenuItem ToggleButton TreeView TreeItem Menu MenuBar MenuItem]
           [javafx.stage FileChooser FileChooser$ExtensionFilter]
           [javafx.application Platform]
           [javafx.fxml FXMLLoader]
           [javafx.event ActionEvent EventHandler]
           [javafx.scene.input KeyCombination ContextMenuEvent]))

(set! *warn-on-reflection* true)

(defonce ^:dynamic *menus* (atom {}))
(defonce ^:dynamic *main-stage* (atom nil))

(defn set-main-stage [main-stage]
  (reset! *main-stage* main-stage))

(defn choose-file [title ext-descr exts]
  (let [chooser (FileChooser.)]
    (.setTitle chooser title)
    (.add (.getExtensionFilters chooser) (FileChooser$ExtensionFilter. ext-descr (java.util.ArrayList. exts)))
    (let [file (.showOpenDialog chooser nil)]
      (if file (.getAbsolutePath file)))))

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

(defn- set-user-data [^Node node key val]
  (let [ud (or (.getUserData node) {})]
    (.setUserData node (assoc ud key val))))

(defn- get-user-data [^Node node key]
  (get (.getUserData node) key))

(defmacro event-handler [event & body]
  `(reify EventHandler
     (handle [this ~event]
       ~@body)))

(extend-type TreeView
  workspace/SelectionProvider
  (selection [this] (->> this
                      (.getSelectionModel)
                      (.getSelectedItems)
                      (map #(.getValue ^TreeItem %)))))

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

(defn- make-menu [desc menu]
  (let [command-context (make-command-context desc)]
    (->> menu
      (mapv (fn [item] (if (= :separator (:label item))
                         (SeparatorMenuItem.)
                         (create-menu-item desc item)))))))

(defn- populate-context-menu [^ContextMenu context-menu desc menu]
  (let [items (make-menu desc menu)]
    (.addAll (.getItems context-menu) items)))

(defn register-context-menu [^Control control selection-provider menu-id]
  ;; TODO: command-context
  (let [desc (make-desc control menu-id {} selection-provider)]
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
       (set-user-data root ::menubar desc))
     (log/warn :message (format "menubar %s not found" menubar-id)))))

(defn- refresh-menubar [md]
 (let [menu (realize-menu (:menu-id md))
       control (:control md)]
   (when-not (= menu (get-user-data control ::menu))
     (.clear (.getMenus control))
     ; TODO: We must ensure that top-level element are of type Menu and note MenuItem here, i.e. top-level items with ":children"
     (.addAll (.getMenus control) (make-menu md menu))
     (set-user-data control ::menu menu))))

(defn- refresh-menu-state [^Menu menu command-context]
  (doseq [m (.getItems menu)]
    (if (instance? Menu m)
      (refresh-menu-state m command-context)
      (.setDisable m (not (handler/enabled? (keyword (.getId m)) command-context))))))

(defn- refresh-menubar-state [^MenuBar menubar command-context]
  (doseq [m (.getMenus menubar)]
    (refresh-menu-state m command-context)))

(defn register-toolbar [^Scene scene command-context selection-provider toolbar-id menu-id ]
  (let [root (.getRoot scene)]
    (if-let [toolbar (.lookup root toolbar-id)]
      (let [desc (make-desc toolbar menu-id command-context selection-provider)]
        (set-user-data root ::toolbars (assoc-in (get-user-data root ::toolbars) [toolbar-id] desc)))
      (log/warn :message (format "toolbar %s not found" toolbar-id)))))

(defn- refresh-toolbar [td]
 (let [menu (realize-menu (:menu-id td))
       control (:control td)]
   (when-not (= menu (get-user-data control ::menu))
     (.clear (.getChildren control))
     (set-user-data control ::menu menu)
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

(defn- refresh-toolbar-state [toolbar command-context]
  (let [nodes (.getChildren toolbar)
        ids (map #(.getId %) nodes)]
    (doseq [n nodes]
      (.setDisable n (not (handler/enabled? (keyword (.getId n)) command-context)))
      (when (instance? ToggleButton n)
        (if (handler/state (keyword (.getId n)) command-context)
         (.setSelected n true)
         (.setSelected n false))))))

(defn refresh [^Scene scene]
 (let [root (.getRoot scene)
       toolbar-descs (vals (get-user-data root ::toolbars))]
   (when-let [md (get-user-data root ::menubar)]
     (refresh-menubar md)
     (refresh-menubar-state (:control md) (make-command-context md)))
   (doseq [td toolbar-descs]
     (refresh-toolbar td)
     (refresh-toolbar-state (:control td) (make-command-context td)))))

(defn modal-progress [title total-work worker-fn]
  (run-now
    (let [root (FXMLLoader/load (io/resource "progress.fxml"))
          stage (Stage.)
          scene (Scene. root)
          title-control (.lookup root "#title")
          progress-control (.lookup root "#progress")
          message-control (.lookup root "#message")
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
