(ns editor.ui
  (:require [editor.workspace :as workspace]
            [editor.jfx :as jfx]
            [editor.handler :as handler])
  (:import [javafx.scene.control Control ContextMenu SeparatorMenuItem TreeView TreeItem MenuItem] 
           [javafx.event ActionEvent EventHandler]
           [javafx.scene.input KeyCombination ContextMenuEvent]))

(set! *warn-on-reflection* true)

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

(defn- create-menu-item [item context arg-map]
  (let [menu-item (MenuItem. (:label item))
        command (:command item)]
    (when (:acc item)
      (.setAccelerator menu-item (KeyCombination/keyCombination (:acc item))))
    (when (:icon item) (.setGraphic menu-item (jfx/get-image-view (:icon item))))
    (.setDisable menu-item (not (handler/enabled? command context arg-map)))
    (.setOnAction menu-item (event-handler event (handler/run command context arg-map) ))
    menu-item))

(defn- populate-context-menu [^ContextMenu context-menu selection-provider menu-var]
  (let [instances (workspace/selection selection-provider)
        context :project ; TODO
        arg-map {:instances instances}]
    (doseq [item @menu-var]
      (let [menu-item (if (= :separator (:label item))
                        (SeparatorMenuItem.)
                        (create-menu-item item context arg-map) )]
        (.add (.getItems context-menu) menu-item)))))

(defn register-context-menu [^Control control selection-provider menu-var]
  (.addEventHandler control ContextMenuEvent/CONTEXT_MENU_REQUESTED
    (event-handler event
                   (when-not (.isConsumed event)
                     (let [^ContextMenu cm (ContextMenu.) ]
                       (populate-context-menu cm selection-provider menu-var)
                       ; Required for autohide to work when the event originates from the anchor/source control
                       ; See RT-15160 and Control.java
                       (.setImpl_showRelativeToWindow cm true)
                       (.show cm control (.getScreenX ^ContextMenuEvent event) (.getScreenY ^ContextMenuEvent event))
                       (.consume event))))))

