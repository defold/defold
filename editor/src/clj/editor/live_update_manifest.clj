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

(ns editor.live-update-manifest
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.editor-extensions.runtime :as rt]
            [editor.fs :as fs]
            [editor.future :as future]
            [editor.handler :as handler]
            [editor.id :as id]
            [editor.localization :as localization]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [internal.java :as java]
            [util.coll :as coll]
            [util.fn :as fn])
  (:import [com.dynamo.bob ClassLoaderScanner Progress]
           [com.dynamo.bob.archive.liveupdate PackageArtifacts PackageBuilder PackageProvider$Property PackageProvider$PropertyType PackageProviders PackageProviders$Schema PackageResolver PackageResolver$Diagnostic PackageResolver$PackageReferences PackageResolver$PresetError PackageResolver$Selection PackageResolver$Severity]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ContainerPreset LiveUpdateManifest$FileCompressionPreset LiveUpdateManifest$ManifestDesc]
           [java.nio.file Path]
           [java.util.function Function]))

(set! *warn-on-reflection* true)

(def ^:private icon "icons/32/Icons_04-Project-file.png")
(def ^:private package-icon "icons/32/Icons_42-Layers.png")

(defn- preset-error-message [^PackageResolver$PresetError error]
  (localization/message (str "liveupdate.compression.error." (.code error))
                        (into {} (.arguments error))))

(defn diagnostic-message
  "Retains structured preset validation for localization in the manifest view."
  [^PackageResolver$Diagnostic diagnostic]
  (if-let [error (.presetError diagnostic)]
    (localization/message "liveupdate.compression.error.preset"
                          {"name" (get (.arguments error) "name")
                           "error" (preset-error-message error)})
    (.message diagnostic)))

(defn- set-target [evaluation-context self old-target new-target]
  ;; Preserve authored references during loading, but do not restore a stale
  ;; reference that became cyclic while this rule was in Path mode.
  (when (and old-target (= :package new-target))
    (let [package (g/node-value self :package evaluation-context)
          targets (g/node-value self :package-targets evaluation-context)]
      (when (and (not (string/blank? package))
                 (coll/not-any? #(= package %) targets))
        (g/set-property self :package "")))))

(g/defnode RuleNode
  (inherits outline/OutlineNode)
  (input read-only g/Bool)
  (input package-targets g/Any)
  (input rule-diagnostics g/Any)
  (display-order [:target :id :operation :path :no-match :package])
  (property id g/Str (default "Rule")
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic visible (g/fnk [target] (not= :package target)))
            (dynamic label (properties/label-dynamic :liveupdate-rule :id)))
  (property order g/Int (default 0) (dynamic visible (g/constantly false)))
  (property operation g/Keyword (default :add)
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-rule :operation))
            (dynamic edit-type (g/constantly {:type :radio-group
                                              :options [[:add (localization/message "liveupdate.add")]
                                                        [:remove (localization/message "liveupdate.remove")]]})))
  (property target g/Keyword (default :path)
            (set set-target)
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-rule :target))
            (dynamic edit-type (g/constantly {:type :radio-group
                                              :options [[:path (localization/message "liveupdate.path")]
                                                        [:package (localization/message "liveupdate.package")]]})))
  (property path g/Str
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-rule :path))
            (dynamic visible (g/fnk [target] (not= :package target)))
            (dynamic error (g/fnk [_node-id target path rule-diagnostics]
                             (when (and (not= :package target) path)
                               (when-let [^PackageResolver$Diagnostic diagnostic (get rule-diagnostics _node-id)]
                                 (g/->error _node-id :path
                                            (if (= PackageResolver$Severity/ERROR (.severity diagnostic)) :fatal :warning)
                                            path (.message diagnostic)))))))
  (property no-match g/Keyword (default :warn)
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-rule :no-match))
            (dynamic visible (g/fnk [target] (not= :package target)))
            (dynamic edit-type (g/constantly {:type :radio-group
                                              :options [[:warn (localization/message "liveupdate.no-match.warn")]
                                                        [:error (localization/message "liveupdate.no-match.error")]
                                                        [:ignore (localization/message "liveupdate.no-match.ignore")]]})))
  (property package g/Str (default "")
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-rule :package))
            (dynamic visible (g/fnk [target] (not= :path target)))
            (dynamic edit-type (g/fnk [package-targets]
                                 {:type :choicebox
                                  :options (mapv (juxt identity identity) package-targets)}))
            (dynamic error (g/fnk [_node-id target package rule-diagnostics]
                             (when (not= :path target)
                               (when-let [^PackageResolver$Diagnostic diagnostic (get rule-diagnostics _node-id)]
                                 (g/->error _node-id :package :fatal package (.message diagnostic)))))))
  (output data g/Any :cached
          (g/fnk [_node-id id order operation target path no-match package]
            {:node-id _node-id
             :order order
             :value (cond-> {:name id}
                      (not= :add operation) (assoc :operation operation)
                      (and (not= :package target) path) (assoc :path path)
                      (and (not= :package target) (not= :warn no-match)) (assoc :no-match no-match)
                      (not= :path target) (assoc :package package))}))
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id id order operation target package read-only rule-diagnostics]
            (let [^PackageResolver$Diagnostic diagnostic (get rule-diagnostics _node-id)]
              {:node-id _node-id
               :node-outline-key (str order)
               :label (if (= :package target)
                        (localization/message "liveupdate.package-rule"
                                              {"operation" (if (= :add operation) "+" "−")
                                               "name" package})
                        (str (if (= :add operation) "+ " "− ") id))
               :read-only (boolean read-only)
               :order order
               :outline-error? (= PackageResolver$Severity/ERROR (when diagnostic (.severity diagnostic)))
               :icon (if (= :package target) package-icon icon)}))))

(defn- attach-rule [parent child]
  [(g/connect parent :read-only child :read-only)
   (g/connect parent :package-targets child :package-targets)
   (g/connect parent :rule-diagnostics child :rule-diagnostics)
   (g/connect child :_node-id parent :nodes)
   (g/connect child :data parent :rule-data)
   (g/connect child :node-outline parent :child-outlines)])

(defn- rename-package [evaluation-context self old-name new-name]
  (when-let [manifest (core/scope (:basis evaluation-context) self)]
    (let [packages (g/node-value manifest :package-nodes evaluation-context)]
      ;; Duplicate old or new names cannot identify a rule's target package.
      (when (coll/not-any? (fn [node-id]
                             (let [name (g/node-value node-id :id evaluation-context)]
                               (and (not= self node-id)
                                    (or (= old-name name) (= new-name name)))))
                           packages)
        (coll/into-> packages []
          (mapcat #(g/node-value % :nodes evaluation-context))
          (filter #(and (g/node-instance? (:basis evaluation-context) RuleNode %)
                        (= old-name (g/node-value % :package evaluation-context))))
          (map #(g/set-property % :package new-name)))))))

(g/defnode FileCompressionSelection
  (input read-only g/Bool)
  (input file-preset-options g/Any)
  (property file-compression-preset g/Str (default "")
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-package :file-compression-preset))
            (dynamic edit-type (g/fnk [file-preset-options]
                                 {:type :choicebox :options file-preset-options}))
            (dynamic error (g/fnk [file-compression-error] file-compression-error)))
  (output file-compression-error g/Any
          (g/fnk [_node-id file-compression-preset file-preset-options]
            (when (coll/not-any? #(= file-compression-preset (first %)) file-preset-options)
              (g/->error _node-id :file-compression-preset :fatal file-compression-preset
                         (localization/message "liveupdate.compression.unknown-file" {"name" file-compression-preset}))))))

(g/defnode PackageNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (inherits FileCompressionSelection)
  (display-order [:id :container-preset :file-compression-preset])
  (input container-preset-options g/Any)
  (input package-references g/Any)
  (input diagnostics g/Any)
  (output read-only g/Bool (g/fnk [read-only] (boolean read-only)))
  (output name-error g/Any :cached
          (g/fnk [_node-id id package-references]
            (when-let [^PackageResolver$Diagnostic diagnostic
                       (coll/first-where #(and (= id (.packageName ^PackageResolver$Diagnostic %))
                                               (= -1 (.rule ^PackageResolver$Diagnostic %)))
                                         (.diagnostics ^PackageResolver$PackageReferences package-references))]
              (g/->error _node-id :id :fatal id (.message diagnostic)))))
  (property id g/Str (default "package")
            (dynamic read-only? (g/fnk [read-only] read-only))
            (set rename-package)
            (dynamic label (properties/label-dynamic :liveupdate-package :id))
            (dynamic error (g/fnk [name-error] name-error)))
  (property order g/Int (default 0) (dynamic visible (g/constantly false)))
  (property container-preset g/Str (default "")
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-package :container-preset))
            (dynamic edit-type (g/fnk [container-preset-options]
                                 {:type :choicebox :options container-preset-options}))
            (dynamic error (g/fnk [container-error] container-error)))
  (output container-error g/Any
          (g/fnk [_node-id container-preset container-preset-options]
            (when (coll/not-any? #(= container-preset (first %)) container-preset-options)
              (g/->error _node-id :container-preset :fatal container-preset
                         (localization/message "liveupdate.compression.unknown-container" {"name" container-preset})))))
  (input rule-data g/Any :array)
  (output package-targets g/Any
          (g/fnk [id package-references]
            (get (.targets ^PackageResolver$PackageReferences package-references) id)))
  (output rule-diagnostics g/Any :cached
          (g/fnk [id rule-data diagnostics]
            (let [rules (vec (sort-by :order rule-data))]
              (coll/into-> diagnostics {}
                (keep (fn [^PackageResolver$Diagnostic diagnostic]
                        (when (= id (.packageName diagnostic))
                          (when-let [rule (get rules (.rule diagnostic))]
                            [(:node-id rule) diagnostic]))))))))
  (output data g/Any :cached
          (g/fnk [_node-id id order container-preset file-compression-preset rule-data]
            (let [rules (vec (sort-by :order rule-data))]
              {:node-id _node-id
               :order order
               :rules rules
               :value (cond-> {:name id}
                        (not (string/blank? file-compression-preset)) (assoc :file-compression-preset file-compression-preset)
                        (coll/not-empty rules) (assoc :rules (mapv :value rules))
                        (not (string/blank? container-preset)) (assoc :container-preset container-preset))})))
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id id order child-outlines read-only ^:try name-error ^:try file-compression-error ^:try container-error]
            {:node-id _node-id
             :node-outline-key id
             :label id
             :order order
             :icon package-icon
             :read-only (boolean read-only)
             :outline-error? (or (g/error-fatal? name-error) (g/error-fatal? file-compression-error) (g/error-fatal? container-error))
             :children (vec (sort-by :order child-outlines))
             :child-reqs [{:node-type RuleNode :tx-attach-fn attach-rule}]})))

(g/defnode BasePackageNode
  (inherits outline/OutlineNode)
  (inherits FileCompressionSelection)
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id ^:try file-compression-error]
            {:node-id _node-id
             :node-outline-key "base"
             :label (localization/message "liveupdate.base")
             :icon "icons/32/Icons_S_15_lock.png"
             :read-only true
             :outline-error? (g/error-fatal? file-compression-error)})))

(defn- attach-package [parent child]
  [(g/connect parent :read-only child :read-only)
   (g/connect parent :file-preset-options child :file-preset-options)
   (g/connect parent :container-preset-options child :container-preset-options)
   (g/connect parent :package-references child :package-references)
   (g/connect parent :diagnostics child :diagnostics)
   (g/connect child :_node-id parent :nodes)
   (g/connect child :_node-id parent :package-nodes)
   (g/connect child :data parent :package-data)
   (g/connect child :node-outline parent :child-outlines)])

(defn- attach-pasted-package [parent child]
  [(g/expand-ec
     (fn resolve-package-id [evaluation-context]
       (g/update-property child :id id/resolve
                          (mapv #(get-in % [:value :name])
                                (g/node-value parent :package-data evaluation-context)))))
   (attach-package parent child)])

(defn- rename-preset [evaluation-context self old-name new-name]
  ;; Empty selections mean the built-in defaults, not an unnamed preset.
  (when-not (or (string/blank? old-name) (string/blank? new-name))
    (when-let [group (core/scope (:basis evaluation-context) self)]
      ;; Duplicate old or new names cannot identify a selection's preset.
      (when (coll/not-any? (fn [node-id]
                             (let [name (g/node-value node-id :id evaluation-context)]
                               (and (not= self node-id)
                                    (or (= old-name name) (= new-name name)))))
                           (g/node-value group :nodes evaluation-context))
        (when-let [manifest (core/scope (:basis evaluation-context) group)]
          (let [container (= :container (g/node-value group :kind evaluation-context))
                node-type (if container PackageNode FileCompressionSelection)
                property (if container :container-preset :file-compression-preset)]
            (coll/into-> (g/node-value manifest :nodes evaluation-context) []
              (filter #(g/node-instance? (:basis evaluation-context) node-type %))
              (filter #(= old-name (g/node-value % property evaluation-context)))
              (map #(g/set-property % property new-name)))))))))

(defn- preset-property-error [node-id id preset-errors property value]
  (when-let [^PackageResolver$PresetError error (get-in preset-errors [id property])]
    (g/->error node-id (if (= "name" property) :id (keyword property)) :fatal value
               (preset-error-message error))))

(g/defnode PresetNode
  (inherits outline/OutlineNode)
  (input read-only g/Bool)
  (input preset-errors g/Any)
  (property id g/Str
            (set rename-preset)
            (dynamic label (properties/label-dynamic :liveupdate-preset :id))
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic error (g/fnk [_node-id id preset-errors]
                             (preset-property-error _node-id id preset-errors "name" id))))
  (property order g/Int (default 0) (dynamic visible (g/constantly false)))
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id id order read-only preset-errors]
            {:node-id _node-id :node-outline-key (str order) :label id :order order :icon icon
             :read-only (boolean read-only) :outline-error? (contains? preset-errors id)})))

(g/defnode FileCompressionPresetNode
  (inherits PresetNode)
  (display-order [:id :file-compression :lz4-level])
  (property file-compression g/Keyword (default (protobuf/default LiveUpdateManifest$FileCompressionPreset :file-compression))
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-preset :file-compression))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:lz4 "LZ4"]
                                                        [:none (localization/message "liveupdate.compression.none")]]})))
  (property lz4-level g/Int (default (protobuf/default LiveUpdateManifest$FileCompressionPreset :lz4-level))
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-preset :lz4-level))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options (mapv (fn [level] [level (str level)]) (range 1 13))}))
            (dynamic visible (g/fnk [file-compression] (= :lz4 file-compression)))
            (dynamic error (g/fnk [_node-id id lz4-level preset-errors]
                             (preset-property-error _node-id id preset-errors "lz4-level" lz4-level))))
  (output data g/Any :cached
          (g/fnk [_node-id id order file-compression lz4-level]
            {:node-id _node-id :order order
             :value (protobuf/make-map-without-defaults LiveUpdateManifest$FileCompressionPreset
                      :name id :file-compression file-compression :lz4-level lz4-level)})))

(g/defnode ContainerPresetNode
  (inherits PresetNode)
  (display-order [:id :provider :zip-level])
  (property provider g/Keyword (default :zip)
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-preset :provider))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options [[:zip "ZIP"]
                                                        [:folder (localization/message "liveupdate.folder")]]})))
  (property custom-provider g/Str (default "")
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-preset :custom-provider))
            (dynamic visible (g/constantly false))
            (dynamic error (g/fnk [_node-id id custom-provider preset-errors]
                             (preset-property-error _node-id id preset-errors "provider" custom-provider))))
  (property zip-level g/Int (default (protobuf/default LiveUpdateManifest$ContainerPreset :zip-level))
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-preset :zip-level))
            (dynamic edit-type (g/constantly {:type :choicebox
                                              :options (mapv (fn [level] [level (str level)]) (range -1 10))}))
            (dynamic visible (g/fnk [provider] (= :zip provider)))
            (dynamic error (g/fnk [_node-id id zip-level preset-errors]
                             (preset-property-error _node-id id preset-errors "zip-level" zip-level))))
  (output data g/Any :cached
          (g/fnk [_node-id id order provider custom-provider zip-level]
            {:node-id _node-id :order order
             :value (protobuf/make-map-without-defaults LiveUpdateManifest$ContainerPreset
                      :name id :provider (if (= :custom provider) custom-provider (name provider)) :zip-level zip-level)})))

(defn- attach-preset [parent child]
  [(g/connect parent :read-only child :read-only)
   (g/connect parent :preset-errors child :preset-errors)
   (g/connect child :_node-id parent :nodes)
   (g/connect child :data parent :preset-data)
   (g/connect child :node-outline parent :child-outlines)])

(defn- attach-pasted-preset [parent child]
  [(g/expand-ec
     (fn resolve-preset-id [evaluation-context]
       (g/update-property child :id id/resolve
                          (mapv #(get-in % [:value :name])
                                (g/node-value parent :preset-data evaluation-context)))))
   (attach-preset parent child)])

(g/defnode CompressionPresetsNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (property kind g/Keyword (dynamic visible (g/constantly false)))
  (input read-only g/Bool)
  (output read-only g/Bool (g/fnk [read-only] (boolean read-only)))
  (input preset-errors g/Any)
  (output preset-errors g/Any (g/fnk [preset-errors] preset-errors))
  (input preset-data g/Any :array)
  (output presets g/Any :cached (g/fnk [preset-data] (vec (sort-by :order preset-data))))
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id kind child-outlines]
            {:node-id _node-id :node-outline-key (name kind)
             :label (localization/message (str "liveupdate.compression." (name kind))) :icon icon :read-only true
             :children (vec (sort-by :order child-outlines))
             :child-reqs [{:node-type (case kind :container ContainerPresetNode :file FileCompressionPresetNode)
                           :tx-attach-fn attach-pasted-preset}]})))

(defn- make-preset [parent kind order preset]
  (case kind
    :container
    (let [provider (:provider preset "zip")]
      (g/make-nodes [child [ContainerPresetNode :id (:name preset) :order order
                            :provider (case provider "zip" :zip "folder" :folder :custom)
                            :custom-provider (if (#{"zip" "folder"} provider) "" provider)
                            :zip-level (:zip-level preset (protobuf/default LiveUpdateManifest$ContainerPreset :zip-level))]]
        (attach-preset parent child)))
    :file
    (g/make-nodes [child [FileCompressionPresetNode :id (:name preset) :order order
                          :file-compression (:file-compression preset (protobuf/default LiveUpdateManifest$FileCompressionPreset :file-compression))
                          :lz4-level (:lz4-level preset (protobuf/default LiveUpdateManifest$FileCompressionPreset :lz4-level))]]
      (attach-preset parent child))))

(defn- set-provider-value [settings provider property value]
  (let [provider-index (coll/first-index-where #(= provider (:provider %)) settings)
        provider-index (or provider-index (count settings))
        properties (get-in settings [provider-index :properties] [])
        property-index (coll/first-index-where #(= property (:name %)) properties)
        properties (assoc properties (or property-index (count properties)) {:name property :value (str value)})]
    (assoc settings provider-index {:provider provider :properties properties})))

(defn- clear-provider-value [settings provider property]
  (coll/into-> settings []
    (keep (fn [entry]
            (if (not= provider (:provider entry))
              entry
              (let [properties (filterv #(not= property (:name %)) (:properties entry))]
                (when (coll/not-empty properties)
                  (assoc entry :properties properties))))))))

(defn- set-provider-property [provider property _evaluation-context node-id _old-value value]
  (g/update-property node-id :provider-settings set-provider-value provider property value))

(defn- clear-provider-property [provider property node-id _prop-kw]
  (g/update-property node-id :provider-settings clear-provider-value provider property))

(defn- provider-property [node-id read-only provider ^PackageProvider$Property description authored]
  (let [id (.id description)
        type (condp = (.type description)
               PackageProvider$PropertyType/BOOLEAN g/Bool
               PackageProvider$PropertyType/INTEGER g/Int
               PackageProvider$PropertyType/NUMBER g/Num
               g/Str)
        parse (fn [value]
                (try
                  {:value (.parse description value)}
                  (catch IllegalArgumentException error
                    {:value value
                     :error (g/->error node-id :provider-settings :fatal value (ex-message error))})))
        default (parse (.defaultValue description))
        value (when (contains? authored id) (parse (get authored id)))
        error (:error (or value default))
        choices (.choices description)]
    {:node-id node-id
     :prop-kw :provider-settings
     :label (localization/message (str "liveupdate.provider." provider ".property." id) {} (.label description))
     :type type
     :read-only? read-only
     :value (:value value)
     :original-value (:value default)
     :error error
     :edit-type (cond-> {:type (if error g/Str type)
                         :category-separator true
                         :set-fn (fn/partial set-provider-property provider id)
                         :clear-fn (fn/partial clear-provider-property provider id)}
                  (coll/not-empty choices)
                  (assoc :type :choicebox
                         :options (mapv (fn [choice] [(.parse description choice) choice]) choices)))}))

(defn- produce-properties [node-id declared-properties provider-settings packages container-presets read-only schemas]
  (if (g/error? schemas)
    (-> declared-properties
        (assoc-in [:properties :provider-error] {:node-id node-id
                                                 :type g/Str
                                                 :label (localization/message "liveupdate.providers")
                                                 :read-only? true
                                                 :value ""
                                                 :error schemas})
        (update :display-order conj :provider-error))
    (reduce (fn [properties provider]
              (let [^PackageProviders$Schema schema (get schemas provider)
                    authored (coll/into-> provider-settings {}
                               (filter #(= provider (:provider %)))
                               (mapcat :properties)
                               (map (juxt :name :value)))
                    descriptions (if schema (vec (.properties schema)) [])
                    known-ids (coll/into-> descriptions #{} (map #(.id ^PackageProvider$Property %)))
                    ;; Retain editable settings when a plugin is missing.
                    descriptions (coll/into-> authored descriptions
                                   (remove (fn [[id]] (contains? known-ids id)))
                                   (map (fn [[id]] (PackageProvider$Property. id id PackageProvider$PropertyType/STRING "" []))))
                    property-keys (mapv #(keyword (str "live-update-provider." provider) (.id ^PackageProvider$Property %)) descriptions)]
                (if (coll/empty? descriptions)
                  properties
                  (-> properties
                      (update :properties into
                              (mapv (fn [key description]
                                      [key (provider-property node-id read-only provider description authored)])
                                    property-keys descriptions))
                      (update :display-order conj
                              (into [(localization/message (str "liveupdate.provider." provider ".label") {} (if schema (.label schema) provider))]
                                    property-keys))))))
            declared-properties
            (let [providers (coll/into-> container-presets {"" "zip"}
                              (map (fn [{:keys [value]}] [(:name value) (:provider value "zip")])))]
              (coll/into-> packages [] (keep #(get providers (get-in % [:value :container-preset] ""))) (distinct))))))

(g/defnode LiveUpdateManifestNode
  (inherits resource-node/ResourceNode)
  (property provider-settings g/Any (default []) (dynamic visible (g/constantly false)))
  (property provider-schemas g/Any (default {}) (dynamic visible (g/constantly false)))
  (property output-directory g/Str (default (protobuf/default LiveUpdateManifest$ManifestDesc :output-directory))
            (dynamic read-only? (g/fnk [read-only] read-only))
            (dynamic label (properties/label-dynamic :liveupdate-manifest :output-directory))
            (dynamic edit-type (g/fnk [project-root ^:try resolved-output-directory]
                                 {:type :directory
                                  :directory (if (g/error? resolved-output-directory) project-root (some-> resolved-output-directory str))}))
            (dynamic error (g/fnk [^:try resolved-output-directory]
                             (when (g/error? resolved-output-directory) resolved-output-directory))))
  (output read-only g/Bool (g/fnk [resource] (resource/read-only? resource)))
  (input project-root g/Str)
  (output resolved-output-directory g/Any :cached
          (g/fnk [_node-id output-directory project-root]
            ;; Unloaded resources have no workspace connections.
            (when project-root
              (try
                (PackageBuilder/resolveOutputDirectory output-directory (Path/of project-root (make-array String 0))
                                                       (reify Function (apply [_ name] (System/getenv name))))
                (catch IllegalArgumentException error
                  (g/->error _node-id :output-directory :fatal output-directory (ex-message error)))))))
  (input resource-map g/Any)
  (property generated-paths g/Any (default #{}) (dynamic visible (g/constantly false)))
  (property matched-diagnostics g/Any (dynamic visible (g/constantly false)))
  (input package-nodes g/NodeID :array)
  (input package-data g/Any :array)
  (input base-outline outline/OutlineData)
  (input base-file-compression-preset g/Str)
  (input container-presets g/Any)
  (input file-presets g/Any)
  (input container-presets-outline outline/OutlineData)
  (input file-presets-outline outline/OutlineData)
  (output container-preset-options g/Any :cached
          (g/fnk [container-presets]
            (coll/into-> container-presets [["" (localization/message "liveupdate.compression.default-container")]]
              (map #(let [name (get-in % [:value :name])] [name name])))))
  (output file-preset-options g/Any :cached
          (g/fnk [file-presets]
            (coll/into-> file-presets [["" (localization/message "liveupdate.compression.default-file")]]
              (map #(let [name (get-in % [:value :name])] [name name])))))
  (output container-preset-errors g/Any :cached
          (g/fnk [save-value]
            (PackageResolver/containerPresetErrors (protobuf/map->pb LiveUpdateManifest$ManifestDesc save-value))))
  (output file-preset-errors g/Any :cached
          (g/fnk [save-value]
            (PackageResolver/fileCompressionPresetErrors (protobuf/map->pb LiveUpdateManifest$ManifestDesc save-value))))
  (output packages g/Any :cached (g/fnk [package-data] (vec (sort-by :order package-data))))
  (output _properties g/Properties :cached
          (g/fnk [_node-id _declared-properties provider-settings packages container-presets read-only ^:try provider-schemas]
            (produce-properties _node-id _declared-properties provider-settings packages container-presets read-only provider-schemas)))
  (output save-value g/Any :cached
          (g/fnk [packages output-directory provider-settings base-file-compression-preset container-presets file-presets]
            (protobuf/make-map-without-defaults LiveUpdateManifest$ManifestDesc
              :output-directory output-directory
              :base-file-compression-preset base-file-compression-preset
              :file-compression-presets (mapv :value file-presets)
              :container-presets (mapv :value container-presets)
              :providers provider-settings
              :packages (mapv :value packages))))
  (output package-references g/Any :cached
          (g/fnk [save-value]
            (PackageResolver/packageReferences (protobuf/map->pb LiveUpdateManifest$ManifestDesc save-value))))
  (output source-paths g/Any :cached
          (g/fnk [resource-map generated-paths]
            (coll/into-> resource-map []
              (keep (fn [[path source]]
                      (when (and (= :file (resource/source-type source))
                                 (not (PackageArtifacts/isGeneratedPath path generated-paths))
                                 (or (resource/file-resource? source) (resource/zip-resource? source)))
                        path))))))
  (output selection g/Any :cached
          (g/fnk [save-value source-paths]
            (PackageResolver/select
              (protobuf/map->pb LiveUpdateManifest$ManifestDesc save-value)
              source-paths)))
  (output selection-inputs g/Any :cached
          (g/fnk [save-value resource-map generated-paths]
            [save-value resource-map generated-paths]))
  (output diagnostics g/Any :cached
          (g/fnk [save-value selection-inputs matched-diagnostics]
            (if (= selection-inputs (:inputs matched-diagnostics))
              (:diagnostics matched-diagnostics)
              (PackageResolver/validate (protobuf/map->pb LiveUpdateManifest$ManifestDesc save-value)))))
  (output selected-paths g/Any :cached
          (g/fnk [_node-id selection]
            (or (g/flatten-errors
                  (coll/into-> (.diagnostics ^PackageResolver$Selection selection) []
                    (keep (fn [^PackageResolver$Diagnostic diagnostic]
                            (when (= PackageResolver$Severity/ERROR (.severity diagnostic))
                              (g/->error _node-id :selected-paths :fatal nil (diagnostic-message diagnostic)))))))
                (coll/into-> (.values (.matches ^PackageResolver$Selection selection)) (sorted-set) cat))))
  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id child-outlines base-outline container-presets-outline file-presets-outline]
            {:node-id _node-id
             :node-outline-key "Live Update Manifest"
             :label (localization/message "resource.type.archivemanifest")
             :icon icon
             :children (into [base-outline
                              {:node-id _node-id
                               :node-outline-key "compression-types"
                               :label (localization/message "liveupdate.compression.types")
                               :icon icon
                               :read-only true
                               :properties {:properties {}}
                               :children [container-presets-outline file-presets-outline]}]
                             (sort-by :order child-outlines))
             :child-reqs [{:node-type PackageNode :tx-attach-fn attach-pasted-package}]}))
  (output build-targets g/Any (g/constantly [])))

(defn- make-rule [parent order rule]
  (g/make-nodes [child [RuleNode :id (:name rule)
                        :order order
                        :operation (:operation rule :add)
                        :target (cond
                                  (and (contains? rule :path) (contains? rule :package)) :invalid
                                  (contains? rule :package) :package
                                  :else :path)
                        :path (:path rule)
                        :no-match (:no-match rule :warn)
                        :package (:package rule "")]]
    (attach-rule parent child)))

(defn- make-package [parent order package]
  (g/make-nodes [child [PackageNode :id (:name package)
                        :order order
                        :container-preset (:container-preset package "")
                        :file-compression-preset (:file-compression-preset package "")]]
    (attach-package parent child)
    (coll/into-> (:rules package) []
      (map-indexed (fn [index rule] (make-rule child index rule))))))

(defn- load-environment [workspace nodes]
  (let [schemas (try
                  (into {} (PackageProviders/describe (ClassLoaderScanner. java/class-loader)))
                  (catch Exception error
                    (g/error-fatal (ex-message error))))
        generated-paths (set (PackageArtifacts/generatedPaths (.toPath (workspace/project-directory workspace))))]
    (mapv (fn [node-id]
            (g/set-properties node-id :provider-schemas schemas :generated-paths generated-paths))
          nodes)))

(defn- listen-for-resource-changes! [workspace]
  ;; Loading happens after the project's plugin listener is installed. Keep one
  ;; listener per workspace, finding current nodes after reloads.
  (let [listener (reify resource/ResourceListener
                   (handle-changes [_ _changes _render-progress!]
                     (let [basis (g/now)
                           nodes (coll/into-> (g/outputs basis workspace :resource-map) []
                                   (map gt/target-id)
                                   (filter #(g/node-instance? basis LiveUpdateManifestNode %)))]
                       (when (coll/not-empty nodes)
                         (g/transact {:undoable false} (load-environment workspace nodes))))))]
    (force
      (g/user-data-swap! workspace ::resource-listener
                         (fn resource-listener [existing]
                           (or existing
                               (delay (workspace/add-resource-listener! workspace 0 listener))))))))

(defn- load-manifest [{:keys [workspace]} {:keys [source-value] self :node-id}]
  ;; Resource loaders construct transaction data without querying the graph.
  [(g/callback listen-for-resource-changes! workspace)
   (g/set-property self :output-directory (:output-directory source-value (protobuf/default LiveUpdateManifest$ManifestDesc :output-directory)))
   (g/set-property self :provider-settings (:providers source-value []))
   (load-environment workspace [self])
   (g/connect workspace :root self :project-root)
   (g/connect workspace :resource-map self :resource-map)
   (g/make-nodes [base [BasePackageNode :file-compression-preset (:base-file-compression-preset source-value "")]]
     (g/connect self :read-only base :read-only)
     (g/connect self :file-preset-options base :file-preset-options)
     (g/connect base :_node-id self :nodes)
     (g/connect base :file-compression-preset self :base-file-compression-preset)
     (g/connect base :node-outline self :base-outline))
   (mapv (fn [kind presets-key errors-output data-input outline-input]
           (g/make-nodes [group [CompressionPresetsNode :kind kind]]
             (g/connect self :read-only group :read-only)
             (g/connect self errors-output group :preset-errors)
             (g/connect group :_node-id self :nodes)
             (g/connect group :presets self data-input)
             (g/connect group :node-outline self outline-input)
             (coll/into-> (get source-value presets-key) []
               (map-indexed (fn [index preset] (make-preset group kind index preset))))))
         [:container :file]
         [:container-presets :file-compression-presets]
         [:container-preset-errors :file-preset-errors]
         [:container-presets :file-presets]
         [:container-presets-outline :file-presets-outline])
   (coll/into-> (:packages source-value) []
     (map-indexed (fn [index package] (make-package self index package))))])

(defn- selection->add-parent [selection evaluation-context]
  (or (handler/adapt-single selection LiveUpdateManifestNode evaluation-context)
      (handler/adapt-single selection PackageNode evaluation-context)
      (handler/adapt-single selection CompressionPresetsNode evaluation-context)
      (when-let [preset (handler/adapt-single selection PresetNode evaluation-context)]
        (core/scope (:basis evaluation-context) preset))
      (when-let [rule (handler/adapt-single selection RuleNode evaluation-context)]
        (core/scope (:basis evaluation-context) rule))))

(defn- add-kind [parent evaluation-context]
  (cond
    (g/node-instance? (:basis evaluation-context) LiveUpdateManifestNode parent) :package
    (g/node-instance? (:basis evaluation-context) CompressionPresetsNode parent) :preset
    :else :rule))

(handler/defhandler :edit.add-embedded-component :workbench
  (active? [selection evaluation-context]
    (selection->add-parent selection evaluation-context))
  (enabled? [selection evaluation-context]
    (not (g/node-value (selection->add-parent selection evaluation-context) :read-only evaluation-context)))
  (label [selection evaluation-context]
    (localization/message (str "liveupdate.add-" (name (add-kind (selection->add-parent selection evaluation-context) evaluation-context)))))
  (run [app-view selection]
    (g/let-ec [parent (selection->add-parent selection evaluation-context)
               kind (add-kind parent evaluation-context)
               root (if (= :package kind) parent (core/scope (:basis evaluation-context) parent))
               children (g/node-value parent (case kind :package :package-data :preset :preset-data :rule-data) evaluation-context)
               order (inc (transduce (map :order) max -1 children))
               tx-data (case kind
                         :package (make-package parent order {:name (id/gen "package" (mapv #(get-in % [:value :name]) children))})
                         :preset (make-preset parent (g/node-value parent :kind evaluation-context) order {:name (id/gen "preset" (mapv #(get-in % [:value :name]) children))})
                         (make-rule parent order {:name "Rule" :path "/.*"}))]
      (let [op-seq (gensym)
            nodes (g/tx-nodes-added
                    (g/transact
                      (concat
                        (g/operation-sequence op-seq)
                        (g/operation-label (localization/message (str "liveupdate.add-" (name kind))))
                        tx-data)))]
        (g/let-ec [project (g/node-value app-view :project-id evaluation-context)
                   open-resource-nodes (g/node-value app-view :open-resource-nodes evaluation-context)
                   selection-tx-data (project/select project root nodes open-resource-nodes evaluation-context)]
          (g/transact
            (concat
              (g/operation-sequence op-seq)
              selection-tx-data)))))))

(defn- reorder [node-id offset evaluation-context]
  (let [parent (core/scope (:basis evaluation-context) node-id)
        label (if (g/node-instance? (:basis evaluation-context) RuleNode node-id) :rule-data :package-data)
        children (vec (sort-by :order (g/node-value parent label evaluation-context)))
        index (.indexOf ^java.util.List (mapv :node-id children) node-id)
        target (+ index offset)]
    (when (< -1 target (count children))
      ;; Renumber the sequence as well, so pasted nodes with equal order values
      ;; acquire a stable position on their first move.
      (let [other (children target)
            current (children index)
            reordered (assoc children index other target current)]
        (coll/into-> reordered []
          (map-indexed (fn [order child] (g/set-property (:node-id child) :order order))))))))

(defn- reorderable-selection [selection evaluation-context]
  (when-let [nodes (handler/selection->node-ids selection evaluation-context)]
    (when (= 1 (count nodes))
      (let [node (first nodes)
            basis (:basis evaluation-context)]
        (when (and (or (g/node-instance? basis RuleNode node) (g/node-instance? basis PackageNode node))
                   (not (g/node-value node :read-only evaluation-context)))
          node)))))

(handler/defhandler :edit.reorder-up :workbench
  (active? [selection evaluation-context] (reorderable-selection selection evaluation-context))
  (enabled? [selection evaluation-context] (coll/not-empty (reorder (reorderable-selection selection evaluation-context) -1 evaluation-context)))
  (run [selection]
    (g/let-ec [tx-data (reorder (reorderable-selection selection evaluation-context) -1 evaluation-context)]
      (g/transact tx-data))))

(handler/defhandler :edit.reorder-down :workbench
  (active? [selection evaluation-context] (reorderable-selection selection evaluation-context))
  (enabled? [selection evaluation-context] (coll/not-empty (reorder (reorderable-selection selection evaluation-context) 1 evaluation-context)))
  (run [selection]
    (g/let-ec [tx-data (reorder (reorderable-selection selection evaluation-context) 1 evaluation-context)]
      (g/transact tx-data))))

(defn editor-script-env
  "Returns the private Lua API for the built-in Live Update conversion script."
  [project]
  {"liveupdate"
   {"legacy_rules"
    (rt/suspendable-lua-fn legacy-rules [{:keys [evaluation-context]}]
      (let [workspace (project/workspace project evaluation-context)
            resource-map (g/node-value workspace :resource-map evaluation-context)
            project-directory (workspace/project-directory (:basis evaluation-context) workspace)
            save-data (mapv (fn [{:keys [resource save-value]}]
                              [(resource/proj-path resource) save-value (resource/resource-type resource (:basis evaluation-context))])
                            (project/dirty-save-data project evaluation-context))]
        (future/io
          ;; Conversion is a one-off operation. Bob uses a temporary snapshot so
          ;; it sees unsaved content without writing into the user's project.
          (let [generated-paths (PackageArtifacts/generatedPaths (.toPath project-directory))
                snapshot (fs/create-temp-directory! "defold-liveupdate-conversion-")]
            (try
              (let [contents (coll/into-> save-data {}
                               (keep (fn [[path save-value resource-type]]
                                       (when-let [content (resource-node/save-content save-value resource-type)]
                                         [path content]))))]
                (doseq [[path source] resource-map
                        :when (and (= :file (resource/source-type source))
                                   (or (resource/file-resource? source) (resource/zip-resource? source))
                                   (not (string/starts-with? path "/builtins/"))
                                   (not (PackageArtifacts/isGeneratedPath path generated-paths)))]
                  (let [destination (io/file snapshot (subs path 1))]
                    (fs/create-parent-directories! destination)
                    (if-let [content (get contents path)]
                      (spit destination content :encoding "UTF-8")
                      (with-open [input (io/input-stream source)]
                        (io/copy input destination)))))
                (let [plan (PackageBuilder/buildPlan (.toPath snapshot) "" {} java/class-loader (Progress/discarding))]
                  (coll/join-to-string "\n" (mapv #(str "rules {\n" % "}\n") (PackageResolver/migrateLegacy plan)))))
              (finally
                (fs/delete-directory! snapshot)))))))}})

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "archivemanifest"
    :label (localization/message "resource.type.archivemanifest")
    :node-type LiveUpdateManifestNode
    :ddf-type LiveUpdateManifest$ManifestDesc
    :load-fn load-manifest
    :icon icon
    :view-types [:live-update-manifest :text]))
