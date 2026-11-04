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
            [editor.code.view :as code-view]
            [editor.defold-project :as project]
            [editor.editor-tab :as editor-tab]
            [editor.fs :as fs]
            [editor.live-update-manifest :as manifest]
            [editor.live-update-manifest-view :as manifest-view]
            [editor.properties :as properties]
            [editor.properties-view :as properties-view]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.java :as java]
            [support.test-support :as test-support]
            [util.coll :as coll])
  (:import [com.dynamo.bob ClassLoaderResourceScanner Platform Progress Project TaskResult]
           [com.dynamo.bob.archive.liveupdate PackageArtifacts PackageBuilder PackageResolver$Package PackageResolver$Plan PackageResolver$Severity]
           [com.dynamo.bob.fs DefaultFileSystem]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ManifestDesc]
           [java.net URI]
           [java.nio.charset StandardCharsets]
           [java.util.zip ZipEntry ZipOutputStream]
           [javafx.animation AnimationTimer PauseTransition]
           [javafx.event Event]
           [javafx.scene Scene]
           [javafx.scene.control Button Hyperlink Label MenuItem RadioButton SplitPane Tab TabPane TextField TreeItem TreeTableColumn TreeTableView]
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
       :preferences preferences :state (g/user-data view :preview-state)})))

(defn- close-manifest-view! [{:keys [^Tab tab view state]}]
  (let [job (:job @state)]
    (Event/fireEvent tab (Event. Tab/CLOSED_EVENT))
    (.remove (.getTabs (.getTabPane tab)) tab)
    (await-ui! #(nil? (g/node-by-id view)))
    (when job (is (future-cancelled? job)))
    (is (= {} @state))))

(defn- await-preview! [state previous]
  (await-ui!
    (fn []
      (let [{:keys [job result error]} @state]
        (when error (throw error))
        (when (and (nil? job) result (not (identical? result previous)))
          (:compiled result))))))

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
    (let [preferences (test-util/make-test-prefs)
          view (open-manifest-view! project app-view path preferences)]
      (try
        (let [compiled (await-preview! (:state view) nil)]
          (await-ui! #(when-let [table (resource-table (:parent view))]
                        (= (.base ^PackageResolver$Plan (:plan compiled))
                           (coll/into-> (table-rows table) #{} (map :path))))))
        (f view)
        (finally
          (close-manifest-view! view))))))

(defn- build-bob-plan!
  ^PackageResolver$Plan [project contents manifest-path]
  (let [workspace (project/workspace project)
        generated-paths (g/node-value project :live-update-output-paths)]
    (test-util/with-temp-dir! snapshot
      (doseq [[path source] (g/node-value workspace :resource-map)
              :when (and (= :file (resource/source-type source))
                         (not (.startsWith ^String path "/builtins/"))
                         (not (PackageArtifacts/isGeneratedPath path generated-paths)))]
        (let [destination (io/file snapshot (subs path 1))]
          (fs/create-parent-directories! destination)
          (if-let [content (get contents path)]
            (spit destination content :encoding "UTF-8")
            (with-open [input (io/input-stream source)]
              (io/copy input destination)))))
      (PackageBuilder/buildPlan (.toPath snapshot) manifest-path {} java/class-loader (Progress/discarding)))))

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
      (is (not (.exists (io/file root "exported packages"))))
      (test-util/make-resource! workspace "/reloaded.lumanifest" (g/node-value node :save-value))
      (workspace/resource-sync! workspace)
      (is (= "exported packages" (g/node-value (project/get-resource-node project "/reloaded.lumanifest") :output-directory))))))

;; Provider defaults ignore legacy settings. Explicit overrides use normal
;; property editing, history and reload.
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
      (is (= [1] (properties/values (property))))
      (test-util/set-setting! (project/get-resource-node project "/liveupdate.settings") ["liveupdate" "zip-compression-level"] 7)
      (is (= [1] (properties/values (property))))
      (ui/run-now
        (let [control (:desc (properties-view/make-control-view (property) {} @test-util/localization))]
          ((:on-value-changed control) 9)))
      (is (= [{:provider "zip" :properties [{:name "compression-level" :value "9"}]}]
             (:providers (g/node-value node :save-value))))
      (is (= [9] (properties/values (property))))
      (is (properties/overridden? (property)))
      (g/undo! :undo/global)
      (is (= [1] (properties/values (property))))
      (g/redo! :undo/global)
      (test-util/make-resource! workspace "/provider-reloaded.lumanifest" (g/node-value node :save-value))
      (workspace/resource-sync! workspace)
      (is (= (g/node-value node :save-value) (g/node-value (project/get-resource-node project "/provider-reloaded.lumanifest") :save-value)))
      (properties/clear-override! (property))
      (is (= [1] (properties/values (property))))
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
;; Unsaved provider settings are captured by the manifest's save value.
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
        (is (= [{:provider "manifest-test"
                 :properties [{:name "extension" :value "alternate"}
                              {:name "quality" :value "7"}
                              {:name "scale" :value "0.5"}]}]
               (:providers (g/node-value node :save-value))))))))

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

;; Resolver diagnostics must follow unsaved manifest edits.
;; No-match policies update property and outline diagnostics
;; when files appear or disappear; regex errors expose one-based columns.
(deftest validation-and-inventory
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (project/get-resource-node project "/packages.lumanifest")
          rule (get-in (g/node-value node :packages) [0 :rules 0 :node-id])]
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
      (is (= #{"/episode.collection" "/extra.json"} (g/node-value node :selected-paths))))))

;; Preview lists custom files by their source paths, including known extensions,
;; and agrees with Bob without producing compiled files.
(deftest custom-resources-preview
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
      (with-manifest-view! project app-view "/packages.lumanifest"
        (fn [{:keys [state]}]
          (doseq [index (range 2)]
            (let [previous (:result @state)]
              (test-util/set-code-editor-source! raw (str "{\"review\":" index "}"))
              (let [plan ^PackageResolver$Plan (:plan (await-preview! state previous))
                    paths (.resources ^PackageResolver$Package (get (.packages plan) "levels"))]
                (is (contains? paths "/extra.json"))
                (is (contains? paths "/opaque/data.script"))
                (is (not (contains? paths "/opaque/data.scriptc"))))))))
      (is (not (.exists (io/file (workspace/build-path workspace "/extra.json")))))
      (is (not (.exists (io/file (workspace/build-path workspace "/opaque/data.script")))))
      (let [contents (coll/into-> (g/node-value project :dirty-save-data) {}
                       (keep (fn [save-data]
                               (when-let [content (resource-node/save-data-content save-data)]
                                 [(resource/proj-path (:resource save-data)) content]))))
            plan (build-bob-plan! project contents "/packages.lumanifest")]
        (is (contains? (.resources ^PackageResolver$Package (get (.packages plan) "levels")) "/opaque/data.script"))
        (is (not (contains? (.resources plan) "/opaque/data.scriptc")))))))

;; Exported artifacts must not become regex inputs to previews or Bob builds.
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
      (let [sources (set (g/node-value node :source-paths))
            refreshed (build-bob-plan! project {} "/packages.lumanifest")]
        (is (not (contains? sources "/episode.zip")))
        (is (= (.base packaged) (.base refreshed)))
        (is (= (.resources ^PackageResolver$Package (get (.packages packaged) "episode"))
               (.resources ^PackageResolver$Package (get (.packages refreshed) "episode")))))
      (with-manifest-view! project app-view "/packages.lumanifest"
        (fn [{:keys [state]}]
          (let [plan ^PackageResolver$Plan (get-in @state [:result :compiled :plan])
                paths (.resources ^PackageResolver$Package (get (.packages plan) "episode"))]
            (is (= #{"/extra.json"} (set paths)))))))))

;; The example separates both episode ranges from shared content, keeps custom
;; text paths, and agrees between the editor graph and Bob with unsaved text.
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
      (let [contents (coll/into-> (g/node-value project :save-data) {}
                       (keep (fn [data]
                               (when-let [content (resource-node/save-data-content data)]
                                 [(resource/proj-path (:resource data)) content]))))
            plan (build-bob-plan! project contents "/liveupdate.lumanifest")]
        (with-manifest-view! project app-view "/liveupdate.lumanifest"
          (fn [{:keys [state]}]
            (let [graph-plan ^PackageResolver$Plan (:plan (get-in @state [:result :compiled]))]
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
              (is (= #{"common" "episodes_common" "halloween"}
                     (coll/into-> (.packages plan) #{}
                       (keep (fn [[name package]]
                               (when (contains? (.resources ^PackageResolver$Package package) "/common/shared.scriptc")
                                 name)))))))))))))

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
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [state]}]
        (let [initial ^PackageResolver$Plan (:plan (get-in @state [:result :compiled]))]
          (is (contains? (.locks initial) "/regular.goc"))
          (is (contains? (.locks initial) "/payload.collectionc"))
          (is (contains? (.locks initial) "/shared.scriptc"))
          (is (not (contains? (.locks initial) "/child.goc")))
          (is (not (contains? (.locks initial) "/dynamic.goc"))))
        (let [previous (:result @state)]
          (test-util/prop! (project/get-resource-node project "/dynamic.factory") :load-dynamically false)
          (is (contains? (.locks ^PackageResolver$Plan (:plan (await-preview! state previous))) "/dynamic.goc")))
        (let [previous (:result @state)]
          (test-util/set-code-editor-source! (project/get-resource-node project "/shared.script") "local extra = require 'extra'\n")
          (let [changed ^PackageResolver$Plan (:plan (await-preview! state previous))]
            (is (contains? (.base changed) "/extra.luac"))
            (is (contains? (.resources ^PackageResolver$Package (get (.packages changed) "content")) "/extra.luac"))))))))

;; Registered views refresh from unsaved edits and inventory without compiling.
;; Reordering retains the controls and cached editor targets; closing and
;; reopening disposes the old view and creates a fresh preview.
(deftest graph-preview
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (project/get-resource-node project "/packages.lumanifest")
          raw (project/get-resource-node project "/extra.json")
          packages (g/node-value node :packages)
          episode (get-in packages [0 :rules 0 :node-id])
          old-state (atom nil)]
      (with-manifest-view! project app-view "/packages.lumanifest"
        (fn [{:keys [state parent]}]
          (reset! old-state state)
          (let [initial (:result @state)
                compiled (:compiled initial)
                table (resource-table parent)
                form (first (.getChildren ^Pane parent))
                children (vec (.getChildren ^Pane form))
                episode-node (project/get-resource-node project "/episode.collection")
                episode-targets (g/node-value episode-node :build-targets)]
            (is (= #{"/episode.collectionc" "/extra.json"}
                   (set (.resources ^PackageResolver$Package (get (.packages ^PackageResolver$Plan (:plan compiled)) "levels")))))
            (g/transact {:undoable false} (app-view/select app-view node [episode]))
            (await-ui! #(= "Package: levels" (.getText ^Label (.lookup ^Pane parent "#liveupdate-package-heading"))))
            (is (identical? compiled (get-in @state [:result :compiled])))
            (is (nil? (:job @state)))
            (let [previous (:result @state)
                  _ (test-util/handler-run :edit.reorder-down
                                           [{:name :workbench :env {:selection [episode]}}] nil)
                  result (await-preview! state previous)]
              (is (= "Data" (.getName (.getRules (.description ^PackageResolver$Package (get (.packages ^PackageResolver$Plan (:plan result)) "levels")) 0))))
              (is (identical? episode-targets (g/node-value episode-node :build-targets))))
            (is (= children (vec (.getChildren ^Pane form))))
            (is (identical? table (resource-table parent))))
          (let [previous (:result @state)]
            (g/transact (g/set-property node :output-directory "exports/packages"))
            (await-preview! state previous)
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
          (let [previous (:result @state)]
            (g/transact (g/set-property episode :path "/nothing[.]collection"))
            (test-util/set-code-editor-source! raw "{\"unsaved\":true}")
            (await-preview! state previous)
            (await-ui! #(= [["/extra.json" []]] (table-tree (resource-table parent))))
            (let [row (.getValue ^TreeItem (first (.getChildren (.getRoot (resource-table parent)))))]
              (is (= "/extra.json" (:source row)))
              (is (= [(get-in packages [0 :rules 1 :node-id])] (:rule-nodes row)))
              (is (= "Data" (:added-by row)))))
          (let [previous (:result @state)]
            (test-util/make-resource! workspace "/nothing.collection" {:name "new"})
            (workspace/resource-sync! workspace)
            (let [result (await-preview! state previous)]
              (is (= #{"/nothing.collectionc" "/extra.json"}
                     (set (.resources ^PackageResolver$Package (get (.packages ^PackageResolver$Plan (:plan result)) "levels")))))))))
      (is (= {} @@old-state))
      (with-manifest-view! project app-view "/packages.lumanifest"
        (fn [{:keys [state]}]
          (is (not (identical? @old-state state)))
          (is (instance? PackageResolver$Plan (get-in @state [:result :compiled :plan]))))))))

(g/defnode PausedBuildTargets
  (inherits resource-node/ResourceNode)
  (input targets g/Any)
  (property gate g/Any)
  (output build-targets g/Any :cached
          (g/fnk [targets gate]
            (when gate
              (deliver (:started gate) (not (ui/on-ui-thread?)))
              (try
                (when (= :timeout (deref (:release gate) 30000 :timeout))
                  (throw (ex-info "Timed out waiting to release build targets" {})))
                (finally
                  (deliver (:finished gate) true))))
            targets)))

(defn- pause-preview! [project manifest-path]
  ;; Insert a graph node into the actual dependency traversal. This exercises
  ;; cancellation through the view without replacing functions or its worker.
  (let [source (project/get-resource-node project "/episode.collection")
        manifest (project/get-resource-node project manifest-path)
        rule (get-in (g/node-value manifest :packages) [0 :rules 0 :node-id])
        gate {:started (promise) :release (promise) :finished (promise)}
        node (first (g/tx-nodes-added
                      (g/transact
                        (g/make-nodes [paused [PausedBuildTargets :resource (g/node-value source :resource) :gate gate]]
                          (g/connect source :build-targets paused :targets)
                          (g/disconnect source :node-id+resource project :node-id+resources)
                          (g/connect paused :node-id+resource project :node-id+resources)
                          (g/update-property rule :id str " updated")))))]
    (assoc gate :node node)))

;; Edits and search input remain usable during traversal. Cancelled results
;; cannot replace newer content, and a validation error preserves the table.
(deftest preview-superseded-and-failed
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/bonus.collection" {:name "bonus"})
    (workspace/resource-sync! workspace)
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [state parent]}]
        (let [node (project/get-resource-node project "/packages.lumanifest")
              rule (get-in (g/node-value node :packages) [0 :rules 0 :node-id])
              table (resource-table parent)
              filter-control ^TextField (.lookup ^Pane parent "#liveupdate-filter")
              previous (:result @state)
              work (pause-preview! project "/packages.lumanifest")]
          (try
            (await-ui! #(realized? (:started work)))
            (is (true? @(:started work)))
            (let [job (:job @state)]
              (g/transact (g/set-property (:node work) :gate nil)
                (g/set-property rule :path "/bonus[.]collection"))
              (.setText filter-control "episode")
              (.setText filter-control "extra.json")
              (is (= "extra.json" (.getText filter-control)))
              (is (identical? previous (:result @state)))
              (await-ui! #(realized? (:finished work)))
              (is (future-cancelled? job)))
            (let [result (await-preview! state previous)]
              (is (= #{"/bonus.collectionc" "/extra.json"}
                     (set (.resources ^PackageResolver$Package (get (.packages ^PackageResolver$Plan (:plan result)) "levels")))))
              (await-ui! #(= [["/extra.json" []]] (table-tree table)))
              (.setText filter-control "bonus")
              (await-ui! #(= [["/bonus.collectionc" []]] (table-tree table)))
              (is (identical? result (get-in @state [:result :compiled]))))
            (let [previous (:result @state)]
              (g/set-property! rule :path "[")
              (await-ui! #(some? (:error @state)))
              (is (identical? previous (:result @state)))
              (is (identical? table (resource-table parent)))
              (is (identical? filter-control (.lookup ^Pane parent "#liveupdate-filter")))
              (g/set-property! rule :path "/bonus[.]collection")
              (await-ui! #(nil? (:error @state)))
              (await-preview! state previous)
              (is (nil? (:error @state))))
            (finally
              (deliver (:release work) true)
              (await-ui! #(realized? (:finished work))))))))))

;; Closing a tab interrupts active graph evaluation. Reopening creates a fresh
;; view after the old worker has signalled completion.
(deftest close-active-preview
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [active (atom nil)]
      (try
        (with-manifest-view! project app-view "/packages.lumanifest"
          (fn [_]
            (let [work (pause-preview! project "/packages.lumanifest")]
              (reset! active work)
              (await-ui! #(realized? (:started work))))))
        (is (= true (deref (:finished @active) 30000 :timeout)))
        (g/set-property! (:node @active) :gate nil)
        (with-manifest-view! project app-view "/packages.lumanifest"
          (fn [{:keys [state]}]
            (is (instance? PackageResolver$Plan (get-in @state [:result :compiled :plan])))))
        (finally
          (when-let [work @active]
            (deliver (:release work) true)
            (is (= true (deref (:finished work) 30000 :timeout)))))))))

;; Status-bar cancellation stops an in-flight traversal and preserves its last
;; result. Finishing another view must not dismiss the active task's progress.
(deftest delayed-preview-progress
  (test-util/with-scratch-project "test/resources/live_update_project"
    (test-util/make-resource! workspace "/other.lumanifest"
                              {:packages [{:name "other" :rules [{:name "Data" :path "/extra[.]json"}]}]})
    (workspace/resource-sync! workspace)
    (with-manifest-view! project app-view "/packages.lumanifest"
      (fn [{:keys [state preferences]}]
        (let [previous (:result @state)
              work (pause-preview! project "/packages.lumanifest")
              _ (await-ui! #(realized? (:started work)))
              second-view (open-manifest-view! project app-view "/other.lumanifest" preferences)
              {:keys [^Button progress-cancel-button ^Label status-label ^Pane progress-hbox]}
              (ui/collect-controls (.getRoot (.getScene (ui/main-stage)))
                                   ["progress-cancel-button" "status-label" "progress-hbox"])]
          (try
            (await-preview! (:state second-view) nil)
            (await-ui! #(.isVisible progress-cancel-button))
            (is (= "Preparing Live Update preview" (.getText status-label)))
            (let [job (:job @state)]
              (.fire progress-cancel-button)
              (await-ui! #(realized? (:finished work)))
              (is (future-cancelled? job)))
            (await-ui! #(nil? (:job @state)))
            (is (identical? previous (:result @state)))
            (await-ui! #(not (.isVisible progress-hbox)))
            (finally
              (deliver (:release work) true)
              (await-ui! #(realized? (:finished work)))
              (close-manifest-view! second-view))))))))

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
      (fn [{:keys [state parent]}]
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
          (let [previous (:result @state)]
            (g/transact (g/set-property rule :path "/episode[.]collection"))
            (await-preview! state previous))
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
