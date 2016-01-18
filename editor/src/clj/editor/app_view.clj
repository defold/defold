(ns editor.app-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.login :as login]
            [editor.project :as project]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.resource :as resource])
  (:import [com.defold.editor EditorApplication]
           [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [java.awt Desktop]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList ListChangeListener]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent Event EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem Menu MenuItem MenuBar TabPane Tab ProgressBar]
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
  (property auto-pulls g/Any)
  (property active-tool g/Keyword)

  (input outline g/Any)

  (output active-tab Tab (g/fnk [^TabPane tab-pane] (-> tab-pane (.getSelectionModel) (.getSelectedItem))))
  (output active-outline g/Any :cached (g/fnk [outline] outline))
  (output active-resource (g/protocol resource/Resource) (g/fnk [^Tab active-tab]
                                                                 (when active-tab
                                                                   (ui/user-data active-tab ::resource))))
  (output active-view g/NodeID (g/fnk [^Tab active-tab]
                                      (when active-tab
                                        (ui/user-data active-tab ::view))))
  (output open-resources g/Any (g/fnk [^TabPane tab-pane] (map (fn [^Tab tab] (ui/user-data tab ::resource)) (.getTabs tab-pane)))))

(defn- invalidate [node label]
  (g/invalidate! [[node label]]))

(defn- disconnect-sources [target-node target-label]
  (for [[source-node source-label] (g/sources-of target-node target-label)]
    (g/disconnect source-node source-label target-node target-label)))

(defn- replace-connection [source-node source-label target-node target-label]
  (concat
    (disconnect-sources target-node target-label)
    (if (and source-node (contains? (-> source-node g/node-type* g/output-labels) source-label))
      (g/connect source-node source-label target-node target-label)
      [])))

(defn- on-selected-tab-changed [app-view resource-node]
  (g/transact
    (replace-connection resource-node :node-outline app-view :outline))
  (invalidate app-view :active-tab))

(defn- on-tabs-changed [app-view]
  (invalidate app-view :open-resources))

(handler/defhandler :move-tool :global
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :move)))
  (state [app-view] (= (g/node-value app-view :active-tool) :move)))

(handler/defhandler :scale-tool :global
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :scale)))
  (state [app-view]  (= (g/node-value app-view :active-tool) :scale)))

(handler/defhandler :rotate-tool :global
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :rotate)))
  (state [app-view]  (= (g/node-value app-view :active-tool) :rotate)))

(ui/extend-menu ::toolbar nil
                [{:icon "icons/Icons_T_01_Select.png"
                  :command :select-tool}
                 {:icon "icons/Icons_T_02_Move.png"
                  :command :move-tool}
                 {:icon "icons/Icons_T_03_Rotate.png"
                  :command :rotate-tool}
                 {:icon "icons/Icons_T_04_Scale.png"
                  :command :scale-tool}])

(handler/defhandler :quit :global
  (enabled? [] true)
  (run [] (Platform/exit)))

(handler/defhandler :new :global
  (enabled? [] true)
  (run [] (prn "NEW NOW!")))

(handler/defhandler :open :global
  (enabled? [] true)
  (run [] (when-let [file-name (ui/choose-file "Open Project" "Project Files" ["*.project"])]
            (EditorApplication/openEditor (into-array String [file-name])))))

(handler/defhandler :logout :global
  (enabled? [] true)
  (run [prefs] (login/logout prefs)))

(handler/defhandler :close :global
  (enabled? [] true)
  (run [app-view] (let [tab-pane ^TabPane (g/node-value app-view :tab-pane)]
                    (when-let [tab (-> tab-pane (.getSelectionModel) (.getSelectedItem))]
                      (.remove (.getTabs tab-pane) tab)
                      ; TODO: Workaround as there's currently no API to close tabs programatically with identical semantics to close manually
                      ; See http://stackoverflow.com/questions/17047000/javafx-closing-a-tab-in-tabpane-dynamically
                      (Event/fireEvent tab (Event. Tab/CLOSED_EVENT))))))

(defn make-about-dialog []
  (let [root ^Parent (ui/load-fxml "about.fxml")
        stage (Stage.)
        scene (Scene. root)
        controls (ui/collect-controls root ["version" "sha1"])]
    (ui/text! (:version controls) (str "Version: " (System/getProperty "defold.version" "NO VERSION")))
    (ui/text! (:sha1 controls) (format "(%s)" (System/getProperty "defold.sha1" "NO SHA1")))
    (ui/title! stage "About")
    (.setScene stage scene)
    (ui/show! stage)))

(handler/defhandler :about :global
  (enabled? [] true)
  (run [] (make-about-dialog)))

(handler/defhandler :reload-stylesheet :global
  (enabled? [] true)
  (run [] (ui/reload-root-styles!)))

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
                             {:label "Open Asset"
                              :acc "Shift+Shortcut+R"
                              :command :open-asset}
                             {:label :separator}
                             {:label "Close"
                              :acc "Shortcut+W"
                              :command :close}
                             {:label :separator}
                             {:label "Logout"
                              :command :logout}
                             {:label "Quit"
                              :acc "Shortcut+Q"
                              :command :quit}]}
                 {:label "Edit"
                  :id ::edit
                  :children [{:label "Undo"
                              :acc "Shortcut+Z"
                              :icon "icons/undo.png"
                              :command :undo}
                             {:label "Redo"
                              :acc "Shift+Shortcut+Z"
                              :icon "icons/redo.png"
                              :command :redo}
                             {:label :separator}
                             {:label "Copy"
                              :acc "Shortcut+C"
                              :command :copy}
                             {:label "Cut"
                              :acc "Shortcut+X"
                              :command :cut}
                             {:label "Paste"
                              :acc "Shortcut+V"
                              :command :paste}
                             {:label "Delete"
                              :acc "Shortcut+BACKSPACE"
                              :icon_ "icons/redo.png"
                              :command :delete}
                             ]}
                 {:label "Help"
                  :children [{:label "About"
                              :command :about}
                             {:label "Reload Stylesheet"
                              :acc "F5"
                              :command :reload-stylesheet}]}])

(defrecord DummySelectionProvider []
  workspace/SelectionProvider
  (selection [this] []))

(defn- make-title
  ([] "Defold Editor 2.0")
  ([project-title] (str (make-title) " - " project-title)))

(defn make-app-view [view-graph project-graph project ^Stage stage ^MenuBar menu-bar ^TabPane tab-pane prefs]
  (.setUseSystemMenuBar menu-bar true)
  (.setTitle stage (make-title))
  (let [app-view (first (g/tx-nodes-added (g/transact (g/make-node view-graph AppView :stage stage :tab-pane tab-pane :active-tool :move))))]
    (-> tab-pane
      (.getSelectionModel)
      (.selectedItemProperty)
      (.addListener
        (reify ChangeListener
          (changed [this observable old-val new-val]
            (on-selected-tab-changed app-view (when new-val (ui/user-data ^Tab new-val ::resource-node)))))))
    (-> tab-pane
      (.getTabs)
      (.addListener
        (reify ListChangeListener
          (onChanged [this change]
            (on-tabs-changed app-view)))))

    (ui/register-toolbar (.getScene stage) "#toolbar" ::toolbar)
    (ui/register-menubar (.getScene stage) "#menu-bar" ::menubar)
    (let [ui-refresh-timer (ui/->timer 2 (fn [dt]
                                           (ui/refresh (.getScene stage))
                                           (let [settings (g/node-value project :settings)
                                                 project-title (settings ["project" "title"])
                                                 new-title (make-title project-title)]
                                             (when (not= (.getTitle stage) new-title)
                                               (.setTitle stage new-title)))))
          auto-refresh-timer (ui/->timer 10 (fn [dt]
                                              (let [auto-pulls (g/node-value app-view :auto-pulls)]
                                                (doseq [[node label] auto-pulls]
                                                  (g/node-value node label)))))]
      (ui/on-close-request! stage (fn [_]
                                    (ui/timer-stop! ui-refresh-timer)
                                    (ui/timer-stop! auto-refresh-timer)))
      (ui/timer-start! ui-refresh-timer)
      (ui/timer-start! auto-refresh-timer))
    app-view))

(defn open-resource [app-view workspace project resource]
  (let [resource-type (resource/resource-type resource)
        view-type (or (first (:view-types resource-type)) (workspace/get-view-type workspace :text))]
    (if-let [make-view-fn (:make-view-fn view-type)]
      (let [resource-node (project/get-resource-node project resource)
            ^TabPane tab-pane   (g/node-value app-view :tab-pane)
            parent     (AnchorPane.)
            tab        (doto (Tab. (resource/resource-name resource))
                         (.setContent parent)
                         (ui/user-data! ::resource resource)
                         (ui/user-data! ::resource-node resource-node))
            tabs       (doto (.getTabs tab-pane) (.add tab))
            view-graph (g/make-graph! :history false :volatility 2)
            opts       (assoc ((:id view-type) (:view-opts resource-type))
                              :app-view app-view
                              :project project
                              :workspace workspace
                              :tab tab)
            view       (make-view-fn view-graph parent resource-node opts)]
        (ui/user-data! tab ::view view)
        (.setGraphic tab (jfx/get-image-view (:icon resource-type "icons/cog.png") 16))
        (let [close-handler (.getOnClosed tab)]
          (.setOnClosed tab (ui/event-handler
                             event
                             (g/delete-graph! view-graph)
                             (when close-handler
                               (.handle close-handler event)))))
        (.select (.getSelectionModel tab-pane) tab)
        (project/select! project [resource-node]))
      (.open (Desktop/getDesktop) (File. (resource/abs-path resource))))))

(handler/defhandler :open-asset :global
  (enabled? [] true)
  (run [workspace project app-view] (when-let [resource (first (dialogs/make-resource-dialog workspace {}))]
                                      (open-resource app-view workspace project resource))))

(defn fetch-libraries [workspace project]
  (workspace/set-project-dependencies! workspace (project/project-dependencies project))
  (workspace/update-dependencies! workspace)
  (workspace/resource-sync! workspace))

(handler/defhandler :fetch-libraries :global
  (enabled? [] true)
  (run [workspace project] (fetch-libraries workspace project)))
