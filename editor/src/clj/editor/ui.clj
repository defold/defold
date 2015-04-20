(ns editor.ui
  (:require [editor.workspace :as workspace]
            [editor.jfx :as jfx]
            [editor.handler :as handler]
            [service.log :as log])
  (:import [javafx.scene.control ButtonBase Control ContextMenu SeparatorMenuItem ToggleButton TreeView TreeItem MenuItem]
           [javafx.application Platform]
           [javafx.event ActionEvent EventHandler]
           [javafx.scene.input KeyCombination ContextMenuEvent]))

(set! *warn-on-reflection* true)

(def ^:dynamic *menus* (atom {}))

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
  (let [exts (collect-menu-extensions)]
    (->>
      (get @*menus* id)
      (:menu)
      (mapcat (fn [x] (concat [x] (get exts (:id x))))))))

(defn- create-menu-item [item arg-map]
  (let [menu-item (MenuItem. (:label item))
        command (:command item)]
    (when (:acc item)
      (.setAccelerator menu-item (KeyCombination/keyCombination (:acc item))))
    (when (:icon item) (.setGraphic menu-item (jfx/get-image-view (:icon item))))
    (.setDisable menu-item (not (handler/enabled? command arg-map)))
    (.setOnAction menu-item (event-handler event (handler/run command arg-map) ))
    menu-item))

(defn- populate-context-menu [^ContextMenu context-menu selection-provider menu]
  (let [instances (workspace/selection selection-provider)
        arg-map {:instances instances}]
    (doseq [item menu]
      (let [menu-item (if (= :separator (:label item))
                        (SeparatorMenuItem.)
                        (create-menu-item item arg-map) )]
        (.add (.getItems context-menu) menu-item)))))

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

(defn make-ui-binder [scene]
  (atom {:scene scene
         :toolbars {}}))

(defn bind-toolbar [binder toolbar-id arg-map]
  (if-let [toolbar (.lookup (:scene @binder) toolbar-id)]
    (do
      (swap! binder assoc-in [:toolbars toolbar-id] toolbar)
      (doseq [n (.getChildren toolbar)]
        (when (instance? ButtonBase n)
          (.setOnAction n (event-handler event (handler/run (keyword (.getId n)) :project arg-map))))))
    (log/warn :message (format "toolbar %s not found" toolbar-id))))

(defn- refresh-toolbar [toolbar arg-map]
  (let [nodes (.getChildren toolbar)
        ids (map #(.getId %) nodes)]
    (doseq [n nodes]
      (.setDisable n (not (handler/enabled? (keyword (.getId n)) arg-map)))
      (when (instance? ToggleButton n)
        (if (handler/state (keyword (.getId n)) arg-map)
          (.setSelected n true)
          (.setSelected n false))))))

(defn refresh [binder arg-map]
  (doseq [[_ t] (:toolbars @binder)]
    (refresh-toolbar t arg-map)))

