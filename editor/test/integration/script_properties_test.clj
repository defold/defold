(ns integration.script-properties-test
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.defold-project :as project]
            [editor.game-object :as game-object]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as tu]
            [support.test-support :refer [with-clean-system]])
  (:import (java.io ByteArrayOutputStream)
           (com.dynamo.gameobject.proto GameObject$PrototypeDesc)
           (com.dynamo.lua.proto Lua$LuaModule)))

(defn- unpack-property-declarations [property-declarations]
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

(defn- perform-script-properties-source-test! []
  (tu/with-loaded-project
    (let [script-id (tu/resource-node project "/script/props.script")]
      (testing "reading values"
               (is (= 1.0 (prop script-id "number")))
               (is (read-only? script-id "number")))
      (testing "broken prop defs" ;; string vals are not supported
               (with-source script-id "go.property(\"number\", \"my_string\")\n"
                 (is (nil? (prop script-id "number"))))))))

(deftest script-properties-source
  (with-bindings {#'tu/use-new-code-editor? false}
    (perform-script-properties-source-test!))
  (with-bindings {#'tu/use-new-code-editor? true}
    (perform-script-properties-source-test!)))

(defn- perform-script-properties-component-test! []
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

(deftest script-properties-component
  (with-bindings {#'tu/use-new-code-editor? false}
    (perform-script-properties-component-test!))
  (with-bindings {#'tu/use-new-code-editor? true}
    (perform-script-properties-component-test!)))

(defn- perform-script-properties-broken-component-test! []
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

(deftest script-properties-broken-component
  (with-bindings {#'tu/use-new-code-editor? false}
    (perform-script-properties-broken-component-test!))
  (with-bindings {#'tu/use-new-code-editor? true}
    (perform-script-properties-broken-component-test!)))

(defn perform-script-properties-component-load-order-test! []
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
          (doseq [[prop-name prop-value] {"bool" true
                                          "hash" "hash2"
                                          "material" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.material")
                                          "number" 2.0
                                          "quat" [180.0 0.0, 0.0]
                                          "texture" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.atlas")
                                          "textureset" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.atlas")
                                          "url" "/url"
                                          "vec3" [1.0 2.0 3.0]
                                          "vec4" [1.0 2.0 3.0 4.0]}]
            (is (overridden? script-component prop-name))
            (is (= prop-value (prop script-component prop-name)))))))))

(deftest script-properties-component-load-order
  (with-bindings {#'tu/use-new-code-editor? false}
    (perform-script-properties-component-load-order-test!))
  (with-bindings {#'tu/use-new-code-editor? true}
    (perform-script-properties-component-load-order-test!)))

(defn- perform-script-properties-collection-test! []
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

(deftest script-properties-collection
  (with-bindings {#'tu/use-new-code-editor? false}
    (perform-script-properties-collection-test!))
  (with-bindings {#'tu/use-new-code-editor? true}
    (perform-script-properties-collection-test!)))

(defn- perform-script-properties-broken-collection-test! []
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

(deftest script-properties-broken-collection
  (with-bindings {#'tu/use-new-code-editor? false}
    (perform-script-properties-broken-collection-test!))
  (with-bindings {#'tu/use-new-code-editor? true}
    (perform-script-properties-broken-collection-test!)))

(defn perform-script-properties-collection-load-order-test! []
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
            (doseq [[prop-name prop-value] {"bool" true
                                            "hash" "hash3"
                                            "material" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.material")
                                            "number" 3.0
                                            "quat" [180.0 0.0, 0.0]
                                            "texture" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_collection.atlas")
                                            "textureset" (workspace/resolve-workspace-resource workspace "/script/resources/from_props_game_object.atlas")
                                            "url" "/url2"
                                            "vec3" [1.0 2.0 3.0]
                                            "vec4" [1.0 2.0 3.0 4.0]}]
              (is (= prop-value (prop script-component prop-name))))))))))

(deftest script-properties-collection-load-order
  (with-bindings {#'tu/use-new-code-editor? false}
    (perform-script-properties-collection-load-order-test!))
  (with-bindings {#'tu/use-new-code-editor? true}
    (perform-script-properties-collection-load-order-test!)))

(defn- build-resource [project path]
  (when-some [resource-node (tu/resource-node project path)]
    (:resource (first (g/node-value resource-node :build-targets)))))

(defn- texture-build-resource [project path]
  (when-some [resource-node (tu/resource-node project path)]
    (:resource (g/node-value resource-node :texture-build-target))))

(defn- build-output [project path]
  (with-open [in (io/input-stream (build-resource project path))
              out (ByteArrayOutputStream.)]
    (io/copy in out)
    (.toByteArray out)))

(defn- properties [node-id]
  (:properties (g/node-value node-id :_properties)))

(defn- built-resources [node-id]
  (into #{}
        (comp (remove sequential?)
              (keep :resource))
        (tree-seq #(or (sequential? %) (seq (:deps %)))
                  #(if (sequential? %) (seq %) (:deps %))
                  (g/node-value node-id :build-targets))))

(defn- material-resource-property? [property value]
  (and (is (= :property-type-resource (:go-prop-type property)))
       (is (= properties/sub-type-material (:go-prop-sub-type property)))
       (is (= value (:value property)))
       (is (= resource/Resource (:type (:edit-type property))))
       (is (= "material" (:ext (:edit-type property))))))

(defn- texture-resource-property? [property value]
  (and (is (= :property-type-resource (:go-prop-type property)))
       (is (= properties/sub-type-texture (:go-prop-sub-type property)))
       (is (= value (:value property)))
       (is (= resource/Resource (:type (:edit-type property))))
       (is (= ["atlas" "tilesource" "cubemap" "jpg" "png"] (:ext (:edit-type property))))))

(defn- textureset-resource-property? [property value]
  (and (is (= :property-type-resource (:go-prop-type property)))
       (is (= properties/sub-type-textureset (:go-prop-sub-type property)))
       (is (= value (:value property)))
       (is (= resource/Resource (:type (:edit-type property))))
       (is (= ["atlas" "tilesource"] (:ext (:edit-type property))))))

(deftest resource-script-properties-test
  (tu/with-loaded-project
    (let [resource (partial tu/resource workspace)
          resource-node (partial tu/resource-node project)
          build-resource (partial build-resource project)
          texture-build-resource (partial texture-build-resource project)]

      (let [props-script (resource-node "/script/props.script")
            properties (properties props-script)]
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_props_script.material")))
        (is (texture-resource-property? (:__texture properties) (resource "/script/resources/from_props_script.atlas")))
        (is (textureset-resource-property? (:__textureset properties) (resource "/script/resources/from_props_script.atlas")))
        (is (set/superset? (built-resources props-script)
                           #{(build-resource "/script/resources/from_props_script.material")
                             (texture-build-resource "/script/resources/from_props_script.atlas")
                             (build-resource "/script/resources/from_props_script.atlas")})))

      (let [props-game-object (resource-node "/game_object/props.go")
            props-script-component (:node-id (tu/outline props-game-object [0]))
            properties (properties props-script-component)]
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_props_game_object.material")))
        (is (texture-resource-property? (:__texture properties) (resource "/script/resources/from_props_game_object.atlas")))
        (is (textureset-resource-property? (:__textureset properties) (resource "/script/resources/from_props_game_object.atlas")))
        (is (set/superset? (built-resources props-game-object)
                           #{(build-resource "/script/resources/from_props_game_object.material")
                             (texture-build-resource "/script/resources/from_props_game_object.atlas")
                             (build-resource "/script/resources/from_props_game_object.atlas")})))

      (let [props-collection (resource-node "/collection/props.collection")
            props-script-component (:node-id (tu/outline props-collection [0 0]))
            properties (properties props-script-component)]
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_props_game_object.material")))
        (is (texture-resource-property? (:__texture properties) (resource "/script/resources/from_props_collection.atlas")))
        (is (textureset-resource-property? (:__textureset properties) (resource "/script/resources/from_props_game_object.atlas")))
        (is (set/superset? (built-resources props-collection)
                           #{(texture-build-resource "/script/resources/from_props_collection.atlas")})))

      (let [sub-props-collection (resource-node "/collection/sub_props.collection")
            props-script-component (:node-id (tu/outline sub-props-collection [0 0 0]))
            properties (properties props-script-component)]
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_props_game_object.material")))
        (is (texture-resource-property? (:__texture properties) (resource "/script/resources/from_props_collection.atlas")))
        (is (textureset-resource-property? (:__textureset properties) (resource "/script/resources/from_sub_props_collection.atlas")))
        (is (set/superset? (built-resources sub-props-collection)
                           #{(build-resource "/script/resources/from_sub_props_collection.atlas")})))

      (let [sub-sub-props-collection (resource-node "/collection/sub_sub_props.collection")
            props-script-component (:node-id (tu/outline sub-sub-props-collection [0 0 0 0]))
            properties (properties props-script-component)]
        (is (material-resource-property? (:__material properties) (resource "/script/resources/from_sub_sub_props_collection.material")))
        (is (texture-resource-property? (:__texture properties) (resource "/script/resources/from_props_collection.atlas")))
        (is (textureset-resource-property? (:__textureset properties) (resource "/script/resources/from_sub_props_collection.atlas")))
        (is (set/superset? (built-resources sub-sub-props-collection)
                           #{(build-resource "/script/resources/from_sub_sub_props_collection.material")}))))))

(defn- created-node [select-fn-call-logger]
  (let [calls (tu/call-logger-calls select-fn-call-logger)
        args (last calls)
        selection (first args)
        node-id (first selection)]
    node-id))

(defn- add-game-object! [collection game-object]
  (let [select-fn (tu/make-call-logger)]
    (collection/add-game-object-file collection collection (g/node-value game-object :resource) select-fn)
    (created-node select-fn)))

(defn- add-component! [game-object component]
  (let [select-fn (tu/make-call-logger)]
    (game-object/add-component-file game-object (g/node-value component :resource) select-fn)
    (created-node select-fn)))

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

(defn- build! [resource-node]
  (let [project (project/get-project resource-node)
        workspace (project/workspace project)]
    (project/build project resource-node (g/make-evaluation-context) nil)
    (tu/make-directory-deleter (workspace/build-path workspace))))

(defn- edit-property! [node-id prop-kw value]
  (let [evaluation-context (g/make-evaluation-context)
        property (get-in (g/node-value node-id :_properties evaluation-context) [:properties prop-kw])
        set-fn (get-in property [:edit-type :set-fn])
        from-fn (get-in property [:edit-type :from-type] identity)
        prop-node-id (:node-id property)
        old-value (from-fn (:value property))]
    (assert (not (:read-only? property)))
    (g/transact
      (if (some? set-fn)
        (set-fn evaluation-context prop-node-id old-value value)
        (g/set-property prop-node-id prop-kw value)))
    (g/update-cache-from-evaluation-context! evaluation-context)))

(defn- reset-property! [node-id prop-kw]
  (tu/prop-clear! (tu/prop-node-id node-id prop-kw) prop-kw))

(defn- find-corresponding [built-items node-with-id-property]
  (let [wanted-id (g/node-value node-with-id-property :id)]
    (some (fn [built-item]
            (when (= wanted-id (:id built-item))
              built-item))
          built-items)))

(deftest edit-resource-script-properties-test
  (with-clean-system
    (let [workspace (tu/setup-scratch-workspace! world "test/resources/empty_project")
          project (tu/setup-project! workspace)
          project-graph (g/node-id->graph-id project)
          resource (partial tu/resource workspace)
          build-resource (partial build-resource project)
          build-resource-path (comp resource/proj-path build-resource)
          build-output (partial build-output project)
          texture-build-resource (partial texture-build-resource project)
          texture-build-resource-path (comp resource/proj-path texture-build-resource)
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
                             (edit-property! :lines ["go.property('material',     material('/from-props-script.material'))"
                                                     "go.property('texture',       texture('/from-props-script.atlas'))"
                                                     "go.property('textureset', textureset('/from-props-script.atlas'))"]))]

          (testing "Script defaults"
            (let [properties (properties props-script)]
              (is (material-resource-property?   (:__material properties)   (resource "/from-props-script.material")))
              (is (texture-resource-property?    (:__texture properties)    (resource "/from-props-script.atlas")))
              (is (textureset-resource-property? (:__textureset properties) (resource "/from-props-script.atlas")))
              (is (= (built-resources props-script)
                     #{(build-resource         "/props.script")
                       (build-resource         "/from-props-script.atlas")
                       (texture-build-resource "/from-props-script.atlas")
                       (build-resource         "/from-props-script.material")
                       (build-resource         "/from-props-script.fp")
                       (build-resource         "/from-props-script.vp")}))
              (with-open [_ (build! props-script)]
                (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                  (is (= (unpack-property-declarations (:properties built-props-script))
                         {"material"   (build-resource-path         "/from-props-script.material")
                          "texture"    (texture-build-resource-path "/from-props-script.atlas")
                          "textureset" (build-resource-path         "/from-props-script.atlas")}))
                  (is (= (sort (:property-resources built-props-script))
                         (sort [(build-resource-path         "/from-props-script.material")
                                (texture-build-resource-path "/from-props-script.atlas")
                                (build-resource-path         "/from-props-script.atlas")]))))))

            (testing "Editing the script affects exposed properties"
              (with-open [_ (tu/make-graph-reverter project-graph)]
                (edit-property! props-script :lines ["go.property('other', texture('/from-props-script.atlas'))"])
                (let [properties (properties props-script)]
                  (is (not (contains? properties :__material)))
                  (is (not (contains? properties :__texture)))
                  (is (not (contains? properties :__textureset)))
                  (is (texture-resource-property? (:__other properties) (resource "/from-props-script.atlas"))))
                (is (= (built-resources props-script)
                       #{(build-resource         "/props.script")
                         (texture-build-resource "/from-props-script.atlas")}))
                (with-open [_ (build! props-script)]
                  (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                    (is (= (unpack-property-declarations (:properties built-props-script))
                           {"other" (texture-build-resource-path "/from-props-script.atlas")}))
                    (is (= (:property-resources built-props-script)
                           [(texture-build-resource-path "/from-props-script.atlas")]))))))

            (testing "Property type change texture -> textureset"
              (with-open [_ (tu/make-graph-reverter project-graph)]

                ;; Set up a script with a single property.
                (edit-property! props-script :lines ["go.property('chameleon', texture('/from-props-script.atlas'))"])
                (is (texture-resource-property? (:__chameleon (properties props-script)) (resource "/from-props-script.atlas")))
                (is (= (built-resources props-script)
                       #{(build-resource         "/props.script")
                         (texture-build-resource "/from-props-script.atlas")}))
                (with-open [_ (build! props-script)]
                  (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                    (is (= (unpack-property-declarations (:properties built-props-script))
                           {"chameleon" (texture-build-resource-path "/from-props-script.atlas")}))
                    (is (= (:property-resources built-props-script)
                           [(texture-build-resource-path "/from-props-script.atlas")]))))

                ;; Change the type of the property by editing the script code.
                (edit-property! props-script :lines ["go.property('chameleon', textureset('/from-props-script.atlas'))"])
                (is (textureset-resource-property? (:__chameleon (properties props-script)) (resource "/from-props-script.atlas")))
                (is (= (built-resources props-script)
                       #{(build-resource         "/props.script")
                         (build-resource         "/from-props-script.atlas")
                         (texture-build-resource "/from-props-script.atlas")}))
                (with-open [_ (build! props-script)]
                  (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                    (is (= (unpack-property-declarations (:properties built-props-script))
                           {"chameleon" (build-resource-path "/from-props-script.atlas")}))
                    (is (= (:property-resources built-props-script)
                           [(build-resource-path "/from-props-script.atlas")]))))))

            (testing "Property type change textureset -> texture"
              (with-open [_ (tu/make-graph-reverter project-graph)]

                ;; Set up a script with a single property.
                (edit-property! props-script :lines ["go.property('chameleon', textureset('/from-props-script.atlas'))"])
                (is (textureset-resource-property? (:__chameleon (properties props-script)) (resource "/from-props-script.atlas")))
                (is (= (built-resources props-script)
                       #{(build-resource         "/props.script")
                         (build-resource         "/from-props-script.atlas")
                         (texture-build-resource "/from-props-script.atlas")}))
                (with-open [_ (build! props-script)]
                  (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                    (is (= (unpack-property-declarations (:properties built-props-script))
                           {"chameleon" (build-resource-path "/from-props-script.atlas")}))
                    (is (= (:property-resources built-props-script)
                           [(build-resource-path "/from-props-script.atlas")]))))

                ;; Change the type of the property by editing the script code.
                (edit-property! props-script :lines ["go.property('chameleon', texture('/from-props-script.atlas'))"])
                (is (texture-resource-property? (:__chameleon (properties props-script)) (resource "/from-props-script.atlas")))
                (is (= (built-resources props-script)
                       #{(build-resource         "/props.script")
                         (texture-build-resource "/from-props-script.atlas")}))
                (with-open [_ (build! props-script)]
                  (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                    (is (= (unpack-property-declarations (:properties built-props-script))
                           {"chameleon" (texture-build-resource-path "/from-props-script.atlas")}))
                    (is (= (:property-resources built-props-script)
                           [(texture-build-resource-path "/from-props-script.atlas")])))))))

          (testing "Game object overrides"
            (let [props-game-object (make-resource-node! "/props.go")
                  props-script-component (add-component! props-game-object props-script)]

              (testing "Before overrides"
                (let [properties (properties props-script-component)]
                  (is (material-resource-property?   (:__material properties)   (resource "/from-props-script.material")))
                  (is (texture-resource-property?    (:__texture properties)    (resource "/from-props-script.atlas")))
                  (is (textureset-resource-property? (:__textureset properties) (resource "/from-props-script.atlas")))
                  (is (not (tu/prop-overridden? props-script-component :__material)))
                  (is (not (tu/prop-overridden? props-script-component :__texture)))
                  (is (not (tu/prop-overridden? props-script-component :__textureset)))
                  (is (= (built-resources props-game-object)
                         #{(build-resource         "/props.go")
                           (build-resource         "/props.script")
                           (build-resource         "/from-props-script.atlas")
                           (texture-build-resource "/from-props-script.atlas")
                           (build-resource         "/from-props-script.material")
                           (build-resource         "/from-props-script.fp")
                           (build-resource         "/from-props-script.vp")}))
                  (with-open [_ (build! props-game-object)]
                    (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))]
                      (is (empty? (unpack-property-declarations (:properties built-props-game-object))))
                      (is (empty? (:property-resources built-props-game-object)))))))

              (testing "Overrides do not affect props script"
                (with-open [_ (tu/make-graph-reverter project-graph)]
                  (edit-property! props-script-component :__material   (resource "/from-props-game-object.material"))
                  (edit-property! props-script-component :__texture    (resource "/from-props-game-object.atlas"))
                  (edit-property! props-script-component :__textureset (resource "/from-props-game-object.atlas"))

                  (let [properties (properties props-script)]
                    (is (material-resource-property?   (:__material properties)   (resource "/from-props-script.material")))
                    (is (texture-resource-property?    (:__texture properties)    (resource "/from-props-script.atlas")))
                    (is (textureset-resource-property? (:__textureset properties) (resource "/from-props-script.atlas")))
                    (is (= (built-resources props-script)
                           #{(build-resource         "/props.script")
                             (build-resource         "/from-props-script.atlas")
                             (texture-build-resource "/from-props-script.atlas")
                             (build-resource         "/from-props-script.material")
                             (build-resource         "/from-props-script.fp")
                             (build-resource         "/from-props-script.vp")}))
                    (with-open [_ (build! props-game-object)]
                      (let [built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                        (is (= (unpack-property-declarations (:properties built-props-script))
                               {"material"   (build-resource-path         "/from-props-script.material")
                                "texture"    (texture-build-resource-path "/from-props-script.atlas")
                                "textureset" (build-resource-path         "/from-props-script.atlas")}))
                        (is (= (sort (:property-resources built-props-script))
                               (sort [(build-resource-path         "/from-props-script.material")
                                      (texture-build-resource-path "/from-props-script.atlas")
                                      (build-resource-path         "/from-props-script.atlas")]))))))))

              (testing "Overrides"
                (let [original-property-values (into {}
                                                     (map (fn [[prop-kw {:keys [value]}]]
                                                            [prop-kw value]))
                                                     (properties props-script))
                      original-property-resources (map resource/proj-path
                                                       [(build-resource         "/from-props-script.material")
                                                        (texture-build-resource "/from-props-script.atlas")
                                                        (build-resource         "/from-props-script.atlas")])]
                  (doseq [[assigned? prop-kw resource build-resource] [[material-resource-property?   :__material   (resource "/from-props-game-object.material") (build-resource         "/from-props-game-object.material")]
                                                                       [texture-resource-property?    :__texture    (resource "/from-props-game-object.atlas")    (texture-build-resource "/from-props-game-object.atlas")]
                                                                       [textureset-resource-property? :__textureset (resource "/from-props-game-object.atlas")    (build-resource         "/from-props-game-object.atlas")]]]
                    (with-open [_ (tu/make-graph-reverter project-graph)]

                      ;; Apply override.
                      (edit-property! props-script-component prop-kw resource)
                      (is (tu/prop-overridden? props-script-component prop-kw))
                      (is (assigned? (get (properties props-script-component) prop-kw) resource))
                      (is (contains? (built-resources props-game-object) build-resource))
                      (with-open [_ (build! props-game-object)]
                        (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))
                              built-props-script (protobuf/bytes->map Lua$LuaModule (build-output "/props.script"))]
                          (is (= (:property-resources built-props-game-object)
                                 [(resource/proj-path build-resource)]))
                          (is (= (sort (:property-resources built-props-script))
                                 (sort original-property-resources)))))

                      ;; Clear override.
                      (reset-property! props-script-component prop-kw)
                      (is (not (tu/prop-overridden? props-script-component prop-kw)))
                      (is (assigned? (get (properties props-script-component) prop-kw) (original-property-values prop-kw)))
                      (is (not (contains? (built-resources props-game-object) build-resource)))
                      (with-open [_ (build! props-game-object)]
                        (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))]
                          (is (empty? (unpack-property-declarations (:properties built-props-game-object))))
                          (is (empty? (:property-resources built-props-game-object)))))))))

              (testing "Property type change texture -> textureset"
                (with-open [_ (tu/make-graph-reverter project-graph)]

                  ;; Set up a script with a single property, referenced from a game object.
                  (edit-property! props-script :lines ["go.property('chameleon', texture('/from-props-script.atlas'))"])
                  (is (texture-resource-property? (:__chameleon (properties props-script-component)) (resource "/from-props-script.atlas")))
                  (is (= (built-resources props-game-object)
                         #{(build-resource         "/props.go")
                           (build-resource         "/props.script")
                           (texture-build-resource "/from-props-script.atlas")}))
                  (with-open [_ (build! props-game-object)]
                    (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))]
                      (is (empty? (unpack-property-declarations (:properties built-props-game-object))))
                      (is (empty? (:property-resources built-props-game-object)))))

                  ;; Override the property in the game object.
                  (edit-property! props-script-component :__chameleon (resource "/from-props-game-object.atlas"))
                  (is (texture-resource-property? (:__chameleon (properties props-script-component)) (resource "/from-props-game-object.atlas")))
                  (is (= (built-resources props-game-object)
                         #{(build-resource         "/props.go")
                           (build-resource         "/props.script")
                           (texture-build-resource "/from-props-script.atlas")
                           (texture-build-resource "/from-props-game-object.atlas")}))
                  (with-open [_ (build! props-game-object)]
                    (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))
                          built-props-script-component (find-corresponding (:components built-props-game-object) props-script-component)]
                      (is (= (unpack-property-declarations (:property-decls built-props-script-component))
                             {"chameleon" (texture-build-resource-path "/from-props-game-object.atlas")}))
                      (is (= (:property-resources built-props-game-object)
                             [(texture-build-resource-path "/from-props-game-object.atlas")]))))

                  ;; Change the type of the property by editing the script code.
                  (edit-property! props-script :lines ["go.property('chameleon', textureset('/from-props-script.atlas'))"])
                  (is (textureset-resource-property? (:__chameleon (properties props-script-component)) (resource "/from-props-game-object.atlas")))
                  (is (= (built-resources props-game-object)
                         #{(build-resource         "/props.go")
                           (build-resource         "/props.script")
                           (build-resource         "/from-props-script.atlas")
                           (texture-build-resource "/from-props-script.atlas")
                           (build-resource         "/from-props-game-object.atlas")
                           (texture-build-resource "/from-props-game-object.atlas")}))
                  (with-open [_ (build! props-game-object)]
                    (let [built-props-game-object (protobuf/bytes->map GameObject$PrototypeDesc (build-output "/props.go"))
                          built-props-script-component (find-corresponding (:components built-props-game-object) props-script-component)]
                      (is (= (unpack-property-declarations (:property-decls built-props-script-component))
                             {"chameleon" (build-resource-path "/from-props-game-object.atlas")}))
                      (is (= (:property-resources built-props-game-object)
                             [(build-resource-path "/from-props-game-object.atlas")])))))))))))))
