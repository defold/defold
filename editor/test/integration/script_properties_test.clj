(ns integration.script-properties-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as tu]))

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
                                          "number" 2.0
                                          "quat" [180.0 0.0, 0.0]
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
        (let [project (tu/setup-project! workspace resource-load-order)
              nodes-by-resource-path (g/node-value project :nodes-by-resource-path)
              collection (nodes-by-resource-path (resource/proj-path collection-resource))
              script-node-outline (tu/outline collection [0 0])
              script-component (:node-id script-node-outline)]
          (is (= "script" (:label script-node-outline)))
          (doseq [[prop-name prop-value] {"bool" true
                                          "hash" "hash3"
                                          "number" 3.0
                                          "quat" [180.0 0.0, 0.0]
                                          "url" "/url2"
                                          "vec3" [1.0 2.0 3.0]
                                          "vec4" [1.0 2.0 3.0 4.0]}]
            (is (= prop-value (prop script-component prop-name)))))))))

(deftest script-properties-collection-load-order
  (with-bindings {#'tu/use-new-code-editor? false}
    (perform-script-properties-collection-load-order-test!))
  (with-bindings {#'tu/use-new-code-editor? true}
    (perform-script-properties-collection-load-order-test!)))
