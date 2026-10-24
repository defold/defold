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
            [clojure.test :refer [deftest is]]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.build :as build]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.live-update-manifest :as manifest]
            [editor.live-update-manifest-view :as manifest-view]
            [editor.live-update-preview :as preview]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
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
           [com.dynamo.bob.archive.liveupdate PackagePreview$Result PackageResolver PackageResolver$Loading PackageResolver$Package PackageResolver$Plan PackageResolver$Resource PackageResolver$Severity]
           [com.dynamo.bob.fs DefaultFileSystem]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ManifestDesc]
           [java.net URI]
           [java.nio.charset StandardCharsets]
           [java.nio.file Files Path]
           [java.nio.file.attribute FileTime]
           [java.util.zip ZipEntry ZipOutputStream]
           [javafx.animation AnimationTimer]
           [javafx.event EventHandler]
           [javafx.scene.control Label MenuItem RadioButton Tab TabPane TreeItem TreeTableCell TreeTableColumn TreeTableView]
           [javafx.scene.input Clipboard]
           [javafx.scene.layout AnchorPane Pane]
           [javax.tools ToolProvider]))

(set! *warn-on-reflection* true)

(defn- build-preview! [resources contents manifest-path options cancelled?]
  (let [controller (preview/controller)]
    (try
      (preview/build! controller resources contents manifest-path options cancelled?)
      (finally
        @(preview/close! controller)))))

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
      (is (= "renamed" (#'manifest-view/selection-package (g/node-value node :packages) #{(:node-id episode)})))
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
            ^PackagePreview$Result result (build-preview! (manifest/source-resources (g/node-value workspace :resource-map)
                                                                                     (g/node-value workspace :live-update-output-paths))
                                                         contents "/packages.lumanifest" {} (constantly false))]
        (is (= {"levels" [(str (io/file root "exported packages/levels.zip"))]
                "copy" [(str (io/file root "exported packages/copy"))]}
               (coll/into-> (.exportPaths result) {} (map (fn [[name paths]] [name (vec paths)])))))
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

;; A real pipeline JAR supplies fields without editor code. Unsaved edits reach
;; Bob's preview output declaration, while provider writers are never invoked.
(deftest custom-provider-properties
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [root (workspace/project-directory workspace)
          classes (io/file root "build/provider-test-classes")
          jar (workspace/file-resource workspace "/provider/plugins/share/provider.jar")
          class-path "com/defold/extension/pipeline/ManifestTestProvider.class"]
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
      (test-util/make-resource! workspace "/provider.lumanifest"
                                {:packages [{:name "custom" :provider "manifest-test"
                                             :rules [{:name "Data" :path "/extra[.]json"}]}]})
      (workspace/resource-sync! workspace)
      (let [node (project/get-resource-node project "/provider.lumanifest")
            property (fn [id]
                       (get-in (properties/coalesce [(g/node-value node :_properties)])
                               [:properties (keyword "live-update-provider.manifest-test" id)]))]
        (is (= ["custom"] (properties/values (property "extension"))))
        (is (= [2] (properties/values (property "quality"))))
        (is (= [1.0] (properties/values (property "scale"))))
        (doseq [[id value] [["extension" "alternate"] ["quality" 7] ["scale" 0.5]]]
          (properties/set-values! (property id) [value]))
        (let [^PackagePreview$Result result (build-preview!
                                              (manifest/source-resources (g/node-value workspace :resource-map)
                                                                         (g/node-value workspace :live-update-output-paths))
                                              {"/provider.lumanifest" (resource-node/save-data-content (g/node-value node :save-data))}
                                              "/provider.lumanifest" {} (constantly false))]
          (is (= [(str (io/file root "build/packages/custom.alternate-7-0.5"))]
                 (vec (get (.exportPaths result) "custom"))))
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
      (is (= [:target :id :operation :path]
             (:display-order (properties/coalesce [(g/node-value path-rule :_properties)]))))
      (is (= [:target :operation :package]
             (:display-order (properties/coalesce [(g/node-value package-rule :_properties)]))))
      (is (= "+ Package: levels" (@test-util/localization (:label (g/node-value package-rule :node-outline)))))
      (ui/run-now
        (doseq [[prop-kw node-ids labels initial-values initial-selection choice]
                [[:operation [path-rule] ["Add" "Remove"] [:add] [true false] :remove]
                 [:target [path-rule] ["Path" "Package"] [:path] [true false] :package]
                 [:target [path-rule package-rule] ["Path" "Package"] [:path :package] [false false] :package]]]
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
                  :target (do (is (= "" (:package saved-rule)))
                              (is (not (contains? saved-rule :path)))
                              (is (= [:target :operation :package]
                                     (:display-order (properties/coalesce [(g/node-value path-rule :_properties)])))))))
              (vswap! component fx/advance-component (description))
              (is (= [false true] (selected)))
              (g/undo! :undo/global)
              (vswap! component fx/advance-component (description))
              (is (= initial-values (mapv #(g/node-value % prop-kw) node-ids)))
              (is (= initial-selection (selected)))
              (is (= original (g/node-value node :save-value)))
              (is (= [:target :id :operation :path]
                     (:display-order (properties/coalesce [(g/node-value path-rule :_properties)]))))
              (is (= "+ Episode" (:label (g/node-value path-rule :node-outline))))
              (g/redo! :undo/global)
              (vswap! component fx/advance-component (description))
              (is (= [false true] (selected)))
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
;; raw-file edits. Regex errors expose one-based columns in localized
;; properties.
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
          (is (= (str "Invalid regular expression at column " column ": " reason)
                 (@test-util/localization (:message error)))))
        (is (g/error-value? (g/node-value node :selected-paths)))
        (is (= (str "Invalid regex in rule 'Episode' of package 'levels' at column " column ": " reason ". Path: " path)
               (.message ^com.dynamo.bob.archive.liveupdate.PackageResolver$Diagnostic (first (g/node-value node :diagnostics))))))
      (g/transact (g/set-property rule :path "/absent/.*"))
      (is (coll/any? #(= PackageResolver$Severity/WARNING (.severity ^com.dynamo.bob.archive.liveupdate.PackageResolver$Diagnostic %))
                    (g/node-value node :diagnostics)))
      (is (nil? (test-util/prop-error rule :path)))
      (is (= "Rule 'Episode' in package 'levels' matches no source files for Path '/absent/.*'. Patterns match the entire source path, including its leading '/'."
             (.message ^com.dynamo.bob.archive.liveupdate.PackageResolver$Diagnostic (first (g/node-value node :diagnostics)))))
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
        (when (< index 6)
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
            ^PackagePreview$Result result (build-preview!
                                            (manifest/source-resources (g/node-value workspace :resource-map)
                                                                       (g/node-value workspace :live-update-output-paths))
                                            contents "/packages.lumanifest" {} (constantly false))]
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
      (is (= #{"/episode.zip"} (g/node-value workspace :live-update-output-paths)))
      (is (= #{"/extra.json"} (g/node-value node :selected-paths)))
      (let [sources (manifest/source-resources (g/node-value workspace :resource-map)
                                               (g/node-value workspace :live-update-output-paths))
            ^PackagePreview$Result refreshed (build-preview! sources {} "/packages.lumanifest" {} (constantly false))]
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
    (let [node (project/get-resource-node project "/liveupdate.lumanifest")
          raw (project/get-resource-node project "/episodes/00/description.txt")
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
            ^PackagePreview$Result result (build-preview! (manifest/source-resources (g/node-value workspace :resource-map)
                                                                                     (g/node-value workspace :live-update-output-paths))
                                                         contents "/liveupdate.lumanifest" {} (constantly false))
            plan (.plan result)]
        (is (= (set (coll/keys expected)) (set (.keySet (.packages plan)))))
        (doseq [[name paths] expected]
          (is (= paths (set (.resources ^PackageResolver$Package (get (.packages plan) name)))) name))
        (doseq [name ["episodes_00_09" "episodes_10_19"]]
          (is (= {"/episodes/common/actor.goc" ["episodes_common"]
                  "/common/shared.scriptc" ["common" "episodes_common" "halloween"]}
                 (get (.requirements plan) name))))
        (is (= (alength (.getBytes unsaved StandardCharsets/UTF_8))
               (get (.compiledSizes result) "/episodes/00/description.txt")))
        (let [rows (#'manifest-view/resource-rows result nil (g/node-value node :packages))]
          (is (= ["common" "episodes_common" "halloween"]
                 (:packages (coll/first-where #(= "/common/shared.scriptc" (:path %)) rows))))))
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

;; Opening the view and changing graph inputs update the preview automatically.
;; Unsaved edits, new matching files, and bundle options invalidate it;
;; repainting and selection alone do not start another build. Reordering keeps
;; the existing form intact while Bob prepares the updated preview.
(deftest compiled-preview
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [node (test-util/open-tab! project app-view "/packages.lumanifest")
          raw (project/get-resource-node project "/extra.json")
          packages (g/node-value node :packages)
          episode (get-in packages [0 :rules 0 :node-id])
          prefs-file (fs/create-temp-file! "liveupdate-preview" ".editor_settings")
          preferences (prefs/make :scopes {:global prefs-file :project prefs-file}
                                  :schemas [:default])
          timers (atom #{})
          frames (atom 0)
          unchanged (FileTime/fromMillis 946684800000)]
      ;; Advance the real view's timer explicitly instead of waiting for frames.
      #_{:clj-kondo/ignore [:discouraged-var]}
      (with-redefs [ui/timer-start! #(swap! timers conj %)
                    ui/timer-stop! #(swap! timers disj %)
                    app-view/begin-task-progress! (fn [_] [progress/null-render-progress! (constantly false)])]
        (letfn [(open! []
                  (ui/run-now
                    (let [tab (Tab.)
                          _ (TabPane. (into-array Tab [tab]))
                          parent (AnchorPane.)
                          view (#'manifest-view/make-view parent node
                                  {:app-view app-view :project project :workspace workspace
                                   :prefs preferences :localization test-util/localization :tab tab})]
                      (g/transact {:undoable false} (g/connect node :_node-id view :resource-node))
                      {:tab tab :parent parent :controller (g/user-data view :preview-controller)})))
                (frame! []
                  (ui/run-now
                    (with-redefs [ui/do-run-later (fn [f] (f))]
                      (doseq [_ (range 2)
                              timer @timers]
                        (.handle ^AnimationTimer (:timer timer) (+ (long (:start timer)) (* (long (swap! frames inc)) 1000000000)))))))
                (completed! [controller]
                  (when-let [completion (get-in @controller [:job :completion])]
                    (is (= true (deref completion 30000 :timeout))))
                  (is (nil? (:error @controller)) (str (:error @controller)))
                  (frame!)
                  (get-in @controller [:result :value :compiled]))
                (close! [{:keys [tab controller]}]
                  (ui/run-now (.handle ^EventHandler (ui/on-closed tab) nil))
                  (is (= true (deref (:close-completion @controller) 30000 :timeout)))
                  (is (not (.exists (.toFile ^Path @(:directory @controller))))))]
          (let [{:keys [controller parent] :as view} (open!)]
            (try
              (frame!)
              (ui/run-now
                (is (identical? (.lookup ^AnchorPane parent "#liveupdate-filter")
                                (first (.getChildren ^Pane (first (.getChildren ^AnchorPane parent)))))))
              (let [result (completed! controller)]
                (is (= #{"/episode.collectionc" "/extra.json"}
                       (set (.resources ^PackageResolver$Package (get (.packages (.plan ^PackagePreview$Result result)) "levels")))))
                (g/transact {:undoable false} (app-view/select app-view node [episode]))
                (frame!)
                (is (nil? (:job @controller)))
                (is (identical? result (get-in @controller [:result :value :compiled]))))
              (g/transact (g/set-property node :output-directory "exports/packages"))
              (frame!)
              (completed! controller)
              (ui/run-now
                (let [^Label path-label (.lookup ^AnchorPane parent "#liveupdate-export-path")
                      [^MenuItem copy ^MenuItem open] (.getItems (.getContextMenu path-label))
                      clipboard (Clipboard/getSystemClipboard)
                      previous (coll/pair-map-by identity #(.getContent clipboard %) (.getContentTypes clipboard))
                      opened (atom nil)]
                  (is (= (str (io/file (workspace/project-directory workspace) "exports/packages/levels.zip")) (.getText path-label)))
                  (try
                    (.fire copy)
                    (is (= (.getText path-label) (.getString clipboard)))
                    (finally
                      (.setContent clipboard previous)))
                  (with-redefs [ui/open-file #(reset! opened %)]
                    (.fire open))
                  (is (= (workspace/project-directory workspace) @opened))))
              (Files/setLastModifiedTime (.resolve ^Path @(:directory @controller) "build/episode.collectionc") unchanged)

              (let [started (promise)
                    release (promise)
                    build! preview/build!
                    children (ui/run-now
                               (vec (.getChildren ^Pane (first (.getChildren ^AnchorPane parent)))))]
                (with-redefs [preview/build! (fn [controller resources contents path options cancelled?]
                                              (deliver started true)
                                              @release
                                              (build! controller resources contents path options cancelled?))]
                  (try
                    (test-util/handler-run :edit.reorder-down
                                          [{:name :workbench :env {:selection [episode]}}] nil)
                    (frame!)
                    (is (= true (deref started 5000 :timeout)))
                    (is (:job @controller))
                    (ui/run-now
                      (is (= children (vec (.getChildren ^Pane (first (.getChildren ^AnchorPane parent)))))))
                    (deliver release true)
                    (let [^PackagePreview$Result result (completed! controller)]
                      (is (= "Data" (.getName (.getRules (.description ^PackageResolver$Package (get (.packages (.plan result)) "levels")) 0))))
                      (is (= unchanged (Files/getLastModifiedTime (.resolve ^Path @(:directory @controller) "build/episode.collectionc")
                                                                 (make-array java.nio.file.LinkOption 0)))))
                    (ui/run-now
                      (is (= children (vec (.getChildren ^Pane (first (.getChildren ^AnchorPane parent)))))))
                    (finally
                      (deliver release true)
                      (when-let [completion (get-in @controller [:job :completion])]
                        (is (= true (deref completion 30000 :timeout))))))))

              (g/transact (g/set-property episode :path "/nothing[.]collection"))
              (test-util/set-code-editor-source! raw "{\"unsaved\":true}")
              (frame!)
              (let [result (completed! controller)
                    rows (#'manifest-view/resource-rows result "levels" (g/node-value node :packages))]
                (is (= 1 (count rows)))
                (is (= "/extra.json" (:path (first rows))))
                (is (= 16 (:size (first rows))))
                (is (= "/extra.json" (:source (first rows))))
                (is (= [(get-in packages [0 :rules 1 :node-id])] (:rule-nodes (first rows))))
                (is (= "Data" (:added-by (first rows))))
                (ui/run-now
                  (let [^TreeTableView table (.lookup ^AnchorPane parent "#liveupdate-resources")]
                    (is (= ["/extra.json"] (mapv #(:path (.getValue ^TreeItem %)) (.getChildren (.getRoot table)))))
                    (is (nil? (.lookup ^AnchorPane parent "#liveupdate-refresh"))))))

              (test-util/make-resource! workspace "/nothing.collection" {:name "new"})
              (workspace/resource-sync! workspace)
              (frame!)
              (let [^PackagePreview$Result result (completed! controller)]
                (is (= #{"/nothing.collectionc" "/extra.json"}
                       (set (.resources ^PackageResolver$Package (get (.packages (.plan result)) "levels"))))))

              (prefs/set! preferences [:bundle :last-bob-options] {"variant" "release"})
              (frame!)
              (completed! controller)
              (is (= {"variant" "release"} (second (get-in @controller [:result :fingerprint]))))
              (finally
                (close! view)))
            (is (:closed @controller))
            (is (coll/empty? @timers)))
          (let [{:keys [controller] :as reopened} (open!)]
            (try
              (frame!)
              (is (instance? PackagePreview$Result (completed! controller)))
              (finally
                (close! reopened)))))))))

;; Rapid edits must restart the debounce window without launching intermediate
;; builds. Advance the view timer explicitly to avoid wall-clock assumptions.
(deftest preview-debounces-edits
  (test-util/with-scratch-project "test/resources/live_update_project"
    (let [controller (preview/controller)
          update! (#'manifest-view/make-update-preview! controller)
          calls (atom [])
          model {:resource (workspace/file-resource workspace "/packages.lumanifest")
                 :resource-map {} :generated-paths #{} :save-data [] :packages []}]
      (try
        #_{:clj-kondo/ignore [:discouraged-var]}
        (with-redefs [preview/build! (fn [_ _ _ _ options _] (swap! calls conj options) :compiled)]
          (doseq [[fingerprint time] [[:first 0.0] [:second 0.1] [:second 0.29] [:latest 0.3]]]
            (update! (assoc model :fingerprint fingerprint) {"variant" "debug"} time)
            (is (nil? (:job @controller)))
            (is (coll/empty? @calls)))
          (update! (assoc model :fingerprint :latest) {"variant" "debug"} 0.51)
          (when-let [completion (get-in @controller [:job :completion])]
            (is (= true (deref completion 5000 :timeout))))
          (is (= [{"variant" "debug"}] @calls))
          (is (= [:latest {"variant" "debug"}] (get-in @controller [:result :fingerprint])))
          (update! (assoc model :fingerprint :latest) {"variant" "debug"} 1.0)
          (is (nil? (:job @controller))))
        (finally
          @(preview/close! controller))))))

;; Delayed status-bar progress keeps fast previews silent. Cancellation and
;; closing a view affect only its job, and completion of an older preview must
;; not clear another view's progress. Frames and work use explicit signals.
(deftest delayed-preview-progress
  (let [controller (preview/controller)
        other-controller (preview/controller)
        update-progress! (#'manifest-view/make-update-progress!)
        update-other-progress! (#'manifest-view/make-update-progress!)
        reports (atom [])
        cancelled (atom false)
        frame! (fn [time]
                 (ui/run-now
                   (update-progress! (:job @controller) time)
                   (update-other-progress! (:job @other-controller) time)))]
    ;; Observe status-bar reports without requiring a main application window.
    #_{:clj-kondo/ignore [:discouraged-var]}
    (with-redefs [app-view/begin-task-progress!
                  (fn [key]
                    (is (= :live-update-preview key))
                    (reset! cancelled false)
                    [#(swap! reports conj %) #(deref cancelled)])]
      (is (= true (deref (preview/update! controller :fast (constantly :fast)) 5000 :timeout)))
      (frame! 0.0)
      (is (coll/empty? @reports))
      (let [started (promise)
            other-started (promise)
            release (promise)
            other-release (promise)
            completion (preview/update! controller :slow
                                        (fn [cancelled?]
                                          (deliver started true)
                                          @release
                                          (is (cancelled?))
                                          :obsolete))
            other-completion (preview/update! other-controller :other
                                              (fn [cancelled?]
                                                (deliver other-started true)
                                                @other-release
                                                (is (not (cancelled?)))
                                                :other))]
        (try
          (is (= true (deref started 5000 :timeout)))
          (is (= true (deref other-started 5000 :timeout)))
          (frame! 1.0)
          (frame! 1.1)
          (is (coll/empty? @reports))
          (frame! 1.21)
          (frame! 1.25)
          (is (= 1 (count @reports)))
          (is (nil? (progress/fraction (first @reports))))
          (is (progress/cancellable? (first @reports)))
          (is (= "Preparing Live Update preview" (@test-util/localization (progress/message (first @reports)))))
          (reset! cancelled true)
          (frame! 1.3)
          (is @(get-in @controller [:job :cancelled]))
          (is (false? @(get-in @other-controller [:job :cancelled])))
          (is (= :fast (get-in @controller [:result :value])))
          (preview/close! controller)
          (frame! 1.4)
          (is (= [false true false] (mapv progress/done? @reports)))
          (deliver release true)
          (is (= true (deref completion 5000 :timeout)))
          (frame! 1.5)
          (is (= 3 (count @reports)))
          (deliver other-release true)
          (is (= true (deref other-completion 5000 :timeout)))
          (frame! 1.6)
          (is (= [false true false true] (mapv progress/done? @reports)))
          (is (= :other (get-in @other-controller [:result :value])))
          (let [release-failure (promise)
                failure (ex-info "Preview failed" {})
                failed-completion (preview/update! other-controller :failure
                                                   (fn [_]
                                                     @release-failure
                                                     (throw failure)))]
            (try
              (frame! 2.0)
              (frame! 2.21)
              (is (not (progress/done? (peek @reports))))
              (deliver release-failure true)
              (is (= true (deref failed-completion 5000 :timeout)))
              (frame! 2.3)
              (is (progress/done? (peek @reports)))
              (is (identical? failure (:error @other-controller)))
              (finally
                (deliver release-failure true)
                (is (= true (deref failed-completion 5000 :timeout))))))
          (finally
            (deliver release true)
            (deliver other-release true)
            (preview/close! controller)
            (preview/close! other-controller)
            (is (= true (deref completion 5000 :timeout)))
            (is (= true (deref other-completion 5000 :timeout)))
            (frame! 3.0)))))))

;; Package views retain dependency hierarchies. Global searches show each match
;; once with every supplying package, including Base, and the contributing
;; rules. Background refreshes and error messages preserve the controls and
;; selected resource. Recycled size cells tolerate empty rows.
(deftest compiled-preview-tree
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
    (let [node (project/get-resource-node project "/packages.lumanifest")
          packages (g/node-value node :packages)
          contents (coll/into-> (g/node-value project :save-data) {}
                     (keep (fn [data]
                             (when-let [content (resource-node/save-data-content data)]
                               [(resource/proj-path (:resource data)) content]))))
          result (build-preview! (manifest/source-resources (g/node-value workspace :resource-map)
                                                            (g/node-value workspace :live-update-output-paths))
                                 contents "/packages.lumanifest" {} (constantly false))
          rows (#'manifest-view/resource-rows result "levels" packages)
          expected [["/bonus.collectionc" [["/shared.goc" [["/shared.scriptc" []]]]]]
                    ["/episode.collectionc" [["/shared.goc" [["/shared.scriptc" []]]]]]
                    ["/extra.json" []]
                    ["/shared.scriptc" []]]]
      (is (= 5 (count rows)))
      (ui/run-now
        (let [parent (AnchorPane.)
              props {:fx/type @#'manifest-view/view-desc
                     :parent parent
                     :model {:resource-node node :packages packages :selection [(:node-id (first packages))]}
                     :preview-state {:result {:value {:compiled result :packages packages}}}
                     :ui-state {}
                     :localization-state @test-util/localization}
              events (atom [])
              opts {:fx.opt/map-event-handler #(swap! events conj %)}
              component (volatile! (fx/create-component props opts))
              ^TreeTableView table (.lookup parent "#liveupdate-resources")]
          (letfn [(advance! [description]
                    (vswap! component fx/advance-component description opts))
                  (children [^TreeItem item]
                    (mapv (fn [^TreeItem child] [(:path (.getValue child)) (children child)])
                          (.getChildren item)))]
            (try
              (is (false? (.isShowRoot table)))
              (is (= expected (children (.getRoot table))))
              (let [^TreeTableColumn size-column (nth (.getColumns table) 1)
                    ^TreeTableCell cell (.call (.getCellFactory size-column) size-column)]
                (.updateTreeTableView cell table)
                (.updateTreeTableColumn cell size-column)
                (.updateIndex cell 0)
                (is (string? (.getText cell)))
                (.updateIndex cell -1)
                (is (= "" (.getText cell))))
              (is (= "Package: levels" (.getText ^Label (.lookup parent "#liveupdate-package-heading"))))
              (let [^Label export-path (.lookup parent "#liveupdate-export-path")]
                (is (= (str (io/file (workspace/project-directory workspace) "build/packages/levels.zip")) (.getText export-path)))
                (is (= ["Copy Path" "Open Folder"] (mapv #(.getText ^MenuItem %) (.getItems (.getContextMenu export-path)))))
                (.fire ^MenuItem (first (.getItems (.getContextMenu export-path))))
                (is (= {:event/type :copy-path :path (.getText export-path)} (dissoc (last @events) :fx/event)))
                (.fire ^MenuItem (second (.getItems (.getContextMenu export-path))))
                (is (= {:event/type :open-folder :path (.getText export-path)} (dissoc (last @events) :fx/event)))
                (is (.contains (.getStyle export-path) "italic")))
              (let [^TreeItem collection (first (.getChildren (.getRoot table)))
                    ^TreeItem go (first (.getChildren collection))
                    ^TreeItem script (first (.getChildren go))]
                (.select (.getSelectionModel table) script)
                (is (= :select-resource (:event/type (last @events))))
                (is (= "levels" (:package (last @events))))
                (is (identical? script (:fx/event (last @events))))
                (is (= "/shared.script" (:source (.getValue script))))
                (is (= #{(get-in packages [0 :rules 0 :node-id])
                         (get-in packages [0 :rules 2 :node-id])}
                       (set (:rule-nodes (.getValue script)))))
                (let [filter-control (.lookup parent "#liveupdate-filter")
                      ^Pane form (first (.getChildren parent))
                      form-children (vec (.getChildren form))]
                  (doseq [description [(assoc-in props [:preview-state :job] {:completion (promise)})
                                       props]]
                    (advance! description)
                    (is (= form-children (vec (.getChildren form))))
                    (is (identical? script (.getSelectedItem (.getSelectionModel table)))))
                  (advance! (assoc-in props [:preview-state :error] (ex-info "Preview failed" {})))
                  (is (identical? filter-control (.lookup parent "#liveupdate-filter")))
                  (is (identical? table (.lookup parent "#liveupdate-resources")))
                  (is (identical? script (.getSelectedItem (.getSelectionModel table))))
                  (is (= "Preview failed" (.getText ^Label (first (.getChildren form)))))
                  (advance! props)
                  (is (= form-children (vec (.getChildren form))))))
              (advance! (assoc-in props [:ui-state :filter] "SHARED.SCRIPT"))
              (is (= [["/shared.scriptc" []]] (children (.getRoot table))))
              (is (= "Search results" (.getText ^Label (.lookup parent "#liveupdate-package-heading"))))
              (is (nil? (.lookup parent "#liveupdate-export-path")))
              (is (.startsWith (.getText ^Label (.lookup parent "#liveupdate-resource-summary")) "1 resources · "))
              (let [^TreeItem match (first (.getChildren (.getRoot table)))
                    ^TreeTableColumn packages-column (nth (.getColumns table) 2)]
                (is (= ["base" "copy" "levels"] (:packages (.getValue match))))
                (is (= "Base, copy, levels" (.getCellData packages-column match)))
                (is (= "/shared.script" (:source (.getValue match))))
                (is (= #{(get-in packages [0 :rules 0 :node-id])
                         (get-in packages [0 :rules 2 :node-id])
                         (get-in packages [1 :rules 0 :node-id])}
                       (set (:rule-nodes (.getValue match)))))
                (.select (.getSelectionModel table) match)
                (is (identical? match (:fx/event (last @events)))))
              (advance! props)
              (is (= expected (children (.getRoot table))))
              (advance! (assoc-in props [:ui-state :filter] "main.collection"))
              (is (= [["/main.collectionc" []]] (children (.getRoot table))))
              (is (= ["base"] (:packages (.getValue ^TreeItem (first (.getChildren (.getRoot table)))))))
              (advance! (assoc-in props [:model :selection] [(:node-id (second packages))]))
              (is (= expected (children (.getRoot table))))
              (is (= "Package: copy" (.getText ^Label (.lookup parent "#liveupdate-package-heading"))))
              (is (= (str (io/file (workspace/project-directory workspace) "build/packages/copy"))
                     (.getText ^Label (.lookup parent "#liveupdate-export-path"))))
              (is (nil? (.getSelectedItem (.getSelectionModel table))))
              (advance! (assoc-in props [:model :selection] []))
              (is (= "Package: Base" (.getText ^Label (.lookup parent "#liveupdate-package-heading"))))
              (is (nil? (.lookup parent "#liveupdate-export-path")))
              (let [base-tree (into {} (children (.getRoot table)))]
                (is (= [["/shared.goc" [["/shared.scriptc" []]]]] (get base-tree "/main.collectionc")))
                (is (not (contains? base-tree "/extra.json"))))
              (advance! (-> props
                            (assoc-in [:model :selection] [])
                            (assoc-in [:ui-state :filter] "shared.script")))
              (is (= [["/shared.scriptc" []]] (children (.getRoot table))))
              (advance! (-> props
                            (assoc-in [:model :selection] [])
                            (assoc-in [:ui-state :filter] "episode.collection")))
              (is (= [["/episode.collectionc" []]] (children (.getRoot table))))
              (is (= ["copy" "levels"] (:packages (.getValue ^TreeItem (first (.getChildren (.getRoot table)))))))
              (advance! (assoc-in props [:ui-state :filter] "no-match"))
              (is (coll/empty? (.getChildren (.getRoot table))))
              (is (= "0 resources · 0 B compiled" (.getText ^Label (.lookup parent "#liveupdate-resource-summary"))))
              (finally
                (fx/delete-component @component)))))))))

;; Primary task inputs identify atlas outputs in package, base, and global
;; views even when the compiler also reads image dependencies.
(deftest compiled-preview-source-attribution
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
    (let [node (project/get-resource-node project "/packages.lumanifest")
          result (build-preview! (manifest/source-resources (g/node-value workspace :resource-map)
                                                            (g/node-value workspace :live-update-output-paths))
                                 {} "/packages.lumanifest" {} (constantly false))]
      (doseq [package ["episode" "base" nil]]
        (let [rows (#'manifest-view/resource-rows result package (g/node-value node :packages))
              sources (coll/into-> rows {}
                        (keep (fn [{:keys [path source]}]
                                (when (contains? #{"/atlas.a.texturesetc" "/atlas.texturec"} path)
                                  [path source]))))]
          (is (= {"/atlas.a.texturesetc" "/atlas.atlas"
                  "/atlas.texturec" "/atlas.atlas"}
                 sources)))))))

;; Deferred base content and subtrees disconnected by package placement must
;; remain nested and visible; cycles must not recurse indefinitely.
(deftest base-preview-tree
  (let [eager PackageResolver$Loading/EAGER
        deferred PackageResolver$Loading/DEFERRED
        resources {"/main.collectionc" {"/proxy.collectionproxyc" eager}
                   "/proxy.collectionproxyc" {"/episode.collectionc" deferred "/resident.collectionc" deferred}
                   "/episode.collectionc" {"/z.goc" eager "/cycle-a.luac" eager}
                   "/resident.collectionc" {"/resident.goc" eager}
                   "/resident.goc" {}
                   "/z.goc" {"/a.scriptc" eager}
                   "/a.scriptc" {}
                   "/cycle-a.luac" {"/cycle-b.luac" eager}
                   "/cycle-b.luac" {"/cycle-a.luac" eager}
                   "/custom.json" {}}
        plan (PackageResolver/resolve
               (protobuf/map->pb LiveUpdateManifest$ManifestDesc
                                 {:packages [{:name "levels"
                                              :rules [{:name "Episode" :path "/episode[.]collection"}
                                                      {:name "Keep in base" :operation :remove :path "/(z[.]go|cycle-a[.]lua)"}]}]})
               {"/episode.collection" ["/episode.collectionc"]
                "/z.go" ["/z.goc"]
                "/cycle-a.lua" ["/cycle-a.luac"]}
               (coll/into-> resources {} (map (fn [[path dependencies]] [path (PackageResolver$Resource. dependencies)])))
               ["/main.collectionc"] ["/custom.json"] true)
        result (PackagePreview$Result. plan (coll/pair-map-by identity (constantly 1) (coll/keys resources)) {})
        rows (#'manifest-view/resource-rows result "base" [])
        tree (#'manifest-view/resource-tree rows)]
    (is (false? (.hasErrors plan)))
    (is (= 9 (count rows)))
    (letfn [(paths [rows]
              (mapv (fn [row] [(:path row) (paths (:children row))]) rows))]
      (is (= [["/custom.json" []]
              ["/cycle-a.luac" [["/cycle-b.luac" []]]]
              ["/main.collectionc" [["/proxy.collectionproxyc" [["/resident.collectionc" [["/resident.goc" []]]]]]]]
              ["/z.goc" [["/a.scriptc" []]]]]
             (paths tree))))))

;; Completion signals verify cancellation and coalescing superseded edits,
;; and close/reopen without overlapping work or restarting unchanged requests.
(deftest preview-lifecycle
  (let [controller (preview/controller)
        started (promise)
        release (promise)
        finished (preview/update! controller :old
                                 (fn [cancelled?]
                                   (deliver started true)
                                   @release
                                   (is (cancelled?))
                                   :obsolete))]
    (is (= true (deref started 5000 :timeout)))
    (is (nil? (preview/update! controller :old (fn [_] (is false "Unchanged inputs must not rebuild")))))
    (let [queued-finished (preview/update! controller :queued (fn [_] (is false "Superseded queued work must not run")))
          new-finished (preview/update! controller :new (constantly :current))]
      (is (not (realized? new-finished)))
      (deliver release true)
      (is (= true (deref finished 5000 :timeout)))
      (is (= true (deref queued-finished 5000 :timeout)))
      (is (= true (deref new-finished 5000 :timeout)))
      (is (= :current (get-in @controller [:result :value])))
      (is (nil? (preview/update! controller :new (fn [_] (is false "Completed inputs must not rebuild")))))))
  (let [controller (preview/controller)
        started (promise)
        release (promise)
        finished (preview/update! controller :active
                                 (fn [_] (deliver started @(:directory @controller)) @release :obsolete))]
    (try
      (let [^Path directory (deref started 5000 :timeout)
            closed (preview/close! controller)]
        (is (not (realized? closed)))
        (is (.exists (.toFile directory)))
        (deliver release true)
        (is (= true (deref finished 5000 :timeout)))
        (is (= true (deref closed 5000 :timeout)))
        (is (not (.exists (.toFile directory))))
        (is (nil? (:result @controller)))
        (is (nil? (preview/update! controller :active (fn [_] (is false "Closed previews must not restart"))))))
      (finally
        (deliver release true)
        @(preview/close! controller))))
  (let [reopened (preview/controller)]
    (is (= true (deref (preview/update! reopened :reopened (constantly :fresh)) 5000 :timeout)))
    (is (= :fresh (get-in @reopened [:result :value])))))

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
            (doseq [prop-kw [:operation :target]]
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
