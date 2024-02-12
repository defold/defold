(ns editor.code.transpilation
  (:require [dynamo.graph :as g]
            [editor.code.data :as code.data]
            [editor.code.resource :as code.resource]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [util.coll :refer [pair]])
  (:import [com.defold.editor CodeTranspiler CodeTranspiler$Import]
           [java.io ByteArrayOutputStream]))

(defn- resource->code-transpiler
  ^CodeTranspiler [resource]
  (let [resource-type (resource/resource-type resource)]
    (:code-transpiler resource-type)))

(defn- build-transpiled-code [build-resource _dep-resources user-data]
  (let [source-resource (:resource build-resource)
        code-transpiler (resource->code-transpiler source-resource)
        node-id (:node-id user-data)
        lines (:lines user-data)
        bytes (resource-io/with-error-translation source-resource node-id nil
                (with-open [reader (code.data/lines-reader lines)
                            output-stream (ByteArrayOutputStream.)]
                  (.build code-transpiler reader output-stream)))]
    (g/precluding-errors bytes
      {:resource build-resource
       :content bytes})))

(g/defnk produce-build-targets [import-build-targets lines]
  (let [user-data {:node-id _node-id
                   :lines lines}]
    (bt/with-content-hash
      {:node-id _node-id
       :resource (workspace/make-build-resource memory-resource)
       :build-fn build-transpiled-code
       :user-data user-data
       :deps import-build-targets})))

(g/defnk produce-code-transpiler [resource lines]
  (let [code-transpiler-factory (resource->code-transpiler-factory resource)
        proj-path (resource/proj-path resource)]
    (with-open [reader (code.data/lines-reader lines)]
      (.makeCodeTranspiler code-transpiler-factory reader proj-path))))

(g/defnode TranspiledCodeEditorResourceNode
  (inherits code.resource/CodeEditorResourceNode)

  ;; Overrides modified-lines property in CodeEditorResourceNode.
  (property modified-lines code.resource/Lines
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         resource (resource-node/resource basis self)
                         code-transpiler (resource->code-transpiler resource)

                         import->candidate-proj-paths
                         (try
                           (with-open [reader (code.data/lines-reader new-value)]
                             (.getImportCandidateProjPaths code-transpiler reader))
                           (catch Exception exception
                             (log/warn :exception exception)))]

                     (when (some? import->candidate-proj-paths)
                       (let [workspace (resource/workspace resource)
                             proj-path->resource #(workspace/find-resource workspace %)

                             import->candidate-resources
                             (into (sorted-set)
                                   (map (fn [^CodeTranspiler$Import import candidate-proj-paths]
                                          (let [candidate-resources
                                                (into []
                                                      (keep proj-path->resource)
                                                      (sort candidate-proj-paths))]
                                            (pair import candidate-resources)))))

                             (util/into-multiple
                               (pair (sorted-map)
                                     )
                               (pair (keep (fn [^CodeTranspiler$Import import candidate-proj-paths]
                                             (let [candidate-resources
                                                   (into []
                                                         (keep proj-path->resource)
                                                         (sort candidate-proj-paths))]
                                               (when (< 1 (count candidate-resources))
                                                 (pair import
                                                       (into (sorted-set)
                                                             candidate-proj-paths)))))))
                               import->candidate-proj-paths)

                             ambiguous-import->candidate-resources
                             (into (sorted-map)
                                   (keep (fn [[^CodeTranspiler$Import import candidate-proj-paths]]
                                           (when ())
                                           (pair {:row (.rowIndex import)
                                                  :import-string (.importString import)}
                                                 (into (sorted-set)
                                                       candidate-proj-paths))
                                           )
                                   import-string->candidate-proj-paths)

                             imported-resource-sets
                             (into []
                                   (map (fn [import-candidate-set]
                                          (into #{}
                                                (keep (fn [proj-path]
                                                        (workspace/find-resource workspace proj-path evaluation-context)))
                                                import-candidate-set)))
                                   import-candidate-sets)



                             imported-resources
                             (into []
                                   (keep try-parse-import)
                                   new-value)]))

                     (g/set-property self :imported-resources imported-resources)))))

  (property imported-resources resource/ResourceVec
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project basis self)
                         connections [[:build-targets :import-build-targets]]]
                     (concat
                       (g/disconnect-sources basis self :import-build-targets)
                       (map (fn [included-resource]
                              (:tx-data (project/connect-resource-node evaluation-context project included-resource self connections)))
                            new-value))))))

  (input import-build-targets g/Any :array)

  (output build-targets g/Any :cached produce-build-targets)

  (output code-transpiler CodeTranspiler :cached produce-code-transpiler))