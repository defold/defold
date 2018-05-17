(ns integration.script-properties-test
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.atlas :as atlas]
            [editor.build-errors-view :as build-errors-view]
            [editor.code.script :as script]
            [editor.collection :as collection]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.game-object :as game-object]
            [editor.progress :as progress]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as tu]
            [support.test-support :refer [with-clean-system]]
            [util.murmur :as murmur])
  (:import (com.dynamo.gameobject.proto GameObject$CollectionDesc GameObject$PrototypeDesc)
           (com.dynamo.lua.proto Lua$LuaModule)
           (java.io ByteArrayOutputStream StringReader)))

(defn- unpack-property-declarations [property-declarations]
  (assert (map? property-declarations))
  (into {}
        (mapcat (fn [[entries-key values-key]]
                  (let [entries (get property-declarations entries-key)
                        values (get property-declarations values-key)]
                    (map (fn [{:keys [key index]}]
                           [key (values index)])
                         entries))))
        (vals properties/type->entry-keys)))

(defn- component [go-id id]
  (let [comps (->> (g/node-value go-id :node-outline)
                :children
                (map (fn [v] [(-> (:label v)
                                (string/split #" ")
                                first) (:node-id v)]))
                (into {}))]
    (comps id)))

(defmacro with-source [script-id source & body]
  `(let [orig# (tu/code-editor-source ~script-id)]
     (tu/code-editor-source! ~script-id ~source)
     ~@body
     (tu/code-editor-source! ~script-id orig#)))

(defn- prop [node-id prop-name]
  (let [key (properties/user-name->key prop-name)]
    (tu/prop (tu/prop-node-id node-id key) key)))

(defn- read-only? [node-id prop-name]
  (tu/prop-read-only? node-id (properties/user-name->key prop-name)))

(defn- overridden? [node-id prop-name]
  (let [key (properties/user-name->key prop-name)]
    (tu/prop-overridden? (tu/prop-node-id node-id key) key)))

(defn- prop! [node-id prop-name value]
  (let [key (properties/user-name->key prop-name)]
    (tu/prop! (tu/prop-node-id node-id key) key value)))

(defn- clear! [node-id prop-name]
  (let [key (properties/user-name->key prop-name)]
    (tu/prop-clear! (tu/prop-node-id node-id key) key)))

(defn- make-fake-file-resource [workspace proj-path text]
  (let [root-dir (workspace/project-path workspace)]
    (tu/make-fake-file-resource workspace (.getPath root-dir) (io/file root-dir proj-path) (.getBytes text "UTF-8"))))

(deftest script-properties-source
  (tu/with-loaded-project
    (let [script-id (tu/resource-node project "/script/props.script")]
      (testing "reading values"
               (is (= 1.0 (prop script-id "number")))
               (is (read-only? script-id "number")))
      (testing "broken prop defs" ;; string vals are not supported
               (with-source script-id "go.property(\"number\", \"my_string\")\n"
                 (is (nil? (prop script-id "number"))))))))

(deftest script-properties-component
  (tu/with-loaded-project
    (let [go-id (tu/resource-node project "/game_object/props.go")
          script-c (component go-id "script")]
      (is (= 2.0 (prop script-c "number")))
      (is (overridden? script-c "number"))
      (prop! script-c "number" 3.0)
      (is (= 3.0 (prop script-c "number")))
      (is (overridden? script-c "number"))
      (clear! script-c "number")
      (is (= 1.0 (prop script-c "number")))
      (is (not (overridden? script-c "number"))))))

(deftest script-properties-broken-component
  (tu/with-loaded-project
    (let [go-id (tu/resource-node project "/game_object/type_faulty_props.go")
          script-c (component go-id "script")]
      (is (not (overridden? script-c "number")))
      (is (= 1.0 (prop script-c "number")))
      (prop! script-c "number" 3.0)
      (is (overridden? script-c "number"))
      (is (= 3.0 (prop script-c "number")))
      (clear! script-c "number")
      (is (not (overridden? script-c "number")))
      (is (= 1.0 (prop script-c "number"))))))

(deftest script-properties-component-load-order
  (tu/with-loaded-project "test/resources/empty_project"
    (let [script-text (slurp "test/resources/test_project/script/props.script")
          game-object-text (slurp "test/resources/test_project/game_object/props.go")
          script-resource (make-fake-file-resource workspace "script/props.script" script-text)
          game-object-resource (make-fake-file-resource workspace "game_object/props.go" game-object-text)]
      (doseq [resource-load-order [[script-resource game-object-resource]
                                   [game-object-resource script-resource]]]
        (let [project (tu/setup-project! workspace resource-load-order)
              nodes-by-resource-path (g/node-value project :nodes-by-resource-path)
              game-object (nodes-by-resource-path "/game_object/props.go")
              script-component (component game-object "script")]
          (doseq [[prop-name prop-value] {"atlas" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.atlas")
                                          "bool" true
                                          "hash" "hash2"
                                          "material" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.material")
                                          "number" 2.0
                                          "quat" [180.0 0.0, 0.0]
                                          "texture" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.png")
                                          "url" "/url"
                                          "vec3" [1.0 2.0 3.0]
                                          "vec4" [1.0 2.0 3.0 4.0]}]
            (is (overridden? script-component prop-name))
            (is (= prop-value (prop script-component prop-name)))))))))

(deftest script-properties-collection
  (tu/with-loaded-project
    (doseq [[resource paths val] [["/collection/props.collection" [[0 0] [1 0]] 3.0]
                                  ["/collection/sub_props.collection" [[0 0 0]] 4.0]
                                  ["/collection/sub_sub_props.collection" [[0 0 0 0]] 5.0]]
            path paths]
      (let [coll-id (tu/resource-node project resource)]
        (let [outline (tu/outline coll-id path)
              script-c (:node-id outline)]
          (is (:outline-overridden? outline))
          (is (= val (prop script-c "number")))
          (is (overridden? script-c "number")))))))

(deftest script-properties-broken-collection
  (tu/with-loaded-project
    ;; [0 0] instance script, bad collection level override, fallback to instance override = 2.0
    ;; [1 0] embedded instance script, bad collection level override, fallback to script setting = 1.0
    ;; [2 0] type faulty instance script, bad collection level override, fallback to script setting = 1.0
    ;; [3 0] type faulty instance script, proper collection-level override = 3.0
    (doseq [[resource path-vals] [["/collection/type_faulty_props.collection" [[[0 0] false 2.0] [[1 0] false 1.0] [[2 0] false 1.0]] [[3 0] true 3.0]]]
            [path overriden val] path-vals]
      (let [coll-id (tu/resource-node project resource)]
        (let [outline (tu/outline coll-id path)
              script-c (:node-id outline)]
          (is (= overriden (:outline-overridden? outline)))
          (is (= val (prop script-c "number")))
          (is (= overriden (overridden? script-c "number"))))))))

(deftest script-properties-collection-load-order
  (tu/with-loaded-project "test/resources/empty_project"
    (let [script-text (slurp "test/resources/test_project/script/props.script")
          game-object-text (slurp "test/resources/test_project/game_object/props.go")
          collection-text (slurp "test/resources/test_project/collection/props.collection")
          script-resource (make-fake-file-resource workspace "script/props.script" script-text)
          game-object-resource (make-fake-file-resource workspace "game_object/props.go" game-object-text)
          collection-resource (make-fake-file-resource workspace "collection/props.collection" collection-text)]
      (doseq [resource-load-order [[script-resource game-object-resource collection-resource]
                                   [script-resource collection-resource game-object-resource]
                                   [game-object-resource script-resource collection-resource]
                                   [game-object-resource collection-resource script-resource]
                                   [collection-resource script-resource game-object-resource]
                                   [collection-resource game-object-resource script-resource]]]
        (testing (apply format "Load order %s %s %s" (map resource/resource->proj-path resource-load-order))
          (let [project (tu/setup-project! workspace resource-load-order)
                nodes-by-resource-path (g/node-value project :nodes-by-resource-path)
                collection (nodes-by-resource-path (resource/proj-path collection-resource))
                script-node-outline (tu/outline collection [0 0])
                script-component (:node-id script-node-outline)]
            (is (= "script" (:label script-node-outline)))
            (doseq [[prop-name prop-value] {"atlas" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.atlas")
                                            "bool" true
                                            "hash" "hash3"
                                            "material" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.material")
                                            "number" 3.0
                                            "quat" [180.0 0.0, 0.0]
                                            "texture" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_collection.png")
                                            "url" "/url2"
                                            "vec3" [1.0 2.0 3.0]
                                            "vec4" [1.0 2.0 3.0 4.0]}]
              (is (= prop-value (prop script-component prop-name))))))))))

(defn- build-resource [project path]
  (when-some [resource-node (tu/resource-node project path)]
    (:resource (first (g/node-value resource-node :build-targets)))))

(defn- texture-build-resource [project path]
  (when-some [resource-node (tu/resource-node project path)]
    (:resource (first (:deps (first (g/node-value resource-node :build-targets)))))))

(defn- build-output [project path]
  (with-open [in (io/input-stream (build-resource project path))
              out (ByteArrayOutputStream.)]
    (io/copy in out)
    (.toByteArray out)))

(defn- save-value [pb-class node-id]
  (with-open [reader (StringReader. (:content (g/node-value node-id :save-data)))]
    (protobuf/read-text pb-class reader)))

(defn- properties [node-id]
  (:properties (g/node-value node-id :_properties)))

(defn- built-resources [node-id]
  (into #{}
        (comp (remove sequential?)
              (keep :resource))
        (tree-seq #(or (sequential? %) (seq (:deps %)))
                  #(if (sequential? %) (seq %) (:deps %))
                  (g/node-value node-id :build-targets))))

(defn- resource-property? [ext property value]
  (and (is (= :property-type-hash (:go-prop-type property)))
       (is (= value (:value property)))
       (is (= resource/Resource (:type (:edit-type property))))
       (is (= ext (:ext (:edit-type property))))))

(def ^:private atlas-resource-property? (partial resource-property? (script/resource-kind->ext "atlas")))
(def ^:private material-resource-property? (partial resource-property? (script/resource-kind->ext "material")))
(def ^:private texture-resource-property? (partial resource-property? (script/resource-kind->ext "texture")))

(deftest resource-script-properties-test
  (tu/with-loaded-project
    (let [resource (partial tu/resource workspace)
          resource-node (partial tu/resource-node project)
          build-resource (partial build-resource project)]

      (let [props-script (resource-node "/script/props.script")
            properties (properties props-script)]
        (is (atlas-resource-property?    (:__atlas properties)    (resource "/script/resources/from_props_script.atlas")))
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_props_script.material")))
        (is (texture-resource-property?  (:__texture properties)  (resource "/script/resources/from_props_script.png")))
        (is (set/superset? (built-resources props-script)
                           #{(build-resource "/script/resources/from_props_script.material")
                             (build-resource "/script/resources/from_props_script.png")
                             (build-resource "/script/resources/from_props_script.atlas")})))

      (let [props-game-object (resource-node "/game_object/props.go")
            props-script-component (:node-id (tu/outline props-game-object [0]))
            properties (properties props-script-component)]
        (is (atlas-resource-property?    (:__atlas properties)    (resource "/script/resources/from_props_game_object.atlas")))
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_props_game_object.material")))
        (is (texture-resource-property?  (:__texture properties)  (resource "/script/resources/from_props_game_object.png")))
        (is (set/superset? (built-resources props-game-object)
                           #{(build-resource "/script/resources/from_props_game_object.material")
                             (build-resource "/script/resources/from_props_game_object.png")
                             (build-resource "/script/resources/from_props_game_object.atlas")})))

      (let [props-collection (resource-node "/collection/props.collection")
            props-script-component (:node-id (tu/outline props-collection [0 0]))
            properties (properties props-script-component)]
        (is (atlas-resource-property?    (:__atlas properties)    (resource "/script/resources/from_props_game_object.atlas")))
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_props_game_object.material")))
        (is (texture-resource-property?  (:__texture properties)  (resource "/script/resources/from_props_collection.png")))
        (is (set/superset? (built-resources props-collection)
                           #{(build-resource "/script/resources/from_props_collection.png")})))

      (let [sub-props-collection (resource-node "/collection/sub_props.collection")
            props-script-component (:node-id (tu/outline sub-props-collection [0 0 0]))
            properties (properties props-script-component)]
        (is (atlas-resource-property?    (:__atlas properties)    (resource "/script/resources/from_sub_props_collection.atlas")))
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_props_game_object.material")))
        (is (texture-resource-property?  (:__texture properties)  (resource "/script/resources/from_props_collection.png")))
        (is (set/superset? (built-resources sub-props-collection)
                           #{(build-resource "/script/resources/from_sub_props_collection.atlas")})))

      (let [sub-sub-props-collection (resource-node "/collection/sub_sub_props.collection")
            props-script-component (:node-id (tu/outline sub-sub-props-collection [0 0 0 0]))
            properties (properties props-script-component)]
        (is (atlas-resource-property?    (:__atlas properties)    (resource "/script/resources/from_sub_props_collection.atlas")))
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_sub_sub_props_collection.material")))
        (is (texture-resource-property?  (:__texture properties)  (resource "/script/resources/from_props_collection.png")))
        (is (set/superset? (built-resources sub-sub-props-collection)
                           #{(build-resource "/script/resources/from_sub_sub_props_collection.material")}))))))

(defn- created-node [select-fn-call-logger]
  (let [calls (tu/call-logger-calls select-fn-call-logger)
        args (last calls)
        selection (first args)
        node-id (first selection)]
    node-id))

(defn- add-component! [game-object component]
  (let [select-fn (tu/make-call-logger)]
    (game-object/add-component-file game-object (g/node-value component :resource) select-fn)
    (created-node select-fn)))

(defn- add-game-object! [collection game-object]
  (let [select-fn (tu/make-call-logger)]
    (collection/add-game-object-file collection collection (g/node-value game-object :resource) select-fn)
    (created-node select-fn)))

(defn- add-collection! [collection instantiated-collection]
  (let [resource (g/node-value instantiated-collection :resource)
        id (resource/base-name resource)
        position [0.0 0.0 0.0]
        rotation [0.0 0.0 0.0 1.0]
        scale [1.0 1.0 1.0]
        overrides []]
    (first
      (g/tx-nodes-added
        (g/transact
          (collection/add-collection-instance collection resource id position rotation scale overrides))))))

(defn- make-atlas! [project proj-path]
  (assert (string/ends-with? proj-path ".atlas"))
  (let [workspace (project/workspace project)
        image-resource (tu/make-png-resource! workspace (string/replace proj-path #".atlas$" ".png"))
        atlas (tu/make-resource-node! project proj-path)]
    (g/transact
      (atlas/add-images atlas [image-resource]))
    atlas))

(defn- make-material! [project proj-path]
  (assert (string/ends-with? proj-path ".material"))
  (let [workspace (project/workspace project)
        vertex-program-resource (tu/make-resource! workspace (string/replace proj-path #".material" ".vp"))
        fragment-program-resource (tu/make-resource! workspace (string/replace proj-path #".material" ".fp"))
        material (tu/make-resource-node! project proj-path)]
    (doto material
      (g/set-property!
        :vertex-program vertex-program-resource
        :fragment-program fragment-program-resource))))

(defn- build-node! [resource-node]
  (let [project (project/get-project resource-node)
        workspace (project/workspace project)
        evaluation-context (g/make-evaluation-context)
        old-artifact-map (workspace/artifact-map workspace)
        build-path (workspace/build-path workspace)
        build-result (project/build! project resource-node evaluation-context nil old-artifact-map progress/null-render-progress!)]
    (g/update-cache-from-evaluation-context! evaluation-context)
    [build-path build-result]))

(defn- build! [resource-node]
  (let [[build-path] (build-node! resource-node)]
    (tu/make-directory-deleter build-path)))

(defn- build-error! [resource-node]
  (let [[build-path build-result] (build-node! resource-node)
        error (:error build-result)]
    (fs/delete-directory! (io/file build-path) {:fail :silently})
    error))

(defn- coalesced-property [node-id prop-kw]
  (get-in (properties/coalesce [(g/node-value node-id :_properties)])
          [:properties prop-kw]))

(defn- edit-property! [node-id prop-kw value]
  (properties/set-values! (coalesced-property node-id prop-kw) [value]))

(defn- reset-property! [node-id prop-kw]
  (properties/clear-override! (coalesced-property node-id prop-kw)))

(defn- find-corresponding [items node-with-id-property]
  (assert (vector? items))
  (let [wanted-id (g/node-value node-with-id-property :id)]
    (some (fn [{:keys [id] :as item}]
            (let [item-id (if (string/starts-with? id collection/path-sep) (subs id 1) id)]
              (when (= wanted-id item-id)
                item)))
          items)))

(def ^:private error-item-open-info-without-opts (comp pop build-errors-view/error-item-open-info))

(deftest edit-script-resource-properties-test
  (with-clean-system
    (let [workspace (tu/setup-scratch-workspace! world "test/resources/empty_project")
          project (tu/setup-project! workspace)
          project-graph (g/node-id->graph-id project)
          resource (partial tu/resource workspace)
          build-resource (partial build-resource project)
          build-resource-path (comp resource/proj-path build-resource)
          build-resource-path-hash (comp murmur/hash64 build-resource-path)
          texture-build-resource (partial texture-build-resource project)
          build-output (partial build-output project)
          make-atlas! (partial make-atlas! project)
          make-material! (partial make-material! project)
          make-resource-node! (partial tu/make-resource-node! project)]
      (with-open [_ (tu/make-directory-deleter (workspace/project-path workspace))]
        (make-atlas!    "/from-props-script.atlas")
        (make-material! "/from-props-script.material")
        (let [props-script (doto (make-resource-node! "/props.script")
                             (g/set-property! :lines ["go.property('atlas',       resource.atlas('/from-props-script.atlas'))"
                                                      "go.property('material', resource.material('/from-props-script.material'))"
                                                      "go.property('texture',   resource.texture('/from-props-script.png'))"]))]
          (is (g/node-instance? script/ScriptNode props-script))
          (is (not (g/override? props-script)))

          (testing "Script defaults"
            (let [properties (properties props-script)]
              (is (atlas-resource-property?    (:__atlas properties)    (resource "/from-props-script.atlas")))
              (is (material-resource-property? (:__material properties) (resource "/from-props-script.material")))
              (is (texture-resource-property?  (:__texture properties)  (resource "/from-props-script.png")))
              (is (= (built-resources props-script)
                     #{(build-resource         "/props.script")
                       (build-resource         "/from-props-script.atlas")
                       (texture-build-resource "/from-props-script.atlas")
                       (build-resource         "/from-props-script.material")
                       (build-resource         "/from-props-script.png")
                       (build-resource         "/from-props-script.fp")
                       (build-resource         "/from-props-script.vp")}))
              (with-open [_ (build! props-script)]
                (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                  (is (= (unpack-property-declarations (:properties built-props-script))
                         {"atlas"    (build-resource-path-hash "/from-props-script.atlas")
                          "material" (build-resource-path-hash "/from-props-script.material")
                          "texture"  (build-resource-path-hash "/from-props-script.png")}))
                  (is (= (sort (:property-resources built-props-script))
                         (sort [(build-resource-path "/from-props-script.atlas")
                                (build-resource-path "/from-props-script.material")
                                (build-resource-path "/from-props-script.png")]))))))

            (testing "Editing the script code affects exposed properties"
              (with-open [_ (tu/make-graph-reverter project-graph)]
                (g/set-property! props-script :lines ["go.property('other', resource.texture('/from-props-script.png'))"])
                (let [properties (properties props-script)]
                  (is (not (contains? properties :__atlas)))
                  (is (not (contains? properties :__material)))
                  (is (not (contains? properties :__texture)))
                  (is (texture-resource-property? (:__other properties) (resource "/from-props-script.png"))))
                (is (= (built-resources props-script)
                       #{(build-resource "/props.script")
                         (build-resource "/from-props-script.png")}))
                (with-open [_ (build! props-script)]
                  (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                    (is (= (unpack-property-declarations (:properties built-props-script))
                           {"other" (build-resource-path-hash "/from-props-script.png")}))
                    (is (= (:property-resources built-props-script)
                           [(build-resource-path "/from-props-script.png")]))))))

            (testing "Missing resource error"
              (with-open [_ (tu/make-graph-reverter project-graph)]
                (g/set-property! props-script :lines ["go.property('texture', resource.texture('/missing-resource.png'))"])
                (let [properties (properties props-script)
                      error-value (tu/prop-error props-script :__texture)]
                  (is (texture-resource-property? (:__texture properties) (resource "/missing-resource.png")))
                  (is (g/error? error-value))
                  (is (= "Texture '/missing-resource.png' could not be found" (:message error-value))))
                (let [error-value (build-error! props-script)]
                  (when (is (g/error? error-value))
                    (let [error-tree (build-errors-view/build-resource-tree error-value)
                          error-item-of-parent-resource (first (:children error-tree))
                          error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                      (is (= "Texture '/missing-resource.png' could not be found" (:message error-item-of-faulty-node)))
                      (is (= [(resource "/props.script") props-script]
                             (error-item-open-info-without-opts error-item-of-parent-resource)))
                      (is (= [(resource "/props.script") props-script]
                             (error-item-open-info-without-opts error-item-of-faulty-node))))))))

            (testing "Unsupported resource error"
              (with-open [_ (tu/make-graph-reverter project-graph)]
                (g/set-property! props-script :lines ["go.property('texture', resource.texture('/unsupported-resource.txt'))"])
                (let [properties (properties props-script)
                      error-value (tu/prop-error props-script :__texture)]
                  (is (texture-resource-property? (:__texture properties) (resource "/unsupported-resource.txt")))
                  (is (g/error? error-value))
                  (is (= "Texture '/unsupported-resource.txt' is not of type .cubemap, .jpg or .png" (:message error-value))))
                (let [error-value (build-error! props-script)]
                  (when (is (g/error? error-value))
                    (let [error-tree (build-errors-view/build-resource-tree error-value)
                          error-item-of-parent-resource (first (:children error-tree))
                          error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                      (is (= "Texture '/unsupported-resource.txt' is not of type .cubemap, .jpg or .png" (:message error-item-of-faulty-node)))
                      (is (= [(resource "/props.script") props-script]
                             (error-item-open-info-without-opts error-item-of-parent-resource)))
                      (is (= [(resource "/props.script") props-script]
                             (error-item-open-info-without-opts error-item-of-faulty-node))))))))))))))

(deftest edit-component-instance-resource-properties-test
  (with-clean-system
    (let [workspace (tu/setup-scratch-workspace! world "test/resources/empty_project")
          project (tu/setup-project! workspace)
          project-graph (g/node-id->graph-id project)
          resource (partial tu/resource workspace)
          build-resource (partial build-resource project)
          build-resource-path (comp resource/proj-path build-resource)
          build-resource-path-hash (comp murmur/hash64 build-resource-path)
          build-output (partial build-output project)
          texture-build-resource (partial texture-build-resource project)
          make-atlas! (partial make-atlas! project)
          make-material! (partial make-material! project)
          make-resource-node! (partial tu/make-resource-node! project)]
      (with-open [_ (tu/make-directory-deleter (workspace/project-path workspace))]
        (make-atlas!    "/from-props-script.atlas")
        (make-material! "/from-props-script.material")
        (make-atlas!    "/from-props-game-object.atlas")
        (make-material! "/from-props-game-object.material")
        (let [props-script (doto (make-resource-node! "/props.script")
                             (g/set-property! :lines ["go.property('atlas',       resource.atlas('/from-props-script.atlas'))"
                                                      "go.property('material', resource.material('/from-props-script.material'))"
                                                      "go.property('texture',   resource.texture('/from-props-script.png'))"]))
              props-game-object (make-resource-node! "/props.go")
              props-script-component (add-component! props-game-object props-script)
              original-property-values (into {}
                                             (map (fn [[prop-kw {:keys [value]}]]
                                                    [prop-kw value]))
                                             (properties props-script))]
          (is (g/node-instance? script/ScriptNode props-script))
          (is (g/node-instance? game-object/ReferencedComponent props-script-component))
          (is (g/node-instance? game-object/GameObjectNode props-game-object))
          (is (not (g/override? props-script)))
          (is (not (g/override? props-script-component)))
          (is (not (g/override? props-game-object)))

          (testing "Before overrides"
            (let [saved-props-game-object (save-value GameObject$PrototypeDesc props-game-object)
                  saved-props-script-component (find-corresponding (:components saved-props-game-object) props-script-component)]
              (is (= [] (:properties saved-props-script-component))))

            (let [properties (properties props-script-component)]
              (is (atlas-resource-property?    (:__atlas properties)    (resource "/from-props-script.atlas")))
              (is (material-resource-property? (:__material properties) (resource "/from-props-script.material")))
              (is (texture-resource-property?  (:__texture properties)  (resource "/from-props-script.png")))
              (is (not (tu/prop-overridden? props-script-component :__atlas)))
              (is (not (tu/prop-overridden? props-script-component :__material)))
              (is (not (tu/prop-overridden? props-script-component :__texture)))
              (is (= (built-resources props-game-object)
                     #{(build-resource         "/props.go")
                       (build-resource         "/props.script")
                       (build-resource         "/from-props-script.atlas")
                       (texture-build-resource "/from-props-script.atlas")
                       (build-resource         "/from-props-script.material")
                       (build-resource         "/from-props-script.png")
                       (build-resource         "/from-props-script.fp")
                       (build-resource         "/from-props-script.vp")}))
              (with-open [_ (build! props-game-object)]
                (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))
                      built-props-script-component (find-corresponding (:components built-props-game-object) props-script-component)]
                  (is (= {} (unpack-property-declarations (:property-decls built-props-script-component))))
                  (is (= [] (:property-resources built-props-game-object)))))))

          (testing "Overrides do not affect props script"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (edit-property! props-script-component :__atlas    (resource "/from-props-game-object.atlas"))
              (edit-property! props-script-component :__material (resource "/from-props-game-object.material"))
              (edit-property! props-script-component :__texture  (resource "/from-props-game-object.png"))

              (let [properties (properties props-script)]
                (is (atlas-resource-property?    (:__atlas properties)    (resource "/from-props-script.atlas")))
                (is (material-resource-property? (:__material properties) (resource "/from-props-script.material")))
                (is (texture-resource-property?  (:__texture properties)  (resource "/from-props-script.png")))
                (is (= (built-resources props-script)
                       #{(build-resource         "/props.script")
                         (build-resource         "/from-props-script.atlas")
                         (texture-build-resource "/from-props-script.atlas")
                         (build-resource         "/from-props-script.material")
                         (build-resource         "/from-props-script.png")
                         (build-resource         "/from-props-script.fp")
                         (build-resource         "/from-props-script.vp")}))
                (with-open [_ (build! props-game-object)]
                  (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                    (is (= (unpack-property-declarations (:properties built-props-script))
                           {"atlas"    (build-resource-path-hash "/from-props-script.atlas")
                            "material" (build-resource-path-hash "/from-props-script.material")
                            "texture"  (build-resource-path-hash "/from-props-script.png")}))
                    (is (= (sort (:property-resources built-props-script))
                           (sort [(build-resource-path "/from-props-script.atlas")
                                  (build-resource-path "/from-props-script.material")
                                  (build-resource-path "/from-props-script.png")]))))))))

          (testing "Overrides"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (doseq [[ext prop-kw resource build-resource] [[(script/resource-kind->ext "atlas")    :__atlas    (resource "/from-props-game-object.atlas")    (build-resource "/from-props-game-object.atlas")]
                                                             [(script/resource-kind->ext "material") :__material (resource "/from-props-game-object.material") (build-resource "/from-props-game-object.material")]
                                                             [(script/resource-kind->ext "texture")  :__texture  (resource "/from-props-game-object.png")      (build-resource "/from-props-game-object.png")]]]
                ;; Apply override.
                (edit-property! props-script-component prop-kw resource)
                (is (tu/prop-overridden? props-script-component prop-kw))
                (is (resource-property? ext (get (properties props-script-component) prop-kw) resource))
                (is (contains? (built-resources props-game-object) build-resource))

                (let [saved-props-game-object (save-value GameObject$PrototypeDesc props-game-object)
                      saved-props-script-component (find-corresponding (:components saved-props-game-object) props-script-component)]
                  (is (= {} (unpack-property-declarations (:property-decls saved-props-script-component))))
                  (is (= (:properties saved-props-script-component)
                         [{:id (properties/key->user-name prop-kw)
                           :value (resource/proj-path resource)
                           :type :property-type-hash}])))

                (with-open [_ (build! props-game-object)]
                  (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))
                        built-props-script-component (find-corresponding (:components built-props-game-object) props-script-component)]
                    (is (= [] (:properties built-props-script-component)))
                    (is (= (unpack-property-declarations (:property-decls built-props-script-component))
                           {(properties/key->user-name prop-kw) (murmur/hash64 (resource/proj-path build-resource))}))
                    (is (= (:property-resources built-props-game-object)
                           [(resource/proj-path build-resource)]))))

                ;; Clear override.
                (reset-property! props-script-component prop-kw)
                (is (not (tu/prop-overridden? props-script-component prop-kw)))
                (is (resource-property? ext (get (properties props-script-component) prop-kw) (original-property-values prop-kw)))
                (is (not (contains? (built-resources props-game-object) build-resource)))

                (let [saved-props-game-object (save-value GameObject$PrototypeDesc props-game-object)
                      saved-props-script-component (find-corresponding (:components saved-props-game-object) props-script-component)]
                  (is (= [] (:properties saved-props-script-component))))

                (with-open [_ (build! props-game-object)]
                  (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))
                        built-props-script-component (find-corresponding (:components built-props-game-object) props-script-component)]
                    (is (= {} (unpack-property-declarations (:property-decls built-props-script-component))))
                    (is (= [] (:property-resources built-props-game-object))))))))

          (testing "Missing resource error"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (edit-property! props-script-component :__texture (resource "/missing-resource.png"))
              (let [properties (properties props-script-component)
                    error-value (tu/prop-error props-script-component :__texture)]
                (is (texture-resource-property? (:__texture properties) (resource "/missing-resource.png")))
                (is (g/error? error-value))
                (is (= "Texture '/missing-resource.png' could not be found" (:message error-value))))
              (let [error-value (build-error! props-game-object)]
                (when (is (g/error? error-value))
                  (let [error-tree (build-errors-view/build-resource-tree error-value)
                        error-item-of-parent-resource (first (:children error-tree))
                        error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                    (is (= "Texture '/missing-resource.png' could not be found" (:message error-item-of-faulty-node)))
                    (is (= [(resource "/props.go") props-game-object]
                           (error-item-open-info-without-opts error-item-of-parent-resource)))
                    (is (= [(resource "/props.go") props-script-component]
                           (error-item-open-info-without-opts error-item-of-faulty-node))))))

              (testing "Error goes away after clearing override"
                (reset-property! props-script-component :__texture)
                (is (not (g/error? (tu/prop-error props-script-component :__texture))))
                (is (not (g/error? (build-error! props-game-object)))))))

          (testing "Unsupported resource error"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (edit-property! props-script-component :__texture (resource "/unsupported-resource.txt"))
              (let [properties (properties props-script-component)
                    error-value (tu/prop-error props-script-component :__texture)]
                (is (texture-resource-property? (:__texture properties) (resource "/unsupported-resource.txt")))
                (is (g/error? error-value))
                (is (= "Texture '/unsupported-resource.txt' is not of type .cubemap, .jpg or .png" (:message error-value))))
              (let [error-value (build-error! props-game-object)]
                (when (is (g/error? error-value))
                  (let [error-tree (build-errors-view/build-resource-tree error-value)
                        error-item-of-parent-resource (first (:children error-tree))
                        error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                    (is (= "Texture '/unsupported-resource.txt' is not of type .cubemap, .jpg or .png" (:message error-item-of-faulty-node)))
                    (is (= [(resource "/props.go") props-game-object]
                           (error-item-open-info-without-opts error-item-of-parent-resource)))
                    (is (= [(resource "/props.go") props-script-component]
                           (error-item-open-info-without-opts error-item-of-faulty-node))))))

              (testing "Error goes away after clearing override"
                (reset-property! props-script-component :__texture)
                (is (not (g/error? (tu/prop-error props-script-component :__texture))))
                (is (not (g/error? (build-error! props-game-object))))))))))))

(deftest edit-game-object-instance-resource-properties-test
  (with-clean-system
    (let [workspace (tu/setup-scratch-workspace! world "test/resources/empty_project")
          project (tu/setup-project! workspace)
          project-graph (g/node-id->graph-id project)
          resource (partial tu/resource workspace)
          build-resource (partial build-resource project)
          build-resource-path (comp resource/proj-path build-resource)
          build-resource-path-hash (comp murmur/hash64 build-resource-path)
          build-output (partial build-output project)
          texture-build-resource (partial texture-build-resource project)
          make-atlas! (partial make-atlas! project)
          make-material! (partial make-material! project)
          make-resource-node! (partial tu/make-resource-node! project)]
      (with-open [_ (tu/make-directory-deleter (workspace/project-path workspace))]
        (make-atlas!    "/from-props-script.atlas")
        (make-material! "/from-props-script.material")
        (make-atlas!    "/from-props-game-object.atlas")
        (make-material! "/from-props-game-object.material")
        (make-atlas!    "/from-props-collection.atlas")
        (make-material! "/from-props-collection.material")
        (let [props-script (doto (make-resource-node! "/props.script")
                             (g/set-property! :lines ["go.property('atlas',       resource.atlas('/from-props-script.atlas'))"
                                                      "go.property('material', resource.material('/from-props-script.material'))"
                                                      "go.property('texture',   resource.texture('/from-props-script.png'))"]))
              props-game-object (doto (make-resource-node! "/props.go")
                                  (add-component! props-script))
              props-collection (make-resource-node! "/props.collection")
              ov-props-game-object (add-game-object! props-collection props-game-object)
              ov-props-script-component (ffirst (g/sources-of ov-props-game-object :ref-ddf))
              props-game-object-instance (ffirst (g/sources-of props-collection :ref-inst-ddf))
              original-property-values (into {}
                                             (map (fn [[prop-kw {:keys [value]}]]
                                                    [prop-kw value]))
                                             (properties props-script))]
          (is (g/node-instance? script/ScriptNode props-script))
          (is (g/node-instance? game-object/ReferencedComponent ov-props-script-component))
          (is (g/node-instance? game-object/GameObjectNode props-game-object))
          (is (g/node-instance? game-object/GameObjectNode ov-props-game-object))
          (is (g/node-instance? collection/ReferencedGOInstanceNode props-game-object-instance))
          (is (g/node-instance? collection/CollectionNode props-collection))
          (is (not (g/override? props-script)))
          (is (not (g/override? props-game-object)))
          (is (not (g/override? props-game-object-instance)))
          (is (not (g/override? props-collection)))
          (is (g/override? ov-props-script-component))
          (is (g/override? ov-props-game-object))

          (testing "Before overrides"
            (let [saved-props-collection (save-value GameObject$CollectionDesc props-collection)
                  saved-props-game-object-instance (find-corresponding (:instances saved-props-collection) props-game-object-instance)]
              (is (= [] (:component-properties saved-props-game-object-instance))))

            (let [properties (properties ov-props-script-component)]
              (is (atlas-resource-property?    (:__atlas properties)    (resource "/from-props-script.atlas")))
              (is (material-resource-property? (:__material properties) (resource "/from-props-script.material")))
              (is (texture-resource-property?  (:__texture properties)  (resource "/from-props-script.png")))
              (is (not (tu/prop-overridden? ov-props-script-component :__atlas)))
              (is (not (tu/prop-overridden? ov-props-script-component :__material)))
              (is (not (tu/prop-overridden? ov-props-script-component :__texture)))
              (is (= (built-resources props-collection)
                     #{(build-resource         "/props.collection")
                       (build-resource         "/props.go")
                       (build-resource         "/props.script")
                       (build-resource         "/from-props-script.atlas")
                       (texture-build-resource "/from-props-script.atlas")
                       (build-resource         "/from-props-script.material")
                       (build-resource         "/from-props-script.png")
                       (build-resource         "/from-props-script.fp")
                       (build-resource         "/from-props-script.vp")}))
              (with-open [_ (build! props-collection)]
                (let [built-props-collection (protobuf/bytes->map GameObject$CollectionDesc (build-output "/props.collection"))
                      built-props-game-object-instance (find-corresponding (:instances built-props-collection) props-game-object-instance)]
                  (is (= [] (:component-properties built-props-game-object-instance)))
                  (is (= [] (:property-resources built-props-collection)))))))

          (testing "Overrides do not affect props script or game object"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (edit-property! ov-props-script-component :__atlas    (resource "/from-props-collection.atlas"))
              (edit-property! ov-props-script-component :__material (resource "/from-props-collection.material"))
              (edit-property! ov-props-script-component :__texture  (resource "/from-props-collection.png"))

              (let [properties (properties props-script)]
                (is (atlas-resource-property?    (:__atlas properties)    (resource "/from-props-script.atlas")))
                (is (material-resource-property? (:__material properties) (resource "/from-props-script.material")))
                (is (texture-resource-property?  (:__texture properties)  (resource "/from-props-script.png")))
                (is (= (built-resources props-script)
                       #{(build-resource         "/props.script")
                         (build-resource         "/from-props-script.atlas")
                         (texture-build-resource "/from-props-script.atlas")
                         (build-resource         "/from-props-script.material")
                         (build-resource         "/from-props-script.png")
                         (build-resource         "/from-props-script.fp")
                         (build-resource         "/from-props-script.vp")}))
                (with-open [_ (build! props-collection)]
                  (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                    (is (= (unpack-property-declarations (:properties built-props-script))
                           {"atlas"    (build-resource-path-hash "/from-props-script.atlas")
                            "material" (build-resource-path-hash "/from-props-script.material")
                            "texture"  (build-resource-path-hash "/from-props-script.png")}))
                    (is (= (sort (:property-resources built-props-script))
                           (sort [(build-resource-path "/from-props-script.atlas")
                                  (build-resource-path "/from-props-script.material")
                                  (build-resource-path "/from-props-script.png")]))))
                  (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))
                        built-props-script-component (find-corresponding (:components built-props-game-object) ov-props-script-component)]
                    (is (= {} (unpack-property-declarations (:property-decls built-props-script-component))))
                    (is (= [] (:property-resources built-props-game-object))))))))

          (testing "Overrides"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (doseq [[ext prop-kw resource build-resource] [[(script/resource-kind->ext "atlas")    :__atlas    (resource "/from-props-collection.atlas")    (build-resource "/from-props-collection.atlas")]
                                                             [(script/resource-kind->ext "material") :__material (resource "/from-props-collection.material") (build-resource "/from-props-collection.material")]
                                                             [(script/resource-kind->ext "texture")  :__texture  (resource "/from-props-collection.png")      (build-resource "/from-props-collection.png")]]]
                ;; Apply override.
                (edit-property! ov-props-script-component prop-kw resource)
                (is (tu/prop-overridden? ov-props-script-component prop-kw))
                (is (resource-property? ext (get (properties ov-props-script-component) prop-kw) resource))
                (is (contains? (built-resources props-collection) build-resource))

                (let [saved-props-collection (save-value GameObject$CollectionDesc props-collection)
                      saved-props-game-object-instance (find-corresponding (:instances saved-props-collection) props-game-object-instance)
                      saved-props-script-component (find-corresponding (:component-properties saved-props-game-object-instance) ov-props-script-component)]
                  (is (= {} (unpack-property-declarations (:property-decls saved-props-script-component))))
                  (is (= (:properties saved-props-script-component)
                         [{:id (properties/key->user-name prop-kw)
                           :value (resource/proj-path resource)
                           :type :property-type-hash}])))

                (with-open [_ (build! props-collection)]
                  (let [built-props-collection (protobuf/bytes->map GameObject$CollectionDesc (build-output "/props.collection"))
                        built-props-game-object-instance (find-corresponding (:instances built-props-collection) props-game-object-instance)
                        built-props-script-component (find-corresponding (:component-properties built-props-game-object-instance) ov-props-script-component)]
                    (is (= [] (:properties built-props-script-component)))
                    (is (= (unpack-property-declarations (:property-decls built-props-script-component))
                           {(properties/key->user-name prop-kw) (murmur/hash64 (resource/proj-path build-resource))}))
                    (is (= (:property-resources built-props-collection)
                           [(resource/proj-path build-resource)]))))

                ;; Clear override.
                (reset-property! ov-props-script-component prop-kw)
                (is (not (tu/prop-overridden? ov-props-script-component prop-kw)))
                (is (resource-property? ext (get (properties ov-props-script-component) prop-kw) (original-property-values prop-kw)))
                (is (not (contains? (built-resources props-collection) build-resource)))

                (let [saved-props-collection (save-value GameObject$CollectionDesc props-collection)
                      saved-props-game-object-instance (find-corresponding (:instances saved-props-collection) props-game-object-instance)]
                  (is (= [] (:component-properties saved-props-game-object-instance))))

                (with-open [_ (build! props-collection)]
                  (let [built-props-collection (protobuf/bytes->map GameObject$CollectionDesc (build-output "/props.collection"))
                        built-props-game-object-instance (find-corresponding (:instances built-props-collection) props-game-object-instance)]
                    (is (= [] (:component-properties built-props-game-object-instance)))
                    (is (= [] (:property-resources built-props-collection))))))))

          (testing "Missing resource error"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (edit-property! ov-props-script-component :__texture (resource "/missing-resource.png"))
              (let [properties (properties ov-props-script-component)
                    error-value (tu/prop-error ov-props-script-component :__texture)]
                (is (texture-resource-property? (:__texture properties) (resource "/missing-resource.png")))
                (is (g/error? error-value))
                (is (= "Texture '/missing-resource.png' could not be found" (:message error-value))))
              (let [error-value (build-error! props-collection)]
                (when (is (g/error? error-value))
                  (let [error-tree (build-errors-view/build-resource-tree error-value)
                        error-item-of-parent-resource (first (:children error-tree))
                        error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                    (is (= "Texture '/missing-resource.png' could not be found" (:message error-item-of-faulty-node)))
                    (is (= [(resource "/props.collection") props-collection]
                           (error-item-open-info-without-opts error-item-of-parent-resource)))
                    (is (= [(resource "/props.collection") ov-props-script-component]
                           (error-item-open-info-without-opts error-item-of-faulty-node))))))

              (testing "Error goes away after clearing override"
                (reset-property! ov-props-script-component :__texture)
                (is (not (g/error? (tu/prop-error ov-props-script-component :__texture))))
                (is (not (g/error? (build-error! props-collection)))))))

          (testing "Unsupported resource error"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (edit-property! ov-props-script-component :__texture (resource "/unsupported-resource.txt"))
              (let [properties (properties ov-props-script-component)
                    error-value (tu/prop-error ov-props-script-component :__texture)]
                (is (texture-resource-property? (:__texture properties) (resource "/unsupported-resource.txt")))
                (is (g/error? error-value))
                (is (= "Texture '/unsupported-resource.txt' is not of type .cubemap, .jpg or .png" (:message error-value))))
              (let [error-value (build-error! props-collection)]
                (when (is (g/error? error-value))
                  (let [error-tree (build-errors-view/build-resource-tree error-value)
                        error-item-of-parent-resource (first (:children error-tree))
                        error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                    (is (= "Texture '/unsupported-resource.txt' is not of type .cubemap, .jpg or .png" (:message error-item-of-faulty-node)))
                    (is (= [(resource "/props.collection") props-collection]
                           (error-item-open-info-without-opts error-item-of-parent-resource)))
                    (is (= [(resource "/props.collection") ov-props-script-component]
                           (error-item-open-info-without-opts error-item-of-faulty-node))))))

              (testing "Error goes away after clearing override"
                (reset-property! ov-props-script-component :__texture)
                (is (not (g/error? (tu/prop-error ov-props-script-component :__texture))))
                (is (not (g/error? (build-error! props-collection))))))))))))

(deftest edit-collection-instance-resource-properties-test
  (with-clean-system
    (let [workspace (tu/setup-scratch-workspace! world "test/resources/empty_project")
          project (tu/setup-project! workspace)
          project-graph (g/node-id->graph-id project)
          resource (partial tu/resource workspace)
          build-resource (partial build-resource project)
          build-resource-path (comp resource/proj-path build-resource)
          build-resource-path-hash (comp murmur/hash64 build-resource-path)
          build-output (partial build-output project)
          texture-build-resource (partial texture-build-resource project)
          make-atlas! (partial make-atlas! project)
          make-material! (partial make-material! project)
          make-resource-node! (partial tu/make-resource-node! project)]
      (with-open [_ (tu/make-directory-deleter (workspace/project-path workspace))]
        (make-atlas!    "/from-props-script.atlas")
        (make-material! "/from-props-script.material")
        (make-atlas!    "/from-props-game-object.atlas")
        (make-material! "/from-props-game-object.material")
        (make-atlas!    "/from-props-collection.atlas")
        (make-material! "/from-props-collection.material")
        (make-atlas!    "/from-sub-props-collection.atlas")
        (make-material! "/from-sub-props-collection.material")
        (let [props-script (doto (make-resource-node! "/props.script")
                             (g/set-property! :lines ["go.property('atlas',       resource.atlas('/from-props-script.atlas'))"
                                                      "go.property('material', resource.material('/from-props-script.material'))"
                                                      "go.property('texture',   resource.texture('/from-props-script.png'))"]))
              props-game-object (doto (make-resource-node! "/props.go")
                                  (add-component! props-script))
              props-collection (doto (make-resource-node! "/props.collection")
                                 (add-game-object! props-game-object))
              sub-props-collection (make-resource-node! "/sub-props.collection")
              ov-props-collection (add-collection! sub-props-collection props-collection)
              ov-props-game-object-instance (ffirst (g/sources-of ov-props-collection :ref-inst-ddf))
              ov-props-game-object (ffirst (g/sources-of ov-props-game-object-instance :source-resource))
              ov-props-script-component (ffirst (g/sources-of ov-props-game-object :ref-ddf))
              props-collection-instance (ffirst (g/sources-of sub-props-collection :ref-coll-ddf))
              original-property-values (into {}
                                             (map (fn [[prop-kw {:keys [value]}]]
                                                    [prop-kw value]))
                                             (properties props-script))]
          (is (g/node-instance? script/ScriptNode props-script))
          (is (g/node-instance? game-object/ReferencedComponent ov-props-script-component))
          (is (g/node-instance? game-object/GameObjectNode props-game-object))
          (is (g/node-instance? game-object/GameObjectNode ov-props-game-object))
          (is (g/node-instance? collection/ReferencedGOInstanceNode ov-props-game-object-instance))
          (is (g/node-instance? collection/CollectionNode ov-props-collection))
          (is (g/node-instance? collection/CollectionNode props-collection))
          (is (g/node-instance? collection/CollectionNode sub-props-collection))
          (is (g/node-instance? collection/CollectionInstanceNode props-collection-instance))
          (is (not (g/override? props-script)))
          (is (not (g/override? props-game-object)))
          (is (not (g/override? props-collection)))
          (is (not (g/override? sub-props-collection)))
          (is (not (g/override? props-collection-instance)))
          (is (g/override? ov-props-script-component))
          (is (g/override? ov-props-game-object))
          (is (g/override? ov-props-game-object-instance))
          (is (g/override? ov-props-collection))

          (testing "Before overrides"
            (let [saved-sub-props-collection (save-value GameObject$CollectionDesc sub-props-collection)
                  saved-props-collection-instance (find-corresponding (:collection-instances saved-sub-props-collection) props-collection-instance)]
              (is (= [] (:instance-properties saved-props-collection-instance))))

            (let [properties (properties ov-props-script-component)]
              (is (atlas-resource-property?    (:__atlas properties)    (resource "/from-props-script.atlas")))
              (is (material-resource-property? (:__material properties) (resource "/from-props-script.material")))
              (is (texture-resource-property?  (:__texture properties)  (resource "/from-props-script.png")))
              (is (not (tu/prop-overridden? ov-props-script-component :__atlas)))
              (is (not (tu/prop-overridden? ov-props-script-component :__material)))
              (is (not (tu/prop-overridden? ov-props-script-component :__texture)))
              (is (= (built-resources sub-props-collection)
                     #{(build-resource         "/sub-props.collection")
                       (build-resource         "/props.go")
                       (build-resource         "/props.script")
                       (build-resource         "/from-props-script.atlas")
                       (texture-build-resource "/from-props-script.atlas")
                       (build-resource         "/from-props-script.material")
                       (build-resource         "/from-props-script.png")
                       (build-resource         "/from-props-script.fp")
                       (build-resource         "/from-props-script.vp")}))
              (with-open [_ (build! sub-props-collection)]
                (let [built-sub-props-collection (protobuf/bytes->map GameObject$CollectionDesc (build-output "/sub-props.collection"))
                      built-props-game-object-instance (first (:instances built-sub-props-collection))]
                  (is (= [] (:collection-instances built-sub-props-collection)))
                  (is (= [] (:embedded-instances built-sub-props-collection)))
                  (is (= [] (:component-properties built-props-game-object-instance)))
                  (is (= [] (:property-resources built-sub-props-collection)))))))

          (testing "Overrides do not affect props script or game object"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (edit-property! ov-props-script-component :__atlas    (resource "/from-sub-props-collection.atlas"))
              (edit-property! ov-props-script-component :__material (resource "/from-sub-props-collection.material"))
              (edit-property! ov-props-script-component :__texture  (resource "/from-sub-props-collection.png"))

              (let [properties (properties props-script)]
                (is (atlas-resource-property?    (:__atlas properties)    (resource "/from-props-script.atlas")))
                (is (material-resource-property? (:__material properties) (resource "/from-props-script.material")))
                (is (texture-resource-property?  (:__texture properties)  (resource "/from-props-script.png")))
                (is (= (built-resources props-script)
                       #{(build-resource         "/props.script")
                         (build-resource         "/from-props-script.atlas")
                         (texture-build-resource "/from-props-script.atlas")
                         (build-resource         "/from-props-script.material")
                         (build-resource         "/from-props-script.png")
                         (build-resource         "/from-props-script.fp")
                         (build-resource         "/from-props-script.vp")}))
                (with-open [_ (build! sub-props-collection)]
                  (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                    (is (= (unpack-property-declarations (:properties built-props-script))
                           {"atlas"    (build-resource-path-hash "/from-props-script.atlas")
                            "material" (build-resource-path-hash "/from-props-script.material")
                            "texture"  (build-resource-path-hash "/from-props-script.png")}))
                    (is (= (sort (:property-resources built-props-script))
                           (sort [(build-resource-path "/from-props-script.atlas")
                                  (build-resource-path "/from-props-script.material")
                                  (build-resource-path "/from-props-script.png")]))))
                  (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))
                        built-props-script-component (find-corresponding (:components built-props-game-object) ov-props-script-component)]
                    (is (= {} (unpack-property-declarations (:property-decls built-props-script-component))))
                    (is (= [] (:property-resources built-props-game-object))))))))

          (testing "Overrides"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (doseq [[ext prop-kw resource build-resource] [[(script/resource-kind->ext "atlas")    :__atlas    (resource "/from-sub-props-collection.atlas")    (build-resource "/from-sub-props-collection.atlas")]
                                                             [(script/resource-kind->ext "material") :__material (resource "/from-sub-props-collection.material") (build-resource "/from-sub-props-collection.material")]
                                                             [(script/resource-kind->ext "texture")  :__texture  (resource "/from-sub-props-collection.png")      (build-resource "/from-sub-props-collection.png")]]]
                ;; Apply override.
                (edit-property! ov-props-script-component prop-kw resource)
                (is (tu/prop-overridden? ov-props-script-component prop-kw))
                (is (resource-property? ext (get (properties ov-props-script-component) prop-kw) resource))
                (is (contains? (built-resources sub-props-collection) build-resource))

                (let [saved-sub-props-collection (save-value GameObject$CollectionDesc sub-props-collection)
                      saved-props-collection-instance (find-corresponding (:collection-instances saved-sub-props-collection) props-collection-instance)
                      saved-props-game-object-instance (find-corresponding (:instance-properties saved-props-collection-instance) ov-props-game-object-instance)
                      saved-props-script-component (find-corresponding (:properties saved-props-game-object-instance) ov-props-script-component)]
                  (is (= {} (unpack-property-declarations (:property-decls saved-props-script-component))))
                  (is (= (:properties saved-props-script-component)
                         [{:id (properties/key->user-name prop-kw)
                           :value (resource/proj-path resource)
                           :type :property-type-hash}])))

                (with-open [_ (build! sub-props-collection)]
                  (let [built-sub-props-collection (protobuf/bytes->map GameObject$CollectionDesc (build-output "/sub-props.collection"))
                        built-props-game-object-instance (first (:instances built-sub-props-collection))
                        built-props-script-component (find-corresponding (:component-properties built-props-game-object-instance) ov-props-script-component)]
                    (is (= [] (:collection-instances built-sub-props-collection)))
                    (is (= [] (:embedded-instances built-sub-props-collection)))
                    (is (= [] (:properties built-props-script-component)))
                    (is (= (unpack-property-declarations (:property-decls built-props-script-component))
                           {(properties/key->user-name prop-kw) (murmur/hash64 (resource/proj-path build-resource))}))
                    (is (= (:property-resources built-sub-props-collection)
                           [(resource/proj-path build-resource)]))))

                ;; Clear override.
                (reset-property! ov-props-script-component prop-kw)
                (is (not (tu/prop-overridden? ov-props-script-component prop-kw)))
                (is (resource-property? ext (get (properties ov-props-script-component) prop-kw) (original-property-values prop-kw)))
                (is (not (contains? (built-resources sub-props-collection) build-resource)))

                (let [saved-sub-props-collection (save-value GameObject$CollectionDesc sub-props-collection)
                      saved-props-collection-instance (find-corresponding (:collection-instances saved-sub-props-collection) props-collection-instance)]
                  (is (= [] (:instance-properties saved-props-collection-instance))))

                (with-open [_ (build! sub-props-collection)]
                  (let [built-sub-props-collection (protobuf/bytes->map GameObject$CollectionDesc (build-output "/sub-props.collection"))
                        built-props-game-object-instance (first (:instances built-sub-props-collection))]
                    (is (= [] (:component-properties built-props-game-object-instance)))
                    (is (= [] (:property-resources built-sub-props-collection))))))))

          (testing "Missing resource error"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (edit-property! ov-props-script-component :__texture (resource "/missing-resource.png"))
              (let [properties (properties ov-props-script-component)
                    error-value (tu/prop-error ov-props-script-component :__texture)]
                (is (texture-resource-property? (:__texture properties) (resource "/missing-resource.png")))
                (is (g/error? error-value))
                (is (= "Texture '/missing-resource.png' could not be found" (:message error-value))))
              (let [error-value (build-error! sub-props-collection)]
                (when (is (g/error? error-value))
                  (let [error-tree (build-errors-view/build-resource-tree error-value)
                        error-item-of-parent-resource (first (:children error-tree))
                        error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                    (is (= "Texture '/missing-resource.png' could not be found" (:message error-item-of-faulty-node)))
                    (is (= [(resource "/sub-props.collection") sub-props-collection]
                           (error-item-open-info-without-opts error-item-of-parent-resource)))
                    (is (= [(resource "/sub-props.collection") ov-props-script-component]
                           (error-item-open-info-without-opts error-item-of-faulty-node)))))))

            (testing "Error goes away after clearing override"
              (reset-property! ov-props-script-component :__texture)
              (is (not (g/error? (tu/prop-error ov-props-script-component :__texture))))
              (is (not (g/error? (build-error! sub-props-collection))))))

          (testing "Unsupported resource error"
            (with-open [_ (tu/make-graph-reverter project-graph)]
              (edit-property! ov-props-script-component :__texture (resource "/unsupported-resource.txt"))
              (let [properties (properties ov-props-script-component)
                    error-value (tu/prop-error ov-props-script-component :__texture)]
                (is (texture-resource-property? (:__texture properties) (resource "/unsupported-resource.txt")))
                (is (g/error? error-value))
                (is (= "Texture '/unsupported-resource.txt' is not of type .cubemap, .jpg or .png" (:message error-value))))
              (let [error-value (build-error! sub-props-collection)]
                (when (is (g/error? error-value))
                  (let [error-tree (build-errors-view/build-resource-tree error-value)
                        error-item-of-parent-resource (first (:children error-tree))
                        error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                    (is (= "Texture '/unsupported-resource.txt' is not of type .cubemap, .jpg or .png" (:message error-item-of-faulty-node)))
                    (is (= [(resource "/sub-props.collection") sub-props-collection]
                           (error-item-open-info-without-opts error-item-of-parent-resource)))
                    (is (= [(resource "/sub-props.collection") ov-props-script-component]
                           (error-item-open-info-without-opts error-item-of-faulty-node))))))

              (testing "Error goes away after clearing override"
                (reset-property! ov-props-script-component :__texture)
                (is (not (g/error? (tu/prop-error ov-props-script-component :__texture))))
                (is (not (g/error? (build-error! sub-props-collection))))))))))))
