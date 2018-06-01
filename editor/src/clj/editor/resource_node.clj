(ns editor.resource-node
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
  ordinary paths."
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [editor.outline :as outline])
  (:import [org.apache.commons.codec.digest DigestUtils]
           [java.io StringReader]))

(set! *warn-on-reflection* true)

(def unknown-icon "icons/32/Icons_29-AT-Unknown.png")

(defn resource-node-dependencies [resource-node-id evaluation-context]
  (let [resource (g/node-value resource-node-id :resource evaluation-context)]
    (when-some [dependencies-fn (:dependencies-fn (resource/resource-type resource))]
      (let [source-value (g/node-value resource-node-id :source-value evaluation-context)]
        (dependencies-fn source-value)))))

(g/defnk produce-save-data [_node-id resource save-value dirty?]
  (let [write-fn (:write-fn (resource/resource-type resource))]
    (cond-> {:resource resource :dirty? dirty? :value save-value :node-id _node-id}
            (and write-fn save-value) (assoc :content (write-fn save-value)))))

(g/defnode ResourceNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (inherits resource/ResourceNode)

  (extern editable? g/Bool (default true) (dynamic visible (g/constantly false)))

  (output save-data g/Any :cached produce-save-data)
  (output source-value g/Any :cached (g/fnk [resource editable?]
                                       (when-some [read-fn (:read-fn (resource/resource-type resource))]
                                         (when (and editable? (resource/exists? resource))
                                           (read-fn resource)))))
  (output reload-dependencies g/Any :cached (g/fnk [_node-id resource save-value]
                                              (when-some [dependencies-fn (:dependencies-fn (resource/resource-type resource))]
                                                (dependencies-fn save-value))))
  
  (output save-value g/Any (g/constantly nil))

  (output cleaned-save-value g/Any (g/fnk [resource save-value editable?]
                                          (when editable?
                                            (let [resource-type (resource/resource-type resource)
                                                  read-fn (:read-fn resource-type)
                                                  write-fn (:write-fn resource-type)]
                                              (if (and read-fn write-fn)
                                                (with-open [reader (StringReader. (write-fn save-value))]
                                                  (read-fn reader))
                                                save-value)))))
  (output dirty? g/Bool (g/fnk [cleaned-save-value source-value editable?]
                          (and editable? (some? cleaned-save-value) (not= cleaned-save-value source-value))))
  (output node-id+resource g/Any (g/fnk [_node-id resource] [_node-id resource]))
  (output own-build-errors g/Any (g/constantly nil))
  (output build-targets g/Any (g/constantly []))
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id _overridden-properties child-outlines own-build-errors resource source-outline]
           (let [rt (resource/resource-type resource)
                 children (cond-> child-outlines
                            source-outline (into (:children source-outline)))]
             {:node-id _node-id
              :label (or (:label rt) (:ext rt) "unknown")
              :icon (or (:icon rt) unknown-icon)
              :children children
              :outline-error? (g/error-fatal? own-build-errors)
              :outline-overridden? (not (empty? _overridden-properties))})))

  (output sha256 g/Str :cached (g/fnk [resource save-data]
                                 (let [content (get save-data :content ::no-content)]
                                   (if (= ::no-content content)
                                     (with-open [s (io/input-stream resource)]
                                       (DigestUtils/sha256Hex ^java.io.InputStream s))
                                     (DigestUtils/sha256Hex ^String content))))))

(defn make-ddf-dependencies-fn [ddf-type]
  (fn [source-value]
    (into []
          (comp
            (filter seq)
            (distinct))
          ((protobuf/get-fields-fn (protobuf/resource-field-paths ddf-type)) source-value))))

(defn register-ddf-resource-type [workspace & {:keys [ext node-type ddf-type load-fn dependencies-fn sanitize-fn icon view-types tags tag-opts label] :as args}]
  (let [read-fn (comp (or sanitize-fn identity) (partial protobuf/read-text ddf-type))
        args (assoc args
               :textual? true
               :load-fn (fn [project self resource]
                          (let [source-value (read-fn resource)]
                            (load-fn project self resource source-value)))
               :dependencies-fn (or dependencies-fn (make-ddf-dependencies-fn ddf-type))
               :read-fn read-fn
               :write-fn (partial protobuf/map->str ddf-type))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

(defn register-settings-resource-type [workspace & {:keys [ext node-type load-fn icon view-types tags tag-opts label] :as args}]
  (let [read-fn (fn [resource]
                  (with-open [setting-reader (io/reader resource)]
                    (settings-core/parse-settings setting-reader)))
        args (assoc args
               :textual? true
               :load-fn (fn [project self resource]
                          (let [source-value (read-fn resource)]
                            (load-fn project self resource source-value)))
               :read-fn read-fn
               :write-fn (comp settings-core/settings->str settings-core/settings-with-value))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))
