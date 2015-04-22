(ns editor.ui
  (:require [editor.workspace :as workspace]
            [editor.jfx :as jfx]
            [editor.handler :as handler]
            [service.log :as log])
  (:import [javafx.scene Node Scene]
           [javafx.scene.control ButtonBase Control ContextMenu SeparatorMenuItem ToggleButton TreeView TreeItem MenuItem]
           [javafx.application Platform]
           [javafx.event ActionEvent EventHandler]
           [javafx.scene.input KeyCombination ContextMenuEvent]))

(set! *warn-on-reflection* true)

(defonce ^:dynamic *menus* (atom {}))

(defn init []
  (reset! *menus* {}))

(defn do-run-now [f]
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
        val))))

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

(defn- realize-menu [id]
  (let [exts (collect-menu-extensions)
        menu (->>
               (get @*menus* id)
               (:menu)
               (mapcat (fn [x] (concat [x] (get exts (:id x))))))]
    (concat menu (get exts id))))

(defn- create-menu-item [item command-context]
  (let [menu-item (MenuItem. (:label item))
        command (:command item)]
    (when (:acc item)
      (.setAccelerator menu-item (KeyCombination/keyCombination (:acc item))))
    (when (:icon item) (.setGraphic menu-item (jfx/get-image-view (:icon item))))
    (.setDisable menu-item (not (handler/enabled? command command-context)))
    (.setOnAction menu-item (event-handler event (handler/run command command-context) ))
    menu-item))

(defn- populate-context-menu [^ContextMenu context-menu selection-provider menu]
  (let [selection (workspace/selection selection-provider)
        command-context {:selection selection}]
    (doseq [item menu]
      (when (handler/visible? (:command item) command-context)
        (let [menu-item (if (= :separator (:label item))
                          (SeparatorMenuItem.)
                          (create-menu-item item command-context) )]
          (.add (.getItems context-menu) menu-item))))))

(defn register-context-menu [^Control control selection-provider menu-id]
  (.addEventHandler control ContextMenuEvent/CONTEXT_MENU_REQUESTED
    (event-handler event
                   (when-not (.isConsumed event)
                     (let [^ContextMenu cm (ContextMenu.) ]
                       (populate-context-menu cm selection-provider (realize-menu menu-id))
                       ; Required for autohide to work when the event originates from the anchor/source control
                       ; See RT-15160 and Control.java
                       (.setImpl_showRelativeToWindow cm true)
                       (.show cm control (.getScreenX ^ContextMenuEvent event) (.getScreenY ^ContextMenuEvent event))
                       (.consume event))))))

(defn register-tool-bar [^Scene scene command-context selection-provider toolbar-id menu-id ]
  (let [root (.getRoot scene)]
    (if-let [toolbar (.lookup root toolbar-id)]
      (let [toolbar-desc {:control toolbar
                          :menu-id menu-id
                          :command-context command-context
                          :selection-provider selection-provider}]
        (set-user-data root ::toolbars (assoc-in (get-user-data root ::toolbars) [toolbar-id] toolbar-desc)))
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
         (.setId button (name (:command menu-item)))
         (.setOnAction button (event-handler event (handler/run (:command menu-item)
                                                                (assoc (:command-context td) :selection (selection-provider)))))
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
   (doseq [td toolbar-descs]
     (refresh-toolbar td)
     (refresh-toolbar-state (:control td) (:command-context td)))))
