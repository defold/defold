(ns editor.app-view
  (:require [dynamo.graph :as g]
            [dynamo.types :as t]
            [editor.jfx :as jfx]
            [editor.project :as project]
            [editor.handler :as handler]
            [editor.login :as login]
            [editor.dialogs :as dialogs]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [com.defold.editor EditorApplication]
           [java.awt Desktop]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList ListChangeListener]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar TabPane Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))

(g/defnode AppView
  (property stage Stage)
  (property tab-pane TabPane)
  (property refresh-timer AnimationTimer)
  (property auto-pulls t/Any)
  (property active-tool t/Keyword)

  (input outline t/Any)

  (output active-outline t/Any :cached (g/fnk [outline] outline))
  (output active-resource (t/protocol workspace/Resource) (g/fnk [^TabPane tab-pane] (when-let [^Tab tab (-> tab-pane (.getSelectionModel) (.getSelectedItem))] (:resource (.getUserData tab)))))
  (output open-resources t/Any (g/fnk [^TabPane tab-pane] (map (fn [^Tab tab] (:resource (.getUserData tab))) (.getTabs tab-pane))))

  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (:refresh-timer self)))))

(defn- invalidate [node label]
  (g/invalidate! [[(g/node-id node) label]]))

(defn- disconnect-sources [target-node target-label]
  (for [[source-node source-label] (g/sources-of (g/now) target-node target-label)]
    (g/disconnect source-node source-label target-node target-label)))

(defn- replace-connection [source-node source-label target-node target-label]
  (concat
    (disconnect-sources target-node target-label)
    (if (and source-node (contains? (g/outputs source-node) source-label))
      (g/connect source-node source-label target-node target-label)
      [])))

(defn- on-selected-tab-changed [app-view resource-node]
  (g/transact
    (replace-connection resource-node :outline app-view :outline))
  (invalidate app-view :active-resource))

(defn- on-tabs-changed [app-view]
  (invalidate app-view :open-resources))

(handler/defhandler :move-tool
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :move)))
  (state [app-view] (= (:active-tool (g/refresh app-view)) :move)))

(handler/defhandler :scale-tool
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :scale)))
  (state [app-view]  (= (:active-tool (g/refresh app-view)) :scale)))

(handler/defhandler :rotate-tool
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :rotate)))
  (state [app-view]  (= (:active-tool (g/refresh app-view)) :rotate)))

(ui/extend-menu ::toolbar nil
                [{:label "Move"
                  :icon "icons/transform_move.png"
                  :command :move-tool}
                 {:label "Rotate"
                  :icon "icons/transform_rotate.png"
                  :command :rotate-tool}
                 {:label "Scale"
                  :icon "icons/transform_scale.png"
                  :command :scale-tool}])

(handler/defhandler :quit
  (enabled? [] true)
  (run [] (prn "QUIT NOW!")))

(handler/defhandler :new
  (enabled? [] true)
  (run [] (prn "NEW NOW!")))

(handler/defhandler :open
  (enabled? [] true)
  (run [] (when-let [file-name (ui/choose-file "Open Project" "Project Files" ["*.project"])]
            (EditorApplication/openEditor (into-array String [file-name])))))

(handler/defhandler :logout
  (enabled? [] true)
  (run [prefs] (login/logout prefs)))

(handler/defhandler :open-resource
  (enabled? [] true)
  (run [workspace] (prn (dialogs/make-resource-dialog workspace {})) ))

(ui/extend-menu ::menubar nil
                [{:label "File"
                  :id ::file
                  :children [{:label "New"
                              :id ::new
                              :acc "Shortcut+N"
                              :command :new}
                             {:label "Open..."
                              :id ::open
                              :acc "Shortcut+O"
                              :command :open}
                             {:label :separator}
                             {:label "Logout"
                              :command :logout}
                             {:label "Quit"
                              :acc "Shortcut+Q"
                              :command :quit}]}
                 {:label "Edit"
                  :id ::file
                  :children [{:label "Undo"
                              :acc "Shortcut+Z"
                              :icon "icons/undo.png"
                              :command :undo}
                             {:label "Redo"
                              :acc "Shift+Shortcut+Z"
                              :icon "icons/redo.png"
                              :command :redo}
                             {:label :separator}
                             {:label "Open Resource"
                              :acc "Shift+Shortcut+R"
                              :command :open-resource}]}])

(defrecord DummySelectionProvider []
  workspace/SelectionProvider
  (selection [this] []))

(defn make-app-view [view-graph project-graph project ^Stage stage ^MenuBar menu-bar ^TabPane tab-pane prefs]
  (.setUseSystemMenuBar menu-bar true)
  (.setTitle stage "Defold Editor 2.0!")
  (let [app-view (first (g/tx-nodes-added (g/transact (g/make-node view-graph AppView :stage stage :tab-pane tab-pane :active-tool :move))))
        command-context {:app-view app-view
                         :project project
                         :project-graph project-graph
                         :prefs prefs
                         :workspace (:workspace project)}]
    (-> tab-pane
      (.getSelectionModel)
      (.selectedItemProperty)
      (.addListener
        (reify ChangeListener
          (changed [this observable old-val new-val]
            (on-selected-tab-changed app-view (when new-val (.getUserData ^Tab new-val)))))))
    (-> tab-pane
      (.getTabs)
      (.addListener
        (reify ListChangeListener
          (onChanged [this change]
            (on-tabs-changed app-view)))))

    (ui/register-toolbar (.getScene stage) command-context (DummySelectionProvider.) "#toolbar" ::toolbar)
    (ui/register-menubar (.getScene stage) command-context (DummySelectionProvider.) "#menu-bar" ::menubar)
    (let [refresh-timer (proxy [AnimationTimer] []
                          (handle [now]
                            ; TODO: Not invoke this function every frame...
                            (ui/refresh (.getScene stage))
                            (let [auto-pulls (:auto-pulls (g/refresh app-view))]
                             (doseq [[node label] auto-pulls]
                               (g/node-value node label)))))]
      (g/transact
        (concat
          (g/set-property app-view :refresh-timer refresh-timer)))
      (.start refresh-timer))
    app-view))

(defn open-resource [app-view workspace project resource]
  (let [resource-node (project/get-resource-node project resource)
        resource-type (project/get-resource-type resource-node)
        view-type (or (first (:view-types resource-type)) (workspace/get-view-type workspace :text))]
    (if-let [make-view-fn (:make-view-fn view-type)]
      (let [^TabPane tab-pane   (:tab-pane app-view)
            parent     (AnchorPane.)
            tab        (doto (Tab. (workspace/resource-name resource)) (.setContent parent) (.setUserData resource-node))
            tabs       (doto (.getTabs tab-pane) (.add tab))
            view-graph (g/make-graph! :history false :volatility 2)
            view       (make-view-fn view-graph parent resource-node (assoc ((:id view-type) (:view-opts resource-type)) :select-fn (fn [selection op-seq] (project/select! project selection op-seq))))]
        (g/transact
          (concat
            (g/connect project :selection view :selection)
            (g/connect app-view :active-tool view :active-tool)))
        (.setGraphic tab (jfx/get-image-view (:icon resource-type "icons/cog.png")))
        (.setOnClosed tab (ui/event-handler event (g/delete-graph view-graph)))
        (.select (.getSelectionModel tab-pane) tab))
      (.open (Desktop/getDesktop) (File. ^String (workspace/abs-path resource))))))
