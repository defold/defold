;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns integration.live-update-manifest-test
  (:require [cljfx.api :as fx]
            [clojure.java.io :as io]
            [clojure.test :refer [deftest is use-fixtures]]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.build :as build]
            [editor.code.view :as code-view]
            [editor.defold-project :as project]
            [editor.editor-tab :as editor-tab]
            [editor.fs :as fs]
            [editor.live-update-manifest :as manifest]
            [editor.live-update-manifest-view :as manifest-view]
            [editor.live-update-preview :as preview]
            [editor.prefs :as prefs]
            [editor.properties :as properties]
            [editor.properties-view :as properties-view]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.coll :as coll])
  (:import [com.dynamo.bob ClassLoaderResourceScanner Platform Progress Project TaskResult]
           [com.dynamo.bob.archive.liveupdate PackagePreview$Result PackageResolver$Package PackageResolver$Plan PackageResolver$Severity]
           [com.dynamo.bob.fs DefaultFileSystem]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ManifestDesc]
           [java.net URI]
           [java.nio.charset StandardCharsets]
           [java.nio.file Files Path]
           [java.nio.file.attribute FileTime]
           [java.util.zip ZipEntry ZipOutputStream]
           [javafx.animation AnimationTimer PauseTransition]
           [javafx.event Event]
           [javafx.scene Scene]
           [javafx.scene.control Button Hyperlink Label MenuItem RadioButton ScrollPane SplitPane Tab TabPane TextField TreeItem TreeTableColumn TreeTableView]
           [javafx.scene.input Clipboard]
           [javafx.scene.layout Pane]
           [javafx.stage Stage]
           [javafx.util Duration]
           [javax.tools ToolProvider]))

(set! *warn-on-reflection* true)

(defn- with-editor-window [run-tests!]
  (let [^Stage stage (ui/run-now
                       (let [scene (Scene. (ui/load-fxml "editor.fxml"))]
                         (ui/user-data! scene :localization test-util/localization)
                         (let [stage (doto (Stage.) (.setScene scene))]
                           (.applyCss (.getRoot scene))
                           stage)))]
    (binding [ui/*main-stage* (atom stage)]
      (try
        (run-tests!)
        (finally
          (ui/run-now (.close stage)))))))

(use-fixtures :once with-editor-window)

(defn- await-ui! [ready?]
  ;; Keep the scratch project's dynamic bindings on the JavaFX thread while
  ;; real view timers and completion callbacks run. The deadline covers UI work
  ;; as well as background builds; no test advances the production clock.
  (let [key (Object.)
        timeout-error (ex-info "Timed out waiting for editor completion" {})
        completed (volatile! false)
        finish! (fn [value]
                  (when-not @completed
                    (vreset! completed true)
                    (ui/run-later (javafx.application.Platform/exitNestedEventLoop key value))))
        timer (proxy [AnimationTimer] []
                (handle [_]
                  (try
                    (when-let [value (ready?)]
                      (finish! value))
                    (catch Throwable error
                      (finish! error)))))
        timeout (doto (PauseTransition. (Duration/seconds 30))
                  (.setOnFinished (ui/event-handler _
                                    (finish! timeout-error))))]
    (try
      (.start timer)
      (.play timeout)
      (let [value (javafx.application.Platform/enterNestedEventLoop key)]
        (if (instance? Throwable value)
          (throw value)
          value))
      (finally
        (.stop timer)
        (.stop timeout)))))

(defn- open-manifest-view! [project app-view path preferences]
  (let [workspace (project/workspace project)
        scene (.getScene (ui/main-stage))
        split ^SplitPane (.lookup (.getRoot scene) "#editor-tabs-split")]
    (when (.isEmpty (.getItems split))
      (.add (.getItems split) (TabPane.)))
    (g/transact
      {:undoable false}
      (concat
        (manifest-view/register-view-types workspace)
        (code-view/register-view-types workspace)
        (g/set-property app-view :stage (ui/main-stage))
        (g/set-property app-view :scene scene)
        (g/set-property app-view :editor-tabs-split split)
        (g/set-property app-view :localization test-util/localization)))
    (g/transact (manifest/register-resource-types workspace))
    (ui/context! (.getRoot scene) :global
                 {:app-view app-view :workspace workspace :project project
                  :prefs preferences :localization test-util/localization} nil)
    (is (true? (app-view/open-resource! app-view preferences test-util/localization project
                                        (workspace/find-resource workspace path))))
    (.applyCss (.getRoot scene))
    (.layout (.getRoot scene))
    (let [tab (.getSelectedItem (.getSelectionModel ^TabPane (first (.getItems split))))
          view (editor-tab/view-node-id tab)]
      {:tab tab :view view :parent (.getContent ^Tab tab)
       :preferences preferences :controller (g/user-data view :preview-controller)
       :rows-controller (g/user-data view :rows-controller)})))

(defn- close-manifest-view! [{:keys [^Tab tab view controller rows-controller]}]
  (Event/fireEvent tab (Event. Tab/CLOSED_EVENT))
  (.remove (.getTabs (.getTabPane tab)) tab)
  (await-ui! #(and (nil? (g/node-by-id view))
                   (realized? (:close-completion @controller))
                   (realized? (:close-completion @rows-controller))))
  (is (true? @(:close-completion @controller)))
  (is (true? @(:close-completion @rows-controller)))
  (when (realized? (:directory @controller))
    (is (not (.exists (.toFile ^Path @(:directory @controller)))))))

(defn- await-preview! [controller previous]
  (await-ui!
    (fn []
      (let [{:keys [job result error]} @controller]
        (when error (throw error))
        (when (and (nil? job) result (not (identical? result previous)))
          (:compiled (:value result)))))))

(defn- resource-table
  ^TreeTableView [^Pane parent]
  (.lookup parent "#liveupdate-resources"))

(defn- table-tree [^TreeTableView table]
  (letfn [(children [^TreeItem item]
            (mapv (fn [^TreeItem child] [(:path (.getValue child)) (children child)])
                  (.getChildren item)))]
    (children (.getRoot table))))

(defn- table-rows [^TreeTableView table]
  (coll/into-> [(.getRoot table)] []
    (coll/tree-xf (constantly true) #(.getChildren ^TreeItem %))
    (keep #(.getValue ^TreeItem %))))

(defn- with-manifest-view! [project app-view path f]
  (ui/run-now
    (let [prefs-file (fs/create-temp-file! "liveupdate-preview" ".editor_settings")
          preferences (prefs/make :scopes {:global prefs-file :project prefs-file} :schemas [:default])
          view (open-manifest-view! project app-view path preferences)]
      (try
        (let [^PackagePreview$Result compiled (await-preview! (:controller view) nil)]
          (await-ui! #(when-let [table (resource-table (:parent view))]
                        (= (.base (.plan compiled))
                           (coll/into-> (table-rows table) #{} (map :path))))))
        (f view)
        (finally
          (close-manifest-view! view))))))

(defn- build-preview! [project contents manifest-path options cancelled?]
  (let [workspace (project/workspace project)
        controller (preview/controller)
        resources (preview/source-resources (g/node-value workspace :resource-map)
                                            (g/node-value project :live-update-output-paths))
        statuses (:status-map (g/node-value workspace :resource-snapshot))]
    (try
      (preview/build! controller resources statuses contents manifest-path options cancelled?)
      (finally
        (is (= true (deref (preview/close! controller) 30000 :timeout)))))))

;; Package/rule edits must use graph history and preserve the authoring format
;; on reload.
(deftest authoring-and-history
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (project/get-resource-node project "/packages.lumanifest")
          original (g/node-value node :save-value)
          [levels copy] (g/node-value node :packages)
          package-node (:node-id levels)
          [episode data] (:rules levels)]
      (is (= "folder" (get-in copy [:value :provider])))
      (is (:read-only (first (:children (g/node-value node :node-outline)))))
      (is (= #{"/episode.collection" "/extra.json"} (g/node-value node :selected-paths)))
      (test-util/handler-run :edit.reorder-up
                             [{:name :workbench :env {:selection [(:node-id data)]}}] nil)
      (is (= ["Data" "Episode"] (mapv :name (get-in (g/node-value node :save-value) [:packages 0 :rules]))))
      (g/undo! :undo/global)
      (is (= original (g/node-value node :save-value)))
      (g/redo! :undo/global)
      (g/transact (g/set-property package-node :id "renamed"))
      (is (= "renamed" (get-in (g/node-value node :save-value) [:packages 1 :rules 0 :package])))
      (is (= "+ Package: renamed"
             (@test-util/localization (:label (g/node-value (get-in copy [:rules 0 :node-id]) :node-outline)))))
      (g/transact [(g/set-property package-node :provider :custom)
                   (g/set-property package-node :custom-provider "studio-package")])
      (is (= "studio-package" (get-in (g/node-value node :save-value) [:packages 0 :provider])))
      (g/transact (g/set-property (:node-id episode) :no-match :error))
      (is (= :error (get-in (g/node-value node :save-value) [:packages 0 :rules 1 :no-match])))
      (let [value (g/node-value node :save-value)
            text (resource-node/save-data-content (g/node-value node :save-data))]
        (is (= value (protobuf/read-map-without-defaults LiveUpdateManifest$ManifestDesc (java.io.StringReader. text))))
        (test-util/make-resource! workspace "/reloaded.lumanifest" value)
        (workspace/resource-sync! workspace)
        (is (= value (g/node-value (project/get-resource-node project "/reloaded.lumanifest") :save-value)))))))

;; Output directories preserve authored variables through property edits, undo
;; and reload. Previews resolve unsaved values against the real project without
;; creating output folders.
(deftest output-directory-property
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (project/get-resource-node project "/packages.lumanifest")
          root (workspace/project-directory workspace)
          original (g/node-value node :save-value)
          property #(get-in (properties/coalesce [(g/node-value node :_properties)]) [:properties :output-directory])]
      (is (= "build/packages" (g/node-value node :output-directory)))
      (ui/run-now
        (let [description (properties-view/make-control-view (property) {} @test-util/localization)
              change! (get-in description [:children 0 :on-value-changed])]
          (change! "${HOME}/liveupdate/packages")
          (is (= "${HOME}/liveupdate/packages" (:output-directory (g/node-value node :save-value))))
          (is (= (.toPath (io/file (System/getenv "HOME") "liveupdate/packages")) (g/node-value node :resolved-output-directory)))
          (g/undo! :undo/global)
          (is (= original (g/node-value node :save-value)))
          (g/redo! :undo/global)
          (change! "${DEFOLD_TEST_UNDEFINED_PACKAGE_OUTPUT}")
          (is (= :fatal (:severity (properties/validation-message (property)))))
          (change! "exported packages")))
      (let [contents {"/packages.lumanifest" (resource-node/save-data-content (g/node-value node :save-data))}
            ^PackagePreview$Result result (build-preview! project contents "/packages.lumanifest" {} (constantly false))]
        (is (= {"levels" [(io/file root "exported packages/levels.zip")]
                "copy" [(io/file root "exported packages/copy")]}
               (coll/into-> (.exportPaths result) {} (map (fn [[name paths]] [name (mapv io/file paths)])))))
        (is (not (.exists (io/file root "exported packages"))))
        (test-util/make-resource! workspace "/reloaded.lumanifest" (g/node-value node :save-value))
        (workspace/resource-sync! workspace)
        (is (= "exported packages" (g/node-value (project/get-resource-node project "/reloaded.lumanifest") :output-directory)))))))

;; Root settings use the Java provider schema, retain legacy defaults, and save
;; explicit overrides through normal property editing, history and reload.
(deftest provider-properties
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (project/get-resource-node project "/packages.lumanifest")
          key :live-update-provider.zip/compression-level
          property #(get-in (properties/coalesce [(g/node-value node :_properties)]) [:properties key])]
      (is (= [1] (properties/values (property))))
      (is (not (properties/overridden? (property))))
      (is (= (mapv (fn [n] [n (str n)]) (range -1 10)) (get-in (property) [:edit-type :options])))
      (spit (workspace/file-resource workspace "/liveupdate.settings") "[liveupdate]\nzip-compression-level = 6\n")
      (workspace/resource-sync! workspace)
      (test-util/set-setting! (project/get-resource-node project "/game.project") ["liveupdate" "settings"]
                              (workspace/file-resource workspace "/liveupdate.settings"))
      (is (= [6] (properties/values (property))))
      (test-util/set-setting! (project/get-resource-node project "/liveupdate.settings") ["liveupdate" "zip-compression-level"] 7)
      (is (= [7] (properties/values (property))))
      (ui/run-now
        (let [control (:desc (properties-view/make-control-view (property) {} @test-util/localization))]
          ((:on-value-changed control) 9)))
      (is (= [{:provider "zip" :properties [{:name "compression-level" :value "9"}]}]
             (:providers (g/node-value node :save-value))))
      (is (= [9] (properties/values (property))))
      (is (properties/overridden? (property)))
      (g/undo! :undo/global)
      (is (= [7] (properties/values (property))))
      (g/redo! :undo/global)
      (test-util/make-resource! workspace "/provider-reloaded.lumanifest" (g/node-value node :save-value))
      (workspace/resource-sync! workspace)
      (is (= (g/node-value node :save-value) (g/node-value (project/get-resource-node project "/provider-reloaded.lumanifest") :save-value)))
      (properties/clear-override! (property))
      (is (= [7] (properties/values (property))))
      (is (not (contains? (g/node-value node :save-value) :providers)))
      (g/undo! :undo/global)
      (is (= [9] (properties/values (property))))
      (properties/set-values! (property) ["wrong"])
      (is (= :fatal (:severity (properties/validation-message (property)))))
      (is (re-find #"compression-level.*wrong" (@test-util/localization (:message (properties/validation-message (property))))))
      (properties/set-values! (property) [3])
      (is (nil? (properties/validation-message (property)))))))

;; Unused and unavailable provider settings survive form edits and reload, so
;; changing providers does not silently erase authored configuration.
(deftest preserve-provider-settings
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/provider-settings.lumanifest"
                              {:packages [{:name "custom" :provider "unavailable"}]
                               :providers [{:provider "zip" :properties [{:name "compression-level" :value "4"}]}
                                           {:provider "unavailable" :properties [{:name "format" :value "a"}]}]})
    (workspace/resource-sync! workspace)
    (let [node (project/get-resource-node project "/provider-settings.lumanifest")
          package (:node-id (first (g/node-value node :packages)))
          property #(get-in (properties/coalesce [(g/node-value node :_properties)]) [:properties :live-update-provider.unavailable/format])]
      (is (= ["a"] (properties/values (property))))
      (properties/set-values! (property) ["b"])
      (is (= "4" (get-in (g/node-value node :save-value) [:providers 0 :properties 0 :value])))
      (is (= "b" (get-in (g/node-value node :save-value) [:providers 1 :properties 0 :value])))
      (g/transact (g/set-property package :provider :folder))
      (is (nil? (property)))
      (g/transact (g/set-property package :provider :custom))
      (is (= ["b"] (properties/values (property))))
      (test-util/make-resource! workspace "/provider-reloaded.lumanifest" (g/node-value node :save-value))
      (workspace/resource-sync! workspace)
      (is (= (g/node-value node :save-value) (g/node-value (project/get-resource-node project "/provider-reloaded.lumanifest") :save-value))))))

;; Adding a pipeline JAR refreshes an existing manifest's cached properties.
;; Unsaved edits reach Bob's preview without invoking provider writers.
(deftest custom-provider-properties
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/provider.lumanifest"
                              {:packages [{:name "custom" :provider "manifest-test"
                                           :rules [{:name "Data" :path "/extra[.]json"}]}]})
    (workspace/resource-sync! workspace)
    (let [root (workspace/project-directory workspace)
          node (project/get-resource-node project "/provider.lumanifest")
          classes (io/file root "build/provider-test-classes")
          jar (workspace/file-resource workspace "/provider/plugins/share/provider.jar")
          class-path "com/defold/extension/pipeline/ManifestTestProvider.class"]
      (is (= "build/packages" (get-in (g/node-value node :_properties) [:properties :output-directory :value])))
      (fs/create-directories! classes)
      (is (zero? (.run (ToolProvider/getSystemJavaCompiler) nil nil nil
                       (into-array String ["-classpath" (System/getProperty "java.class.path") "-d" (str classes)
                                           "test/resources/live_update_provider/ManifestTestProvider.java"]))))
      (io/make-parents (io/file jar))
      (spit (workspace/file-resource workspace "/provider/ext.manifest") "name: ManifestTestProvider\n")
      (with-open [output (ZipOutputStream. (io/output-stream jar))]
        (doseq [path ["com/" "com/defold/" "com/defold/extension/" "com/defold/extension/pipeline/" class-path]]
          (.putNextEntry output (ZipEntry. ^String path))
          (when (= class-path path)
            (io/copy (io/file classes path) output))
          (.closeEntry output)))
      (workspace/resource-sync! workspace)
      (let [property (fn [id]
                       (get-in (properties/coalesce [(g/node-value node :_properties)])
                               [:properties (keyword "live-update-provider.manifest-test" id)]))]
        (is (= ["custom"] (properties/values (property "extension"))))
        (is (= [2] (properties/values (property "quality"))))
        (is (= [1.0] (properties/values (property "scale"))))
        (doseq [[id value] [["extension" "alternate"] ["quality" 7] ["scale" 0.5]]]
          (properties/set-values! (property id) [value]))
        (let [^PackagePreview$Result result (build-preview! project
                                                            {"/provider.lumanifest" (resource-node/save-data-content (g/node-value node :save-data))}
                                                            "/provider.lumanifest" {} (constantly false))]
          (is (= [(io/file root "build/packages/custom.alternate-7-0.5")]
                 (mapv io/file (get (.exportPaths result) "custom"))))
          (is (not (.exists (io/file root "build/packages")))))))))

;; Radio choices, including keyboard-style group selection, edit the graph
;; through normal property transactions, preserve undo/redo, and leave mixed
;; targets unselected until the user chooses one. Target is first, and package
;; references use their target as the outline label instead of an editable name.
(deftest rule-radio-properties
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (project/get-resource-node project "/packages.lumanifest")
          packages (g/node-value node :packages)
          path-rule (get-in packages [0 :rules 0 :node-id])
          package-rule (get-in packages [1 :rules 0 :node-id])
          original (g/node-value node :save-value)]
      (is (= [:target :id :operation :path :no-match]
             (:display-order (properties/coalesce [(g/node-value path-rule :_properties)]))))
      (is (= [:target :operation :package]
             (:display-order (properties/coalesce [(g/node-value package-rule :_properties)]))))
      (is (= "+ Package: levels" (@test-util/localization (:label (g/node-value package-rule :node-outline)))))
      (ui/run-now
        (doseq [[prop-kw node-ids labels initial-values initial-selection choice choice-selection]
                [[:operation [path-rule] ["Add" "Remove"] [:add] [true false] :remove [false true]]
                 [:target [path-rule] ["Path" "Package"] [:path] [true false] :package [false true]]
                 [:target [path-rule package-rule] ["Path" "Package"] [:path :package] [false false] :package [false true]]
                 [:no-match [path-rule] ["Warn" "Error" "Ignore"] [:warn] [true false false] :error [false true false]]]]
          (let [description (fn []
                              (properties-view/make-control-view
                                (get-in (properties/coalesce (mapv #(g/node-value % :_properties) node-ids))
                                        [:properties prop-kw])
                                {} @test-util/localization))
                component (volatile! (fx/create-component (description)))
                buttons (.getChildren ^Pane (fx/instance @component))
                selected (fn [] (mapv #(.isSelected ^RadioButton %) buttons))]
            (try
              (is (= labels (mapv #(.getText ^RadioButton %) buttons)))
              (is (= initial-selection (selected)))
              (is (= original (g/node-value node :save-value)))
              (let [^RadioButton button (second buttons)]
                (.selectToggle (.getToggleGroup button) button))
              (is (= (mapv (constantly choice) node-ids) (mapv #(g/node-value % prop-kw) node-ids)))
              (let [saved-rule (get-in (g/node-value node :save-value) [:packages 0 :rules 0])]
                (case prop-kw
                  :operation (is (= :remove (:operation saved-rule)))
                  :no-match (is (= :error (:no-match saved-rule)))
                  :target (do (is (= "" (:package saved-rule)))
                              (is (not (contains? saved-rule :path)))
                              (is (= [:target :operation :package]
                                     (:display-order (properties/coalesce [(g/node-value path-rule :_properties)])))))))
              (vswap! component fx/advance-component (description))
              (is (= choice-selection (selected)))
              (g/undo! :undo/global)
              (vswap! component fx/advance-component (description))
              (is (= initial-values (mapv #(g/node-value % prop-kw) node-ids)))
              (is (= initial-selection (selected)))
              (is (= original (g/node-value node :save-value)))
              (is (= [:target :id :operation :path :no-match]
                     (:display-order (properties/coalesce [(g/node-value path-rule :_properties)]))))
              (is (= "+ Episode" (:label (g/node-value path-rule :node-outline))))
              (g/redo! :undo/global)
              (vswap! component fx/advance-component (description))
              (is (= choice-selection (selected)))
              (g/undo! :undo/global)
              (finally
                (fx/delete-component @component)))))))))

;; Package choices come from Bob and follow unsaved reference changes, undo,
;; and renames without offering self-references or indirect cycles.
(deftest package-reference-properties
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/references.lumanifest"
                              {:packages [{:name "a" :rules [{:name "Include b" :package "b"}]}
                                          {:name "b" :rules [{:name "Subtract c" :operation :remove :package "c"}]}
                                          {:name "c" :rules [{:name "Include independent" :package "independent"}]}
                                          {:name "independent"}]})
    (workspace/resource-sync! workspace)
    (let [node (project/get-resource-node project "/references.lumanifest")
          [a b c] (g/node-value node :packages)
          rule (get-in b [:rules 0 :node-id])
          property (fn [rule]
                     (get-in (properties/coalesce [(g/node-value rule :_properties)]) [:properties :package]))
          choices (fn [package]
                    (get-in (property (get-in package [:rules 0 :node-id])) [:edit-type :options]))]
      (is (= :choicebox (get-in (property rule) [:edit-type :type])))
      (is (= [["b" "b"] ["c" "c"] ["independent" "independent"]] (choices a)))
      (is (= [["c" "c"] ["independent" "independent"]] (choices b)))
      (is (= [["independent" "independent"]] (choices c)))
      (ui/run-now
        (let [control (:desc (properties-view/make-control-view (property rule) {} @test-util/localization))]
          (is (= ["c" "independent"] (:items control)))
          (is (= "c" (:value control)))
          ((:on-value-changed control) "independent")))
      (is (= "independent" (get-in (g/node-value node :save-value) [:packages 1 :rules 0 :package])))
      (is (= "− Package: independent" (@test-util/localization (:label (g/node-value rule :node-outline)))))
      (is (= [["a" "a"] ["b" "b"] ["independent" "independent"]] (choices c)))
      (g/undo! :undo/global)
      (is (= [["independent" "independent"]] (choices c)))
      (g/transact (g/set-property (:node-id c) :id "renamed"))
      (is (= [["renamed" "renamed"] ["independent" "independent"]] (choices b)))
      (is (= "renamed" (g/node-value rule :package)))
      (is (nil? (test-util/prop-error rule :package)))
      (is (coll/empty? (g/node-value node :diagnostics))))))

;; Cycles authored in text remain visible and editable, but fail validation
;; until repaired. Property errors and local build validation use Bob's result.
(deftest invalid-package-references
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [value {:packages [{:name "a" :rules [{:name "Include b" :package "b"}]}
                            {:name "b" :rules [{:name "Include c" :package "c"}]}
                            {:name "c" :rules [{:name "Subtract a" :operation :remove :package "a"}]}
                            {:name "independent"}]}]
      (test-util/make-resource! workspace "/references.lumanifest" value)
      (workspace/resource-sync! workspace)
      (let [node (project/get-resource-node project "/references.lumanifest")
            packages (g/node-value node :packages)
            rule (get-in packages [2 :rules 0 :node-id])
            error-message "Package reference cycle in rule 'Subtract a' of package 'c': c -> a -> b -> c"]
        (is (= value (g/node-value node :save-value)))
        (doseq [package packages
                rule (:rules package)]
          (is (g/error-fatal? (test-util/prop-error (:node-id rule) :package))))
        (is (= error-message (:message (test-util/prop-error rule :package))))
        (is (g/error-value? (g/node-value node :selected-paths)))
        (let [property (get-in (properties/coalesce [(g/node-value rule :_properties)]) [:properties :package])
              control (:desc (properties-view/make-control-view property {} @test-util/localization))]
          (is (= "a" (:value control)))
          (is (= ["independent"] (:items control)))
          (ui/run-now ((:on-value-changed control) "independent")))
        (is (coll/empty? (g/node-value node :diagnostics)))
        (is (nil? (test-util/prop-error rule :package)))
        (is (= #{} (g/node-value node :selected-paths)))
        (g/undo! :undo/global)
        (is (= value (g/node-value node :save-value)))
        (is (= error-message (:message (test-util/prop-error rule :package))))
        (g/transact (g/set-property rule :package "absent"))
        (is (= "Unknown package 'absent' in rule 'Subtract a' of package 'c'"
               (:message (test-util/prop-error rule :package))))))))

;; Returning to Package mode preserves valid choices and clears a retained
;; choice if unsaved edits made it cyclic while the rule was in Path mode.
(deftest package-target-mode-restoration
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (project/get-resource-node project "/packages.lumanifest")
          packages (g/node-value node :packages)
          levels-rule (get-in packages [0 :rules 0 :node-id])
          copy-rule (get-in packages [1 :rules 0 :node-id])]
      (g/transact [(g/set-property copy-rule :target :path)
                   (g/set-property copy-rule :path "/extra[.]json")])
      (g/transact [(g/set-property levels-rule :package "copy")
                   (g/set-property levels-rule :target :package)])
      (is (coll/empty? (g/node-value node :diagnostics)))
      (g/transact (g/set-property copy-rule :target :package))
      (is (= "" (g/node-value copy-rule :package)))
      (is (= "Select a package in rule 'Levels' of package 'copy'"
             (:message (test-util/prop-error copy-rule :package))))
      (g/undo! :undo/global)
      (is (= :path (g/node-value copy-rule :target)))
      (is (= "levels" (g/node-value copy-rule :package)))
      (g/transact (g/set-property levels-rule :target :path))
      (g/transact (g/set-property copy-rule :target :package))
      (is (= "levels" (g/node-value copy-rule :package)))
      (is (coll/empty? (g/node-value node :diagnostics))))))

;; Outline commands create and select nodes in one undo step, and cannot edit
;; the base or a mixed selection.
(deftest outline-commands
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (test-util/open-tab! project app-view "/packages.lumanifest")
          base (get-in (g/node-value node :node-outline) [:children 0 :node-id])
          contexts (fn [selection] [{:name :workbench :env {:selection selection :app-view app-view}}])]
      (is (test-util/handler-enabled? :edit.add-embedded-component (contexts [node]) nil))
      (is (false? (test-util/handler-enabled? :edit.add-embedded-component (contexts [base]) nil)))
      (is (false? (test-util/handler-enabled? :edit.add-embedded-component (contexts [node base]) nil)))
      (test-util/handler-run :edit.add-embedded-component (contexts [node]) nil)
      (let [package-node (get-in (g/node-value node :packages) [2 :node-id])]
        (is (= ["levels" "copy" "package"] (mapv :name (:packages (g/node-value node :save-value)))))
        (is (= [package-node] (get (g/node-value project :selected-node-ids-by-resource-node) node)))
        (test-util/handler-run :edit.add-embedded-component (contexts [node]) nil)
        (is (= ["levels" "copy" "package" "package1"] (mapv :name (:packages (g/node-value node :save-value)))))
        (g/undo! :undo/global)
        (is (= ["levels" "copy" "package"] (mapv :name (:packages (g/node-value node :save-value)))))
        (is (= [package-node] (get (g/node-value project :selected-node-ids-by-resource-node) node)))

        (test-util/handler-run :edit.add-embedded-component (contexts [package-node]) nil)
        (let [rule (get-in (g/node-value node :packages) [2 :rules 0 :node-id])]
          (is (= [{:name "Rule" :path "/.*"}] (get-in (g/node-value node :save-value) [:packages 2 :rules])))
          (is (= [rule] (get (g/node-value project :selected-node-ids-by-resource-node) node)))
          (g/undo! :undo/global)
          (is (nil? (get-in (g/node-value node :save-value) [:packages 2 :rules])))
          (is (= [package-node] (get (g/node-value project :selected-node-ids-by-resource-node) node)))
          (g/redo! :undo/global)
          (is (= [rule] (get (g/node-value project :selected-node-ids-by-resource-node) node)))

          (test-util/handler-run :edit.add-embedded-component (contexts [rule]) nil)
          (let [second-rule (get-in (g/node-value node :packages) [2 :rules 1 :node-id])]
            (is (false? (test-util/handler-enabled? :edit.reorder-up (contexts [rule]) nil)))
            (is (test-util/handler-enabled? :edit.reorder-up (contexts [second-rule]) nil))
            (test-util/handler-run :edit.reorder-up (contexts [second-rule]) nil)
            (is (= [second-rule rule] (mapv :node-id (get-in (g/node-value node :packages) [2 :rules]))))))))))

;; Resolver diagnostics and local build roots must follow unsaved manifest and
;; raw-file edits. No-match policies update property and outline diagnostics
;; when files appear or disappear; regex errors expose one-based columns.
(deftest validation-and-local-build
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (project/get-resource-node project "/packages.lumanifest")
          rule (get-in (g/node-value node :packages) [0 :rules 0 :node-id])
          raw (project/get-resource-node project "/extra.json")
          game-project (project/get-resource-node project "/game.project")]
      (doseq [[path column reason] [["[" 1 "Unclosed character class"]
                                    ["/episodes/[" 11 "Unclosed character class"]
                                    ["/episodes/(" 12 "Unclosed group"]]]
        (g/transact (g/set-property rule :path path))
        (let [error (test-util/prop-error rule :path)]
          (is (g/error-fatal? error))
          (is (= (str "Invalid regex in rule 'Episode' of package 'levels' at column " column ": " reason ". Path: " path)
                 (:message error))))
        (is (g/error-value? (g/node-value node :selected-paths)))
        (is (= (str "Invalid regex in rule 'Episode' of package 'levels' at column " column ": " reason ". Path: " path)
               (.message ^com.dynamo.bob.archive.liveupdate.PackageResolver$Diagnostic (first (g/node-value node :diagnostics))))))
      (g/transact (g/set-property rule :path "/absent/.*"))
      (is (coll/any? #(= PackageResolver$Severity/WARNING (.severity ^com.dynamo.bob.archive.liveupdate.PackageResolver$Diagnostic %))
                     (g/node-value node :diagnostics)))
      (is (g/error-warning? (test-util/prop-error rule :path)))
      (is (= "icons/32/Icons_04-Project-file.png" (:icon (g/node-value rule :node-outline))))
      (is (= "Rule 'Episode' in package 'levels' matches no source files for Path '/absent/.*'. Patterns match the entire source path, including its leading '/'."
             (.message ^com.dynamo.bob.archive.liveupdate.PackageResolver$Diagnostic (first (g/node-value node :diagnostics)))))
      (doseq [[policy fatal warning] [[:error true false] [:ignore false false] [:warn false true]]]
        (g/transact (g/set-property rule :no-match policy))
        (let [error (test-util/prop-error rule :path)]
          (is (= fatal (g/error-fatal? error)))
          (is (= warning (g/error-warning? error))))
        (is (= fatal (:outline-error? (g/node-value rule :node-outline))))
        (is (= fatal (g/error-value? (g/node-value node :selected-paths)))))
      (g/transact (g/set-property rule :no-match :error))
      (let [file (io/file (workspace/project-directory workspace) "absent/config.txt")]
        (io/make-parents file)
        (spit file "Custom configuration")
        (workspace/resource-sync! workspace)
        (is (nil? (test-util/prop-error rule :path)))
        (is (false? (:outline-error? (g/node-value rule :node-outline))))
        (is (contains? (g/node-value node :selected-paths) "/absent/config.txt"))
        (is (.delete file))
        (workspace/resource-sync! workspace)
        (is (g/error-fatal? (test-util/prop-error rule :path))))
      (g/transact (g/set-property rule :no-match :warn))
      (g/transact (g/set-property rule :path "/episode[.]collection"))
      (test-util/set-code-editor-source! raw "{\"unsaved\":true}")
      (let [targets (build/resolve-node-dependencies game-project project)]
        (is (not (g/error-value? targets)) (pr-str targets))
        (when-not (g/error-value? targets)
          (let [by-path (coll/pair-map-by #(resource/proj-path (:resource %)) identity targets)
                raw-target (get by-path "/extra.json")]
            (is (contains? by-path "/episode.collectionc"))
            (is raw-target)
            (is (= "{\"unsaved\":true}" (String. ^bytes (:user-data raw-target) java.nio.charset.StandardCharsets/UTF_8)))))))))

;; Custom-resource selection and manifest selection must share a single captured
;; target, including known extensions and repeated unsaved edits.
(deftest custom-resources-and-manifest-share-targets
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [game-project (project/get-resource-node project "/game.project")
          raw (project/get-resource-node project "/extra.json")
          manifest-node (project/get-resource-node project "/packages.lumanifest")
          rule (get-in (g/node-value manifest-node :packages) [0 :rules 1 :node-id])
          opaque-file (workspace/file-resource workspace "/opaque/data.script")
          opaque "This is custom data, not Lua."]
      (io/make-parents opaque-file)
      (spit opaque-file opaque)
      (workspace/resource-sync! workspace)
      (test-util/set-setting! game-project ["project" "custom_resources"] "/extra.json,/opaque")
      (g/transact (g/set-property rule :path "/(extra[.]json|opaque/.*)"))
      (loop [index 0 artifact-map {}]
        (when (< index 2)
          (let [expected (str "{\"review\":" index "}")]
            (test-util/set-code-editor-source! raw expected)
            (let [targets (build/resolve-node-dependencies game-project project)
                  result (g/with-auto-evaluation-context evaluation-context
                           (build/build-project! project game-project artifact-map nil evaluation-context))]
              (is (not (g/error-value? targets)))
              (is (nil? (:error result)) (pr-str (:error result)))
              (when-not (g/error-value? targets)
                (is (= 1 (count (filterv #(= "/extra.json" (resource/proj-path (:resource %))) targets)))))
              (is (= expected (slurp (workspace/build-path workspace "/extra.json"))))
              (is (= opaque (slurp (workspace/build-path workspace "/opaque/data.script"))))
              (recur (inc index) (:artifact-map result))))))
      (let [contents (coll/into-> (g/node-value project :dirty-save-data) {}
                       (keep (fn [save-data]
                               (when-let [content (resource-node/save-data-content save-data)]
                                 [(resource/proj-path (:resource save-data)) content]))))
            ^PackagePreview$Result result (build-preview! project contents "/packages.lumanifest" {} (constantly false))]
        (is (contains? (.resources ^PackageResolver$Package (get (.packages (.plan result)) "levels")) "/opaque/data.script"))
        (is (not (contains? (.resources (.plan result)) "/opaque/data.scriptc")))))))

;; Exported artifacts must not become regex inputs to editor builds or previews.
;; Resource synchronization captures ownership so graph evaluation stays pure.
(deftest exported-artifacts-are-not-sources
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/packages.lumanifest"
                              {:output-directory "."
                               :packages [{:name "episode" :rules [{:name "Files" :path "/.*[.](json|zip)"}]}]})
    (workspace/resource-sync! workspace)
    (is (= #{"/extra.json"} (g/node-value (project/get-resource-node project "/packages.lumanifest") :selected-paths)))
    (let [node (project/get-resource-node project "/packages.lumanifest")
          ^PackageResolver$Plan packaged
          (with-open [bob (Project. (.getContextClassLoader (Thread/currentThread))
                                    (DefaultFileSystem.) (str (workspace/project-directory workspace)) "build")]
            (doseq [[k v] {"platform" (.getPair (Platform/getHostPlatform))
                           "variant" "debug" "archive" "true" "liveupdate" "true"}]
              (.setOption bob k v))
            (.mount bob (ClassLoaderResourceScanner.) [])
            (doseq [^TaskResult result (.build bob (Progress/discarding) (into-array String ["build"]))]
              (is (.isOk result) (str result)))
            (.getLiveUpdatePlan bob))]
      (workspace/resource-sync! workspace)
      (is (contains? (g/node-value workspace :resource-map) "/episode.zip"))
      (is (= #{"/episode.zip"} (g/node-value project :live-update-output-paths)))
      (is (= #{"/extra.json"} (g/node-value node :selected-paths)))
      (let [sources (preview/source-resources (g/node-value workspace :resource-map)
                                              (g/node-value project :live-update-output-paths))
            ^PackagePreview$Result refreshed (build-preview! project {} "/packages.lumanifest" {} (constantly false))]
        (is (not (contains? sources "/episode.zip")))
        (is (= (.base packaged) (.base (.plan refreshed))))
        (is (= (.resources ^PackageResolver$Package (get (.packages packaged) "episode"))
               (.resources ^PackageResolver$Package (get (.packages (.plan refreshed)) "episode")))))
      (let [game-project (project/get-resource-node project "/game.project")
            targets (build/resolve-node-dependencies game-project project)]
        (is (not (g/error-value? targets)) (pr-str targets))
        (when-not (g/error-value? targets)
          (let [paths (coll/into-> targets #{} (map #(resource/proj-path (:resource %))))]
            (is (contains? paths "/extra.json"))
            (is (not (contains? paths "/episode.zip")))))))))

;; The example separates both episode ranges from shared content, keeps custom
;; text paths, and uses unsaved text in both Bob previews and local builds.
(deftest episode-packages-example
  (test-util/with-scratch-project "test/resources/live_update_packages_project"
    (let [raw (project/get-resource-node project "/episodes/00/description.txt")
          unsaved "Episode 00: Välkommen till skogen!"
          expected {"common" #{"/common/messages.txt" "/common/shared.scriptc"}
                    "episodes_common" #{"/episodes/common/actor.goc" "/episodes/common/rules.txt" "/common/shared.scriptc"}
                    "episodes_00_09" #{"/episodes/00/episode.collectionc" "/episodes/00/episode.goc" "/episodes/00/episode.scriptc" "/episodes/00/description.txt"
                                       "/episodes/09/episode.collectionc" "/episodes/09/episode.goc" "/episodes/09/episode.scriptc" "/episodes/09/description.txt"}
                    "episodes_10_19" #{"/episodes/10/episode.collectionc" "/episodes/10/episode.goc" "/episodes/10/episode.scriptc" "/episodes/10/description.txt"
                                       "/episodes/19/episode.collectionc" "/episodes/19/episode.goc" "/episodes/19/episode.scriptc" "/episodes/19/description.txt"}
                    "halloween" #{"/halloween/halloween.collectionc" "/halloween/pumpkin.goc" "/halloween/event.txt" "/common/shared.scriptc"}}]
      (test-util/set-code-editor-source! raw unsaved)
      (let [controller (preview/controller)
            resources (preview/source-resources (g/node-value workspace :resource-map)
                                                (g/node-value project :live-update-output-paths))
            statuses (:status-map (g/node-value workspace :resource-snapshot))
            contents (coll/into-> (g/node-value project :save-data) {}
                       (keep (fn [data]
                               (when-let [content (resource-node/save-data-content data)]
                                 [(resource/proj-path (:resource data)) content]))))]
        (try
          (let [^PackagePreview$Result result (preview/build! controller resources statuses contents "/liveupdate.lumanifest" {} (constantly false))
                plan (.plan result)
                graph-plan (g/with-auto-evaluation-context evaluation-context
                             (.plan ^PackagePreview$Result (preview/resolve-graph project (project/get-resource-node project "/liveupdate.lumanifest" evaluation-context)
                                                                                  evaluation-context (constantly false))))]
            (is (= (set (coll/keys expected)) (set (.keySet (.packages plan)))))
            (doseq [[name paths] expected]
              (is (= paths (set (.resources ^PackageResolver$Package (get (.packages plan) name)))) name)
              (is (= paths (set (.resources ^PackageResolver$Package (get (.packages graph-plan) name)))) name))
            (is (= (.base plan) (.base graph-plan)))
            (is (= (.requirements plan) (.requirements graph-plan)))
            (doseq [name ["episodes_00_09" "episodes_10_19"]]
              (is (= {"/episodes/common/actor.goc" ["episodes_common"]
                      "/common/shared.scriptc" ["common" "episodes_common" "halloween"]}
                     (get (.requirements plan) name))))
            (is (= unsaved (Files/readString (.resolve ^Path @(:directory @controller) "build/episodes/00/description.txt"))))
            (is (= #{"common" "episodes_common" "halloween"}
                   (coll/into-> (.packages plan) #{}
                     (keep (fn [[name package]]
                             (when (contains? (.resources ^PackageResolver$Package package) "/common/shared.scriptc")
                               name)))))))
          (finally
            (is (= true (deref (preview/close! controller) 30000 :timeout))))))
      (with-open [_ (test-util/build! (project/get-resource-node project "/game.project"))]
        (is (= unsaved (slurp (workspace/build-path workspace "/episodes/00/description.txt") :encoding "UTF-8")))
        (is (= "Welcome to the Live Update example.\n"
               (slurp (workspace/build-path workspace "/common/messages.txt") :encoding "UTF-8")))))))

;; Malformed rules remain editable without dropping either authored target.
(deftest preserve-invalid-rule-target
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [value {:packages [{:name "invalid" :rules [{:name "Both" :path "/extra[.]json" :package "missing"}]}]}]
      (test-util/make-resource! workspace "/invalid.lumanifest" value)
      (workspace/resource-sync! workspace)
      (let [node (project/get-resource-node project "/invalid.lumanifest")
            rule (get-in (g/node-value node :packages) [0 :rules 0 :node-id])]
        (is (= value (g/node-value node :save-value)))
        (is (g/error-value? (g/node-value node :selected-paths)))
        (ui/run-now
          (let [property (get-in (properties/coalesce [(g/node-value rule :_properties)]) [:properties :target])
                component (fx/create-component (properties-view/make-control-view property {} @test-util/localization))
                buttons (.getChildren ^Pane (fx/instance component))]
            (try
              (is (= [false false] (mapv #(.isSelected ^RadioButton %) buttons)))
              (is (= value (g/node-value node :save-value)))
              (.fire ^RadioButton (first buttons))
              (finally
                (fx/delete-component component)))))
        (is (= #{"/extra.json"} (g/node-value node :selected-paths)))
        (is (not (contains? (get-in (g/node-value node :save-value) [:packages 0 :rules 0]) :package)))))))

;; The editor graph preserves factory loading boundaries and shared startup
;; locks, including unsaved factory and script dependency edits.
(deftest graph-loading-and-unsaved-dependencies
  (test-util/with-scratch-project "test/resources/live_update_project"
    (doseq [[path value] {"/regular.factory" {:prototype "/regular.go"}
                          "/dynamic.factory" {:prototype "/dynamic.go" :load-dynamically true}
                          "/dynamic.collectionfactory" {:prototype "/payload.collection" :load-dynamically true}
                          "/regular.go" {:components [{:id "shared" :component "/shared.script"}]}
                          "/dynamic.go" {}
                          "/child.go" {:components [{:id "shared" :component "/shared.script"}]}
                          "/payload.collection" {:name "payload" :instances [{:id "child" :prototype "/child.go"}]}
                          "/main.go" {:components [{:id "regular" :component "/regular.factory"}
                                                   {:id "dynamic" :component "/dynamic.factory"}
                                                   {:id "collection" :component "/dynamic.collectionfactory"}]}
                          "/main.collection" {:name "main" :instances [{:id "main" :prototype "/main.go"}]}
                          "/packages.lumanifest" {:packages [{:name "content" :rules [{:name "Content" :path "/(regular|dynamic|child)[.]go|/payload[.]collection"}]}]}}]
      (test-util/make-resource! workspace path value))
    (spit (workspace/file-resource workspace "/shared.script") "function init(self) end\n")
    (spit (workspace/file-resource workspace "/extra.lua") "return {}\n")
    (workspace/resource-sync! workspace)
    (let [node (project/get-resource-node project "/packages.lumanifest")
          resolve! (fn resolve! []
                     (g/with-auto-evaluation-context evaluation-context
                       (.plan ^PackagePreview$Result (preview/resolve-graph project node evaluation-context (constantly false)))))
          ^PackageResolver$Plan initial (resolve!)]
      (is (contains? (.locks initial) "/regular.goc"))
      (is (contains? (.locks initial) "/payload.collectionc"))
      (is (contains? (.locks initial) "/shared.scriptc"))
      (is (not (contains? (.locks initial) "/child.goc")))
      (is (not (contains? (.locks initial) "/dynamic.goc")))
      (test-util/prop! (project/get-resource-node project "/dynamic.factory") :load-dynamically false)
      (is (contains? (.locks ^PackageResolver$Plan (resolve!)) "/dynamic.goc"))
      (test-util/set-code-editor-source! (project/get-resource-node project "/shared.script") "local extra = require 'extra'\n")
      (let [^PackageResolver$Plan changed (resolve!)]
        (is (contains? (.base changed) "/extra.luac"))
        (is (contains? (.resources ^PackageResolver$Package (get (.packages changed) "content")) "/extra.luac"))))))

;; Registered views refresh from unsaved edits and inventory without compiling.
;; Reordering retains the controls and cached editor targets; closing and
;; reopening disposes the old view and creates a fresh preview.
(deftest graph-preview
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (project/get-resource-node project "/packages.lumanifest")
          raw (project/get-resource-node project "/extra.json")
          packages (g/node-value node :packages)
          episode (get-in packages [0 :rules 0 :node-id])
          old-controller (atom nil)]
      (with-manifest-view! project app-view "/packages.lumanifest"
        (fn [{:keys [controller parent preferences]}]
          (reset! old-controller controller)
          (let [initial (:result @controller)
                ^PackagePreview$Result compiled (get-in initial [:value :compiled])
                table (resource-table parent)
                form (first (.getChildren ^Pane parent))
                children (vec (.getChildren ^Pane form))
                episode-node (project/get-resource-node project "/episode.collection")
                episode-targets (g/node-value episode-node :build-targets)]
            (is (= #{"/episode.collectionc" "/extra.json"}
                   (set (.resources ^PackageResolver$Package (get (.packages (.plan compiled)) "levels")))))
            (g/transact {:undoable false} (app-view/select app-view node [episode]))
            (await-ui! #(= "Package: levels" (.getText ^Label (.lookup ^Pane parent "#liveupdate-package-heading"))))
            (is (identical? initial (:result @controller)))
            (is (nil? (:job @controller)))
            (test-util/handler-run :edit.reorder-down
                                   [{:name :workbench :env {:selection [episode]}}] nil)
            (let [^PackagePreview$Result result (await-preview! controller initial)]
              (is (= "Data" (.getName (.getRules (.description ^PackageResolver$Package (get (.packages (.plan result)) "levels")) 0))))
              (is (identical? episode-targets (g/node-value episode-node :build-targets))))
            (is (= children (vec (.getChildren ^Pane form))))
            (is (identical? table (resource-table parent))))
          (let [previous (:result @controller)]
            (g/transact (g/set-property node :output-directory "exports/packages"))
            (await-preview! controller previous)
            (await-ui! #(= "exports/packages/levels.zip" (.getText ^Label (.lookup ^Pane parent "#liveupdate-export-path")))))
          (let [^Label path-label (.lookup ^Pane parent "#liveupdate-export-path")
                clipboard (Clipboard/getSystemClipboard)
                previous (coll/pair-map-by identity #(.getContent clipboard %) (.getContentTypes clipboard))]
            (is (= ["Copy Path" "Open Folder"]
                   (mapv #(.getText ^MenuItem %) (.getItems (.getContextMenu path-label)))))
            (try
              (.fire ^MenuItem (first (.getItems (.getContextMenu path-label))))
              (is (= (io/file (workspace/project-directory workspace) "exports/packages/levels.zip")
                     (io/file (.getString clipboard))))
              (finally
                (.setContent clipboard previous))))
          (let [previous (:result @controller)]
            (g/transact (g/set-property episode :path "/nothing[.]collection"))
            (test-util/set-code-editor-source! raw "{\"unsaved\":true}")
            (await-preview! controller previous)
            (is (not (realized? (:directory @controller))))
            (await-ui! #(= [["/extra.json" []]] (table-tree (resource-table parent))))
            (let [row (.getValue ^TreeItem (first (.getChildren (.getRoot (resource-table parent)))))]
              (is (= "/extra.json" (:source row)))
              (is (= [(get-in packages [0 :rules 1 :node-id])] (:rule-nodes row)))
              (is (= "Data" (:added-by row)))))
          (let [previous (:result @controller)]
            (test-util/make-resource! workspace "/nothing.collection" {:name "new"})
            (workspace/resource-sync! workspace)
            (let [^PackagePreview$Result result (await-preview! controller previous)]
              (is (= #{"/nothing.collectionc" "/extra.json"}
                     (set (.resources ^PackageResolver$Package (get (.packages (.plan result)) "levels")))))))
          (let [previous (:result @controller)]
            (prefs/set! preferences [:bundle :last-bob-options] {"variant" "release"})
            (is (identical? previous (:result @controller))))))
      (is (:closed @@old-controller))
      (with-manifest-view! project app-view "/packages.lumanifest"
        (fn [{:keys [controller]}]
          (is (not (identical? @old-controller controller)))
          (is (not (realized? (:directory @controller))))
          (is (instance? PackagePreview$Result (get-in @controller [:result :value :compiled]))))))))

;; Refreshes preserve unchanged snapshot files.
;; Disk edits, unsaved edits, saves, and removals still update the snapshot.
(deftest preview-reuses-unchanged-sources
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [source (io/file (workspace/file-resource workspace "/unselected.bin"))
          controller (preview/controller)
          build! (fn build! [contents]
                   (preview/build! controller
                                   (preview/source-resources (g/node-value workspace :resource-map)
                                                             (g/node-value project :live-update-output-paths))
                                   (:status-map (g/node-value workspace :resource-snapshot))
                                   contents "/packages.lumanifest" {} (constantly false)))]
      (try
        (with-open [output (io/output-stream source)]
          (.write output (byte-array 4096)))
        (workspace/resource-sync! workspace)
        (build! {})
        (let [node (project/get-resource-node project "/packages.lumanifest")
              rule (get-in (g/node-value node :packages) [0 :rules 0 :node-id])]
          (g/set-property! rule :id "Renamed rule")
          (let [snapshot (.resolve ^Path @(:directory @controller) "unselected.bin")
                unchanged (FileTime/fromMillis 946684800000)]
            (Files/setLastModifiedTime snapshot unchanged)
            (let [^PackagePreview$Result result (build! {"/packages.lumanifest" (resource-node/save-data-content (g/node-value node :save-data))})]
              (is (= "Renamed rule" (.getName (.getRules (.description ^PackageResolver$Package (get (.packages (.plan result)) "levels")) 0)))))
            (is (= unchanged (Files/getLastModifiedTime snapshot (make-array java.nio.file.LinkOption 0))))))
        (let [raw (workspace/file-resource workspace "/extra.json")
              snapshot (.resolve ^Path @(:directory @controller) "extra.json")
              compiled (.resolve ^Path @(:directory @controller) "build/extra.json")
              disk-content "{\"disk\":true}"
              unsaved "{\"unsaved\":true}"
              unchanged (FileTime/fromMillis 946684800000)]
          (spit raw disk-content)
          (workspace/resource-sync! workspace)
          (build! {})
          (is (= disk-content (Files/readString compiled)))
          (is (= disk-content (Files/readString snapshot)))
          (test-util/set-code-editor-source! (project/get-resource-node project "/extra.json") unsaved)
          (build! {"/extra.json" unsaved})
          (is (= unsaved (Files/readString compiled)))
          (is (= unsaved (Files/readString snapshot)))
          (Files/setLastModifiedTime snapshot unchanged)
          (test-util/save-project! project)
          (build! {})
          (is (= unchanged (Files/getLastModifiedTime snapshot (make-array java.nio.file.LinkOption 0)))))
        (io/delete-file source)
        (workspace/resource-sync! workspace)
        (build! {})
        (is (not (.exists (.toFile (.resolve ^Path @(:directory @controller) "unselected.bin")))))
        (finally
          (is (= true (deref (preview/close! controller) 30000 :timeout))))))))

(defn- start-paused-preview! [project controller manifest-path]
  (let [manifest-node (project/get-resource-node project manifest-path)
        packages (g/node-value manifest-node :packages)
        evaluation-context (g/make-evaluation-context)
        fingerprint (:fingerprint @controller)
        started (promise)
        release (promise)
        first-check (atom true)]
    (preview/invalidate! controller)
    {:started started :release release
     :completion
     (preview/update! controller fingerprint
                      (fn [cancelled?]
                        {:packages packages
                         :compiled (preview/resolve-graph project manifest-node evaluation-context
                                                          (fn []
                                                            (when (compare-and-set! first-check true false)
                                                              (deliver started true)
                                                              (when-not (= true (deref release 30000 :timeout))
                                                                (throw (ex-info "Timed out waiting to release graph traversal" {}))))
                                                            (cancelled?)))}))}))

;; Edits during graph traversal discard obsolete results. A read failure keeps
;; the existing table usable, and restoring the file allows another refresh.
(deftest preview-superseded-and-failed
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/bonus.collection" {:name "bonus"})
    (workspace/resource-sync! workspace)
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [controller parent]}]
        (let [node (project/get-resource-node project "/packages.lumanifest")
              rule (get-in (g/node-value node :packages) [0 :rules 0 :node-id])
              table (resource-table parent)
              filter-control (.lookup ^Pane parent "#liveupdate-filter")
              previous (:result @controller)
              work (start-paused-preview! project controller "/packages.lumanifest")
              job (:job @controller)]
          (try
            (await-ui! #(realized? (:started work)))
            (g/transact (g/set-property rule :path "/nothing[.]collection"))
            (await-ui! #(deref (:cancelled job)))
            (g/transact (g/set-property rule :path "/bonus[.]collection"))
            (is (identical? previous (:result @controller)))
            (is (identical? table (resource-table parent)))
            (is (identical? filter-control (.lookup ^Pane parent "#liveupdate-filter")))
            (deliver (:release work) true)
            (let [^PackagePreview$Result result (await-preview! controller previous)]
              (is (= #{"/bonus.collectionc" "/extra.json"}
                     (set (.resources ^PackageResolver$Package (get (.packages (.plan result)) "levels"))))))
            (finally
              (deliver (:release work) true)
              (await-ui! #(realized? (:completion work)))))
          (let [source (workspace/file-resource workspace "/extra.json")
                original (slurp source)
                previous (:result @controller)
                work (start-paused-preview! project controller "/packages.lumanifest")]
            (try
              (await-ui! #(realized? (:started work)))
              (io/delete-file source)
              (deliver (:release work) true)
              (await-ui! #(and (realized? (:completion work)) (some? (:error @controller))))
              (await-ui! #(let [form (first (.getChildren ^Pane parent))
                                diagnostics (last (.getChildren ^Pane form))]
                            (and (instance? ScrollPane diagnostics)
                                 (.contains (.getText ^Label (.getContent ^ScrollPane diagnostics))
                                            ^String (ex-message (:error @controller))))))
              (is (identical? previous (:result @controller)))
              (is (identical? table (resource-table parent)))
              (is (identical? filter-control (.lookup ^Pane parent "#liveupdate-filter")))
              (spit source original)
              (preview/invalidate! controller)
              (await-preview! controller previous)
              (is (nil? (:error @controller)))
              (finally
                (spit source original)
                (deliver (:release work) true)
                (await-ui! #(realized? (:completion work)))))))))))

;; Closing a registered tab cancels its active graph traversal and waits for
;; completion. Closing releases the traversal gate without a sleep.
(deftest close-active-preview
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [closed-during-work (promise)
          cleanup (atom nil)]
      (try
        (with-manifest-view! project app-view "/packages.lumanifest"
          (fn [{:keys [controller]}]
            (let [work (start-paused-preview! project controller "/packages.lumanifest")]
              (reset! cleanup [controller work])
              (add-watch controller ::closing
                         (fn [_ _ previous current]
                           (when (and (:closed current) (not (:closed previous)))
                             (deliver closed-during-work (not (realized? (:completion work))))
                             (deliver (:release work) true))))
              (await-ui! #(realized? (:started work))))))
        (is (= true (deref closed-during-work 30000 :timeout)))
        (is (nil? (:result @(first @cleanup))))
        (finally
          (when-let [[controller work] @cleanup]
            (deliver (:release work) true)
            (is (= true (deref (:completion work) 30000 :timeout)))
            (remove-watch controller ::closing)))))))

;; Real status-bar cancellation stops an in-flight build, preserves the last
;; result, and never cancels another view. Closing releases the preview cache.
(deftest delayed-preview-progress
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/other.lumanifest"
                              {:packages [{:name "other" :rules [{:name "Data" :path "/extra[.]json"}]}]})
    (workspace/resource-sync! workspace)
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [controller preferences]}]
        (let [previous (:result @controller)
              first-work (start-paused-preview! project controller "/packages.lumanifest")
              second-view (open-manifest-view! project app-view "/other.lumanifest" preferences)]
          (try
            (await-ui! #(realized? (:started first-work)))
            (await-preview! (:controller second-view) nil)
            (let [other-controller (:controller second-view)
                  other-work (start-paused-preview! project other-controller "/other.lumanifest")
                  {:keys [^Button progress-cancel-button ^Label status-label ^Pane progress-hbox]}
                  (ui/collect-controls (.getRoot (.getScene (ui/main-stage)))
                                       ["progress-cancel-button" "status-label" "progress-hbox"])]
              (try
                (await-ui! #(and (realized? (:started other-work)) (.isVisible progress-cancel-button)))
                (is (= "Preparing Live Update preview" (.getText status-label)))
                (.fire progress-cancel-button)
                (await-ui! #(deref (get-in @controller [:job :cancelled])))
                (is (false? @(get-in @other-controller [:job :cancelled])))
                (is (identical? previous (:result @controller)))
                (deliver (:release first-work) true)
                (await-ui! #(realized? (:completion first-work)))
                (is (identical? previous (:result @controller)))
                (await-ui! #(and (.isVisible progress-hbox) (not (.isDisabled progress-cancel-button))))
                (deliver (:release other-work) true)
                (await-ui! #(realized? (:completion other-work)))
                (await-ui! #(not (.isVisible progress-hbox)))
                (is (nil? (:error @other-controller)))
                (is (instance? PackagePreview$Result (get-in @other-controller [:result :value :compiled])))
                (finally
                  (deliver (:release other-work) true)
                  (await-ui! #(realized? (:completion other-work))))))
            (finally
              (deliver (:release first-work) true)
              (await-ui! #(realized? (:completion first-work)))
              (close-manifest-view! second-view))))))))

;; Pending table preparation must allow UI input, discard superseded queries,
;; and reuse the compiled graph when only the selection or search changes.
(deftest asynchronous-resource-rows
  (test-util/with-scratch-project "test/resources/live_update_project"
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [controller rows-controller parent]}]
        (let [compiled-result (:result @controller)
              previous (:result @rows-controller)
              fingerprint (:fingerprint @rows-controller)
              started (promise)
              release (promise)
              _ (preview/invalidate! rows-controller)
              completion (preview/update! rows-controller fingerprint
                                          (fn [cancelled?]
                                            (deliver started (not (ui/on-ui-thread?)))
                                            (when-not (= true (deref release 30000 :timeout))
                                              (throw (ex-info "Timed out waiting to release table preparation" {})))
                                            (is (cancelled?))
                                            (:value previous)))
              job (:job @rows-controller)
              filter-control ^TextField (.lookup ^Pane parent "#liveupdate-filter")
              node (project/get-resource-node project "/packages.lumanifest")
              package-node (get-in (g/node-value node :packages) [0 :node-id])]
          (try
            (await-ui! #(realized? started))
            (is (true? @started))
            (g/transact {:undoable false} (app-view/select app-view node [package-node]))
            (.setText filter-control "episode")
            (await-ui! #(deref (:cancelled job)))
            (.setText filter-control "extra.json")
            (await-ui! #(= "extra.json" (peek (:fingerprint @rows-controller))))
            (is (= "extra.json" (.getText filter-control)))
            (is (identical? previous (:result @rows-controller)))
            (deliver release true)
            (await-ui! #(= [["/extra.json" []]] (table-tree (resource-table parent))))
            (is (= "extra.json" (peek (get-in @rows-controller [:result :fingerprint]))))
            (is (identical? compiled-result (:result @controller)))
            (is (nil? (:job @controller)))
            (finally
              (deliver release true)
              (await-ui! #(realized? completion)))))))))

;; Package views show dependency trees; global searches list all supplying
;; packages and rules. Selecting a resource highlights its rules, and the source
;; hyperlink opens its actual resource through the editor command context.
(deftest graph-preview-tree
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/shared.script")
    (test-util/make-resource! workspace "/shared.go"
                              {:components [{:id "script" :component "/shared.script"}]})
    (doseq [name ["episode" "bonus" "main"]]
      (test-util/make-resource! workspace (str "/" name ".collection")
                                {:name name :instances [{:id "shared" :prototype "/shared.go"}]}))
    (test-util/make-resource! workspace "/packages.lumanifest"
                              {:packages [{:name "levels"
                                           :rules [{:name "Episodes" :path "/(episode|bonus)[.]collection"}
                                                   {:name "Data" :path "/extra[.]json"}
                                                   {:name "Script" :path "/shared[.]script"}]}
                                          {:name "copy" :provider "folder" :rules [{:name "Levels" :package "levels"}]}]})
    (workspace/resource-sync! workspace)
    (test-util/set-setting! (project/get-resource-node project "/game.project")
                            ["project" "custom_resources"] "/extra.json")
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [parent ^Tab tab]}]
        (let [node (project/get-resource-node project "/packages.lumanifest")
              packages (g/node-value node :packages)
              table (resource-table parent)
              filter-control ^TextField (.lookup ^Pane parent "#liveupdate-filter")
              clear-button ^Button (.lookup ^Pane parent "#liveupdate-clear-search")
              heading ^Label (.lookup ^Pane parent "#liveupdate-package-heading")
              expected [["/bonus.collectionc" [["/shared.goc" [["/shared.scriptc" []]]]]]
                        ["/episode.collectionc" [["/shared.goc" [["/shared.scriptc" []]]]]]
                        ["/extra.json" []]
                        ["/shared.scriptc" []]]]
          (g/transact {:undoable false} (app-view/select app-view node [(get-in packages [0 :node-id])]))
          (await-ui! #(= expected (table-tree table)))
          (is (= "Package: levels" (.getText heading)))
          (is (false? (.isShowRoot table)))
          (let [^TreeItem collection (first (.getChildren (.getRoot table)))
                ^TreeItem go (first (.getChildren collection))
                ^TreeItem script (first (.getChildren go))]
            (.select (.getSelectionModel table) script)
            (await-ui! #(.lookup ^Pane parent "#liveupdate-source"))
            (is (= "/shared.script" (.getText ^Hyperlink (.lookup ^Pane parent "#liveupdate-source"))))
            (is (= #{(get-in packages [0 :rules 0 :node-id]) (get-in packages [0 :rules 2 :node-id])}
                   (set (get (g/node-value project :selected-node-ids-by-resource-node) node))))
            (is (= "Package: levels" (.getText heading)))
            (.clearSelection (.getSelectionModel table))
            (await-ui! #(nil? (.lookup ^Pane parent "#liveupdate-source"))))
          (.setText filter-control "SHARED.SCRIPT")
          (await-ui! #(= [["/shared.scriptc" []]] (table-tree table)))
          (is (= "Search results" (.getText heading)))
          (is (nil? (.lookup ^Pane parent "#liveupdate-export-path")))
          (let [^TreeItem match (first (.getChildren (.getRoot table)))
                ^TreeTableColumn packages-column (nth (.getColumns table) 1)]
            (is (= ["base" "copy" "levels"] (:packages (.getValue match))))
            (is (= "Base, copy, levels" (.getCellData packages-column match)))
            (is (= #{(get-in packages [0 :rules 0 :node-id])
                     (get-in packages [0 :rules 2 :node-id])
                     (get-in packages [1 :rules 0 :node-id])}
                   (set (:rule-nodes (.getValue match)))))
            (.select (.getSelectionModel table) match))
          (.fire clear-button)
          (await-ui! #(= expected (table-tree table)))
          (is (= "" (.getText filter-control)))
          (is (.isDisabled clear-button))
          (g/transact {:undoable false} (app-view/select app-view node [(get-in packages [1 :node-id])]))
          (await-ui! #(= "Package: copy" (.getText heading)))
          (is (= expected (table-tree table)))
          (is (= "build/packages/copy" (.getText ^Label (.lookup ^Pane parent "#liveupdate-export-path"))))
          (is (nil? (.getSelectedItem (.getSelectionModel table))))
          (g/transact {:undoable false} (app-view/select app-view node [node]))
          (await-ui! #(= "Package: Base" (.getText heading)))
          (is (= [["/shared.goc" [["/shared.scriptc" []]]]] (get (into {} (table-tree table)) "/main.collectionc")))
          (is (not (contains? (into {} (table-tree table)) "/extra.json")))
          (.setText filter-control "episode.collection")
          (await-ui! #(= [["/episode.collectionc" []]] (table-tree table)))
          (is (= ["copy" "levels"] (:packages (.getValue ^TreeItem (first (.getChildren (.getRoot table)))))))
          (.setText filter-control "no-match")
          (await-ui! #(= "0 resources" (.getText ^Label (.lookup ^Pane parent "#liveupdate-resource-summary"))))
          (is (coll/empty? (.getChildren (.getRoot table))))
          (.setText filter-control "shared.script")
          (await-ui! #(= [["/shared.scriptc" []]] (table-tree table)))
          (.select (.getSelectionModel table) ^TreeItem (first (.getChildren (.getRoot table))))
          (await-ui! #(.lookup ^Pane parent "#liveupdate-source"))
          (.fire ^Hyperlink (.lookup ^Pane parent "#liveupdate-source"))
          (let [pane (.getTabPane tab)
                source-tab (await-ui! #(let [selected (.getSelectedItem (.getSelectionModel pane))]
                                         (when (not (identical? selected tab)) selected)))
                source-view (editor-tab/view-node-id source-tab)]
            (try
              (is (= (project/get-resource-node project "/shared.script")
                     (g/node-value source-view :resource-node)))
              (finally
                (Event/fireEvent source-tab (Event. Tab/CLOSED_EVENT))
                (.remove (.getTabs pane) source-tab)
                (await-ui! #(nil? (g/node-by-id source-view)))))))))))

;; Preview changes resolve selection details from current rows, and removing the
;; selected resource clears its source link without leaving stale trace text.
(deftest graph-preview-selection-details
  (test-util/with-scratch-project "test/resources/live_update_project"
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [controller parent]}]
        (let [node (project/get-resource-node project "/packages.lumanifest")
              package (first (g/node-value node :packages))
              rule (get-in package [:rules 1 :node-id])
              table (resource-table parent)]
          (g/transact {:undoable false} (app-view/select app-view node [(:node-id package)]))
          (let [^TreeItem item (await-ui! #(coll/first-where
                                             (fn [^TreeItem item] (= "/extra.json" (:path (.getValue item))))
                                             (.getChildren (.getRoot table))))]
            (.select (.getSelectionModel table) item))
          (await-ui! #(.lookup ^Pane parent "#liveupdate-source"))
          (let [previous (:result @controller)]
            (g/transact (g/set-property rule :path "/episode[.]collection"))
            (await-preview! controller previous))
          (await-ui! #(nil? (.lookup ^Pane parent "#liveupdate-source")))
          (is (coll/not-any? #(= "/extra.json" (:path %)) (table-rows table)))
          (is (nil? (.getSelectedItem (.getSelectionModel table)))))))))

;; Generated editor targets retain their atlas source in package, base, and
;; global views, even though the texture filename differs from Bob's output.
(deftest graph-preview-source-attribution
  (test-util/with-scratch-project "test/resources/live_update_project"
    (io/copy (io/file "test/resources/spine_project/assets/spineboy/images/muzzle01.png")
             (io/file (workspace/file-resource workspace "/test.png")))
    (test-util/make-resource! workspace "/atlas.atlas" {:images [{:image "/test.png"}]})
    (test-util/make-resource! workspace "/atlas.sprite"
                              {:default-animation "test"
                               :material "/builtins/materials/sprite.material"
                               :textures [{:sampler "texture_sampler" :texture "/atlas.atlas"}]})
    (test-util/make-resource! workspace "/main.go"
                              {:components [{:id "sprite" :component "/atlas.sprite"}]})
    (test-util/make-resource! workspace "/main.collection"
                              {:name "main" :instances [{:id "main" :prototype "/main.go"}]})
    (test-util/make-resource! workspace "/packages.lumanifest"
                              {:packages [{:name "episode" :rules [{:name "Atlas" :path "/atlas[.]atlas"}]}]})
    (workspace/resource-sync! workspace)
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [parent]}]
        (let [node (project/get-resource-node project "/packages.lumanifest")
              package-node (get-in (g/node-value node :packages) [0 :node-id])
              table (resource-table parent)
              ^TextField filter-control (.lookup ^Pane parent "#liveupdate-filter")]
          (doseq [[selection query heading] [[[package-node] "" "Package: episode"]
                                             [[node] "" "Package: Base"]
                                             [[node] "atlas" "Search results"]]]
            (g/transact {:undoable false} (app-view/select app-view node selection))
            (.setText filter-control query)
            (await-ui! #(= heading (.getText ^Label (.lookup ^Pane parent "#liveupdate-package-heading"))))
            (let [atlas-rows (filterv (fn [{:keys [^String path]}]
                                        (or (= "/atlas.a.texturesetc" path) (.endsWith path ".texturec")))
                                      (table-rows table))]
              (is (= 2 (count atlas-rows)))
              (is (= #{"/atlas.atlas"} (set (mapv :source atlas-rows)))))))))))

;; Base keeps deferred content and subtrees disconnected by package placement
;; nested and visible in a compiled project.
(deftest base-preview-tree
  (test-util/with-scratch-project "test/resources/live_update_project"
    (spit (workspace/file-resource workspace "/custom.json") "{}")
    (test-util/make-resource! workspace "/a.script")
    (test-util/make-resource! workspace "/z.go" {:components [{:id "script" :component "/a.script"}]})
    (test-util/make-resource! workspace "/resident.go" {})
    (test-util/make-resource! workspace "/resident.collection"
                              {:name "resident" :instances [{:id "resident" :prototype "/resident.go"}]})
    (test-util/make-resource! workspace "/episode.collection"
                              {:name "episode" :instances [{:id "z" :prototype "/z.go"}]})
    (doseq [name ["episode" "resident"]]
      (test-util/make-resource! workspace (str "/" name ".collectionproxy")
                                {:collection (str "/" name ".collection")}))
    (test-util/make-resource! workspace "/main.go"
                              {:components [{:id "episode" :component "/episode.collectionproxy"}
                                            {:id "resident" :component "/resident.collectionproxy"}]})
    (test-util/make-resource! workspace "/main.collection"
                              {:name "main" :instances [{:id "main" :prototype "/main.go"}]})
    (test-util/make-resource! workspace "/packages.lumanifest"
                              {:packages [{:name "levels"
                                           :rules [{:name "Episode" :path "/episode[.]collection"}
                                                   {:name "Keep in base" :operation :remove :path "/z[.]go"}]}]})
    (workspace/resource-sync! workspace)
    (test-util/set-setting! (project/get-resource-node project "/game.project")
                            ["project" "custom_resources"] "/custom.json")
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [parent]}]
        (let [node (project/get-resource-node project "/packages.lumanifest")
              table (resource-table parent)]
          (g/transact {:undoable false} (app-view/select app-view node [node]))
          (await-ui! #(= "Package: Base" (.getText ^Label (.lookup ^Pane parent "#liveupdate-package-heading"))))
          (is (= {"/custom.json" []
                  "/main.collectionc" [["/main.goc" [["/episode.collectionproxyc" []]
                                                     ["/resident.collectionproxyc" [["/resident.collectionc" [["/resident.goc" []]]]]]]]]
                  "/z.goc" [["/a.scriptc" []]]}
                 (select-keys (into {} (table-tree table))
                              ["/custom.json" "/main.collectionc" "/z.goc"]))))))))

;; The controller coalesces queued changes and never restarts unchanged work.
;; All blocked jobs are released and awaited even if an assertion fails.
(deftest preview-lifecycle
  (let [controller (preview/controller)
        started (promise)
        release (promise)
        finished (preview/update! controller :old
                                  (fn [cancelled?]
                                    (deliver started true)
                                    (is (= true (deref release 30000 :timeout)))
                                    (is (cancelled?))
                                    :obsolete))]
    (try
      (is (= true (deref started 30000 :timeout)))
      (is (nil? (preview/update! controller :old (fn [_] (is false "Unchanged inputs must not rebuild")))))
      (let [queued-finished (preview/update! controller :queued (fn [_] (is false "Superseded queued work must not run")))
            new-finished (preview/update! controller :new (constantly :current))]
        (is (not (realized? new-finished)))
        (deliver release true)
        (is (= true (deref finished 30000 :timeout)))
        (is (= true (deref queued-finished 30000 :timeout)))
        (is (= true (deref new-finished 30000 :timeout)))
        (is (= :current (get-in @controller [:result :value])))
        (is (nil? (preview/update! controller :new (fn [_] (is false "Completed inputs must not rebuild"))))))
      (finally
        (deliver release true)
        (is (= true (deref (preview/close! controller) 30000 :timeout)))))))

;; Library manifests expose the same outline and selection with read-only edits.
(deftest library-manifest-read-only
  (test-util/with-project-default-library-directory
    (test-util/with-scratch-project "test/resources/live_update_project"
      (let [uri (URI/create "file:/liveupdate-manifest-test")
            library (test-support/library-file (workspace/project-directory workspace) uri "")]
        (io/make-parents library)
        (with-open [output (ZipOutputStream. (io/output-stream library))]
          (doseq [[path content] [["game.project" "[library]\ninclude_dirs = packages\n"]
                                  ["packages/library.lumanifest" "packages { name: \"library\" rules { name: \"Data\" path: \"/packages/data[.]json\" } }"]
                                  ["packages/data.json" "{}"]]]
            (.putNextEntry output (ZipEntry. ^String path))
            (.write output (.getBytes ^String content StandardCharsets/UTF_8))
            (.closeEntry output)))
        (test-util/set-cached-project-dependencies! workspace [uri])
        (workspace/resource-sync! workspace)
        (let [node (project/get-resource-node project "/packages/library.lumanifest")
              package (first (g/node-value node :packages))
              rule (first (:rules package))]
          (is (= #{"/packages/data.json"} (g/node-value node :selected-paths)))
          (is (g/node-value node :read-only))
          (is (get-in (g/node-value node :_properties) [:properties :live-update-provider.zip/compression-level :read-only?]))
          (is (:read-only (g/node-value (:node-id package) :node-outline)))
          (is (:read-only (g/node-value (:node-id rule) :node-outline)))
          (is (get-in (g/node-value (:node-id package) :_properties) [:properties :provider :read-only?]))
          (is (get-in (g/node-value (:node-id rule) :_properties) [:properties :path :read-only?]))
          (ui/run-now
            (doseq [prop-kw [:operation :target :no-match]]
              (let [property (get-in (properties/coalesce [(g/node-value (:node-id rule) :_properties)]) [:properties prop-kw])
                    component (fx/create-component (properties-view/make-control-view property {} @test-util/localization))
                    buttons (.getChildren ^Pane (fx/instance component))
                    original (g/node-value node :save-value)]
                (try
                  (is (coll/every? #(.isDisabled ^RadioButton %) buttons))
                  (.fire ^RadioButton (second buttons))
                  (is (= original (g/node-value node :save-value)))
                  (finally
                    (fx/delete-component component))))))
          (doseq [selected [node (:node-id package) (:node-id rule)]
                  command [:edit.add-embedded-component :edit.reorder-up :edit.reorder-down]]
            (is (false? (test-util/handler-enabled? command
                                                    [{:name :workbench :env {:selection [selected]}}] nil)))))))))
