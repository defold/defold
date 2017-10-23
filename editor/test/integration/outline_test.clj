(ns integration.outline-test
  (:require [clojure.test :refer :all]
            [service.log :as log]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.app-view :as app-view]
            [editor.collection :as collection]
            [editor.defold-project :as project]
            [editor.game-object :as game-object]
            [editor.outline :as outline]
            [integration.test-util :as test-util]))

(defn- outline
  ([node]
    (outline node []))
  ([node path]
    (loop [outline (g/node-value node :node-outline)
           path path]
      (if-let [segment (first path)]
        (recur (get (vec (:children outline)) segment) (rest path))
        outline))))

(def ^:private ^:dynamic *clipboard* nil)
(def ^:private ^:dynamic *dragboard* nil)
(def ^:private ^:dynamic *drag-source-iterators* nil)

(defrecord TestItemIterator [root-node path]
  outline/ItemIterator
  (value [this] (outline root-node path))
  (parent [this] (when (not (empty? path))
                   (TestItemIterator. root-node (butlast path)))))

(defn- ->iterator [root-node path]
  (TestItemIterator. root-node path))

(defn- delete? [node path]
  (outline/delete? [(->iterator node path)]))

(defn- delete! [node path]
  (when (delete? node path)
    (g/delete-node! (g/override-root (:node-id (outline node path))))))

(defn- copy! [node path]
  (let [data (outline/copy [(->iterator node path)])]
    (alter-var-root #'*clipboard* (constantly data))))

(defn- cut? [node & paths]
  (outline/cut? (mapv #(->iterator node %) paths)))

(defn- cut! [node & paths]
  (let [data (outline/cut! (mapv #(->iterator node %) paths))]
    (alter-var-root #'*clipboard* (constantly data))))

(defn- paste!
  ([project app-view node]
    (paste! project app-view node []))
  ([project app-view node path]
    (let [it (->iterator node path)]
      (assert (outline/paste? (project/graph project) it *clipboard*))
      (outline/paste! (project/graph project) it *clipboard* (partial app-view/select app-view)))))

(defn- copy-paste! [project app-view node path]
  (copy! node path)
  (paste! project app-view node (butlast path)))

(defn- drag? [node path]
  (outline/drag? (g/node-id->graph-id node) [(->iterator node path)]))

(defn- drag! [node path]
  (let [src-item-iterators [(->iterator node path)]
        data (outline/copy src-item-iterators)]
    (alter-var-root #'*dragboard* (constantly data))
    (alter-var-root #'*drag-source-iterators* (constantly src-item-iterators))))

(defn- drop!
  ([project app-view node]
    (drop! project app-view node []))
  ([project app-view node path]
    (outline/drop! (project/graph project) *drag-source-iterators* (->iterator node path) *dragboard* (partial app-view/select app-view))))

(defn- drop?
  ([project node]
    (drop? project node []))
  ([project node path]
    (outline/drop? (project/graph project) *drag-source-iterators* (->iterator node path) *dragboard*)))

(defn- child-count
  ([node]
    (child-count node []))
  ([node path]
   (count (:children (outline node path)))))

(defn- outline-seq [root]
  (tree-seq :children :children (g/node-value root :node-outline)))

(defn- outline-labeled
  [root label]
  (first (filter #(= label (:label %)) (outline-seq root))))

(defn- label [root path]
  (:label (test-util/outline root path)))

(deftest copy-paste-ref-component
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/logic/main.go")]
      (is (= 5 (child-count root)))
      (copy-paste! project app-view root [0])
      (is (= 6 (child-count root))))))

(deftest copy-paste-double-embed
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/collection/embedded_embedded_sounds.collection")]
      ; 2 go instance
      (is (= 2 (child-count root)))
      (copy! root [0])
      (paste! project app-view root)
      ; 3 go instances
      (is (= 3 (child-count root))))))

(deftest copy-paste-component-onto-go-instance
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/collection/embedded_embedded_sounds.collection")]
      ; 1 comp instance
      (is (= 1 (child-count root [0])))
      (copy! root [0 0])
      (paste! project app-view root [0])
      ; 2 comp instances
      (is (= 2 (child-count root [0]))))))

(deftest copy-paste-game-object
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/logic/atlas_sprite.go")]
      ; 1 comp instance
      (is (= 1 (child-count root)))
      (copy! root [0])
      (paste! project app-view root)
      ; 2 comp instances
      (is (= 2 (child-count root)))
      (is (contains? (outline root [1]) :icon))
      (cut! root [0])
      ; 1 comp instances
      (is (= 1 (child-count root))))))

(deftest delete-component
  (with-clean-system
    (let [[workspace project app-view] (test-util/setup! world)
          root (test-util/resource-node project "/logic/atlas_sprite.go")]
      ; 1 comp instance
      (is (= 1 (child-count root)))
      (delete! root [0])
      (is (= 0 (child-count root))))))

(deftest copy-paste-collection
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/logic/atlas_sprite.collection")]
      ; * Collection
      ;   * main (ref-game-object)
      ;     * sprite (component)
      ; 1 go instance
      (is (= 1 (child-count root)))
      ; 1 sprite comp
      (is (= 1 (child-count root [0])))
      (copy! root [0]) ;; copy go-instance
      (paste! project app-view root) ;; paste into root
      ; 2 go instances
      (is (= 2 (child-count root)))
      ; 1 sprite comp
      (is (= 1 (child-count root [1])))
      (paste! project app-view root [0])
      ; 1 sprite comp + 1 go instance
      (is (= 2 (child-count root [0])))
      ; go instance can be cut
      (is (cut? root [0 0]))
      (cut! root [0 0])
      ; 1 sprite
      (is (= 1 (child-count root [0]))))))

(deftest copy-paste-between-collections
  (test-util/with-loaded-project
    (let [;; * Collection
          ;;   * main (ref-game-object)
          ;;     * sprite (component)
          src-root (test-util/resource-node project "/logic/atlas_sprite.collection")
          ;; * Collection
          tgt-root (test-util/resource-node project "/collection/test.collection")]
      ; 0 go instance
      (is (= 0 (child-count tgt-root)))
      (copy! src-root [0]) ;; copy go-instance from source
      (paste! project app-view tgt-root) ;; paste into target root
      ; 1 go instance
      (is (= 1 (child-count tgt-root))))))

(deftest copy-paste-collection-instance
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/collection/sub_props.collection")]
      ; Original tree
      ; + Collection (root)
      ;   + props (collection)
      ;     + props (go)
      ;     + props_embedded (go)
      ; 1 collection instance
      (is (= 1 (child-count root)))
      ; 2 go instances
      (is (= 2 (child-count root [0])))
      (copy! root [0])
      (paste! project app-view root)
      (is (= 2 (child-count root)))
      ; 2 go instances in referenced collection
      (is (= 2 (child-count root [0])))
      (is (= 2 (child-count root [1])))
      (cut! root [0 0])
      ; 1 go instance remains in referenced collection
      (is (= 1 (child-count root [0])))
      (is (= 1 (child-count root [1])))
      (paste! project app-view root)
      ; 2 collection instances + 1 go instances
      (is (= 3 (child-count root)))
      (cut! root [2])
      (paste! project app-view root [0])
      ; 2 collection instances
      (is (= 2 (child-count root)))
      ; 2 go instances in referenced collection
      (is (= 2 (child-count root [0])))
      (is (= 2 (child-count root [1]))))))

(deftest dnd-collection
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/logic/atlas_sprite.collection")]
      (is (= 1 (child-count root)))
      (let [first-id (get (outline root [0]) :label)]
        (drag! root [0])
        (is (not (drop? project root)))
        (is (not (drop? project root [0])))
        (copy-paste! project app-view root [0])
        (is (= 2 (child-count root)))
        (let [second-id (get (outline root [1]) :label)]
          (is (not= first-id second-id))
          (drag! root [1])
          (is (drop? project root [0]))
          (drop! project app-view root [0])
          (is (= 1 (child-count root)))
          (is (= 2 (child-count root [0])))
          (is (= second-id (get (outline root [0 0]) :label)))
          (drag! root [0 0])
          (drop! project app-view root)
          (is (= 2 (child-count root)))
          (is (= second-id (get (outline root [1]) :label))))))))

(deftest copy-paste-dnd-collection
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/logic/atlas_sprite.collection")]
      (copy-paste! project app-view root [0])
      (drag! root [0])
      (drop! project app-view root [1]))))

(deftest read-only-items
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/logic/main.gui")]
      (doseq [path [[] [0] [1] [2] [3]]]
        (is (not (delete? root path)))
        (is (not (cut? root path)))
        (is (not (drag? root path)))))))

(deftest dnd-gui
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/logic/main.gui")]
      (let [first-id (get (outline root [0 1]) :label)
            next-id (get (outline root [0 2]) :label)]
        (drag! root [0 1])
        (drop! project app-view root [0 0])
        (let [second-id (get (outline root [0 0 0]) :label)]
          (is (= first-id second-id))
          (is (= next-id (get (outline root [0 1]) :label)))
          (drag! root [0 0 0])
          (drop! project app-view root [0])
          (is (= second-id (get (outline root [0 5]) :label))))))))

(defn- prop [root path property]
  (let [p (g/node-value (:node-id (outline root path)) :_properties)]
    (get-in p [:properties property :value])))

(deftest copy-paste-gui-box
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/gui/simple.gui")
          path [0 0]
          texture (prop root path :texture)]
      (copy-paste! project app-view root path)
      (is (= texture (prop root [0 2] :texture))))))

(deftest copy-paste-gui-text-utf-16
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/gui/simple.gui")
          path [0 1]
          text (prop root path :text)]
      (copy-paste! project app-view root path)
      (is (= text (prop root [0 2] :text))))))

(deftest copy-paste-gui-template
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/gui/scene.gui")
          path [0 1]
          orig-sub-id (prop root (conj path 0) :generated-id)]
      (is (= "sub_scene/sub_box" (:label (outline root [0 1 0]))))
      (copy-paste! project app-view root path)
      (is (= orig-sub-id (prop root (conj path 0) :generated-id)))
      (let [copy-path [0 4]
            copy-sub-id (prop root (conj copy-path 0) :generated-id)]
        (is (not (nil? copy-sub-id)))
        (is (not= copy-sub-id orig-sub-id))
        (is (= "sub_scene/sub_box" (:label (outline root [0 1 0]))))
        (is (= "sub_scene1/sub_box" (:label (outline root [0 4 0])))))))
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/gui/super_scene.gui")
          tmpl-path [0 0]]
      (g/transact (g/set-property (:node-id (outline root (conj tmpl-path 0))) :position [-100.0 0.0 0.0]))
      (copy-paste! project app-view root tmpl-path)
      (let [p (g/node-value (:node-id (outline root [0 1])) :_properties)]
        (is (= -100.0 (get-in p [:properties :template :value :overrides "box" :position 0])))))))

(deftest copy-paste-gui-template-delete-repeat
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/gui/scene.gui")
          path [0 1]]
      (dotimes [i 5]
        (is (= "sub_scene" (:label (outline root path))))
        (is (not (outline-labeled root "sub_scene1")))
        (copy-paste! project app-view root path)
        (let [new-tmpl (outline-labeled root "sub_scene1")]
          (is new-tmpl)
          (is (g/node-value (:node-id new-tmpl) :_properties))
          (g/transact (g/delete-node (:node-id new-tmpl))))))))

(deftest dnd-gui-template
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/gui/scene.gui")
          tmpl-path [0 1]
          new-pos [-100.0 0.0 0.0]
          super-root (test-util/resource-node project "/gui/super_scene.gui")
          super-tmpl-path [0 0]]
      (is (contains? (:overrides (prop super-root super-tmpl-path :template)) "sub_scene/sub_box"))
      (is (= "sub_scene" (get (outline root tmpl-path) :label)))
      (is (not (nil? (outline root (conj tmpl-path 0)))))
      (let [sub-id (:node-id (outline root (conj tmpl-path 0)))]
        (g/transact (g/set-property sub-id :position new-pos)))
      (drag! root tmpl-path)
      (drop! project app-view root [0 0])
      (let [tmpl-path [0 0 1]]
        (is (= -100.0 (get-in (prop root tmpl-path :template) [:overrides "sub_box" :position 0])))
        (is (= "sub_scene" (get (outline root tmpl-path) :label)))
        (is (not (nil? (outline root (conj tmpl-path 0)))))
        (is (= new-pos (prop root (conj tmpl-path 0) :position))))
      (is (contains? (:overrides (prop super-root super-tmpl-path :template)) "sub_scene/sub_box")))))

(deftest gui-template-overrides
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/gui/scene.gui")
          paths [[0 1] [0 1 0]]
          new-pos [-100.0 0.0 0.0]
          sub-box (:node-id (outline root [0 1 0]))]
      (g/transact (g/set-property sub-box :position new-pos))
      ;; NOTE: Only the affected node is marked as overridden.
      ;; Parent nodes obtain the :child-overridden? attribute
      ;; when decorated in the outline view.
      (is (not (:outline-overridden? (outline root [0]))))
      (is (not (:outline-overridden? (outline root [0 1]))))
      (is (:outline-overridden? (outline root [0 1 0]))))))

(deftest gui-template-structure-modifiable
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/gui/scene.gui")
          sub-path [0 1 0]]
      (is (delete? root sub-path))
      (is (cut? root sub-path))
      (is (drag? root sub-path)))))

(deftest outline-shows-missing-parts
  (with-clean-system
    (let [[workspace project] (log/without-logging (test-util/setup! world "test/resources/missing_project"))]  ; no logging as purposely partially broken project
      (testing "Missing go file visible in collection outline"
       (let [root (test-util/resource-node project "/missing_go.collection")]
         (is (= 1 (child-count root)))
         (is (.startsWith (:label (outline root [0])) "non-existent"))))
      (testing "Missing sub collection visible in collection outline"
       (let [root (test-util/resource-node project "/missing_collection.collection")]
         (is (= 1 (child-count root)))
         (is (.startsWith  (:label (outline root [0])) "non-existent"))))
      (testing "Missing script visible in go outline"
        (let [root (test-util/resource-node project "/missing_component.go")]
          (is (= 1 (child-count root)))
          (is (.startsWith (:label (outline root [0])) "non-existent"))))
      (testing "Missing script visible in collection-go-outline"
        (let [root (test-util/resource-node project "/missing_go_component.collection")
              labels (map :label (outline-seq root))
              expected-prefixes ["Collection" "missing_component" "non-existent"]]
          (is (= 3 (count labels))) ; collection + go + script
          (is (every? true? (map #(.startsWith %1 %2) labels expected-prefixes))))))))

(deftest outline-shows-nil-parts
  (with-clean-system
    (let [[workspace project] (log/without-logging (test-util/setup! world "test/resources/nil_project"))]  ; no logging as purposely partially broken project
      (testing "Nil go file visible in collection outline"
        (let [root (test-util/resource-node project "/nil_go.collection")]
          (is (= 1 (child-count root)))
          (is (.startsWith (:label (outline root [0])) "nil-go"))))
      (testing "Nil sub collection visible in collection outline"
        (let [root (test-util/resource-node project "/nil_collection.collection")]
          (is (= 1 (child-count root)))
          (is (.startsWith  (:label (outline root [0])) "nil-collection"))))
      (testing "Nil script visible in go outline"
        (let [root (test-util/resource-node project "/nil_component.go")]
          (is (= 1 (child-count root)))
          (is (.startsWith (:label (outline root [0])) "nil-component"))))
      (testing "Nil script visible in collection-go-outline"
        (let [root (test-util/resource-node project "/nil_go_component.collection")
              labels (map :label (outline-seq root))
              expected-prefixes ["Collection" "nil-go" "nil-component"]]
          (is (= 3 (count labels))) ; collection + go + script
          (is (every? true? (map #(.startsWith %1 %2) labels expected-prefixes))))))))

(deftest outline-tile-source
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/graphics/sprites.tileset")
          ol (g/node-value node-id :node-outline)]
      (is (some? ol)))))

(deftest copy-paste-particlefx
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/particlefx/fireworks_big.particlefx")]
      ; Original tree
      ; Root (particlefx)
      ; + Drag (modifier)
      ; + Acceleration (modifier)
      ; + primary (emitter)
      ; + secondary (emitter)
      (is (= 4 (child-count root)))
      (copy! root [2])
      (paste! project app-view root)
      (is (= 5 (child-count root)))
      (is (some? (g/node-value (:node-id (outline root [3])) :scene))))))

(deftest copied-contents
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/collection/sub_sub_props.collection")]
      ;; Original tree
      ;; Collection (collection "/collection/sub_sub_props.collection")
      ;; + sub_props (collection "/collection/sub_props.collection")
      ;;   + props (collection "/collection/props.collection")
      ;;     + props (game_object "/game_object/props.go")
      ;;       + script (script "/script/props.script")
      ;;     + props_embedded (game-object)
      ;;       + script (script "/script/props.script")

      ;; Copy "props_embedded" game_object.
      (is (= "props_embedded" (:label (outline root [0 0 1]))))
      (let [{:keys [arcs attachments nodes]} (#'outline/deserialize (copy! root [0 0 1]))
            serial-id->node-type (into {} (map (juxt :serial-id :node-type)) nodes)]

        (is (= {0 collection/EmbeddedGOInstanceNode
                1 game-object/GameObjectNode
                2 game-object/ReferencedComponent}
               serial-id->node-type))

        ;; When pasting, we will invoke the tx-attach-fn from the matching req.
        ;; In this case, EmbeddedGOInstanceNode will select the GameObjectNode
        ;; as the parent for the pasted ReferencedComponent.
        (is (= [[0 2]]
               (sort attachments)))

        ;; Copied arcs should not include connections made by the tx-attach-fn
        ;; selected to attach the ReferencedComponent to the GameObjectNode.
        (is (= [[0 :url 1 :base-url]
                [1 :_node-id 0 :source-id]
                [1 :build-targets 0 :source-build-targets]
                [1 :ddf-component-properties 0 :ddf-component-properties]
                [1 :node-outline 0 :source-outline]
                [1 :proto-msg 0 :proto-msg]
                [1 :resource 0 :source-resource]
                [1 :save-data 0 :source-save-data]
                [1 :scene 0 :scene]]
               (sort arcs)))))))

(deftest drag-drop-between-referenced-collections
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/collection/sub_sub_props.collection")]
      ;; Original tree
      ;; Collection (collection "/collection/sub_sub_props.collection")
      ;; + sub_props (collection "/collection/sub_props.collection")
      ;;   + props (collection "/collection/props.collection")
      ;;     + props (game_object "/game_object/props.go")
      ;;       + script (script "/script/props.script")
      ;;     + props_embedded (game-object)
      ;;       + script (script "/script/props.script")

      ;; Drag "props_embedded" game_object to the root collection.
      (is (= "props_embedded" (:label (outline root [0 0 1]))))
      (is (= 1 (child-count root)))
      (is (= 2 (child-count root [0 0])))
      (drag! root [0 0 1])
      (drop! project app-view root)
      (is (= 2 (child-count root)))
      (is (= 1 (child-count root [0 0])))

      ;; Verify connections.
      (let [dragged-game-object (:node-id (outline root [1]))]
        (is (false? (g/override? dragged-game-object)))
        (is (= "props_embedded" (g/node-value dragged-game-object :id)))
        (is (= ["props_embedded"
                "sub_props"]
               (sort (keys (g/node-value root :id-counts)))))
        (g/set-property! dragged-game-object :id "props_embedded_renamed")
        (is (= ["props_embedded_renamed"
                "sub_props"]
               (sort (keys (g/node-value root :id-counts)))))
        (g/set-property! dragged-game-object :id "props_embedded"))

      ;; Drag "props_embedded" game_object under the "props" game object inside the "props" collection.
      (is (= "props_embedded" (:label (outline root [1]))))
      (is (= "props" (:label (outline root [0 0]))))
      (is (= "props" (:label (outline root [0 0 0]))))
      (is (= 2 (child-count root)))
      (is (= 1 (child-count root [0 0 0])))
      (drag! root [1])
      (drop! project app-view root [0 0 0])
      (is (= 1 (child-count root)))
      (is (= 2 (child-count root [0 0 0])))

      ;; Verify connections.
      (let [dragged-game-object (:node-id (outline root [0 0 0 0]))]
        (is (true? (g/override? dragged-game-object)))
        (is (= "props_embedded" (g/node-value dragged-game-object :id)))
        (is (= ["props"
                "props_embedded"]
               (sort (keys (g/node-value dragged-game-object :id-counts)))))
        (g/set-property! dragged-game-object :id "props_embedded_renamed")
        (is (= ["props"
                "props_embedded_renamed"]
               (sort (keys (g/node-value dragged-game-object :id-counts)))))))))

(deftest cut-paste-between-referenced-collections
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/collection/sub_sub_props.collection")
          sub-props (test-util/resource-node project "/collection/sub_props.collection")]
      ;; Original tree
      ;; Collection (collection "/collection/sub_sub_props.collection")
      ;; + sub_props (collection "/collection/sub_props.collection")
      ;;   + props (collection "/collection/props.collection")
      ;;     + props (game_object "/game_object/props.go")
      ;;       + script (script "/script/props.script")
      ;;     + props_embedded (game-object)
      ;;       + script (script "/script/props.script")

      ;; Cut "props_embedded" game_object and paste it below "sub_props".
      (is (= "props_embedded" (:label (outline root [0 0 1]))))
      (is (= 2 (child-count root [0 0])))
      (cut! root [0 0 1])
      (is (= 1 (child-count root [0 0])))

      (is (= "sub_props" (:label (outline root [0]))))
      (is (nil? (outline-labeled sub-props "props_embedded")))
      (is (= 1 (child-count root [0])))
      (paste! project app-view root [0])
      (is (= 2 (child-count root [0])))
      (is (some? (outline-labeled sub-props "props_embedded")))

      ;; Verify connections.
      (let [pasted-game-object (:node-id (outline root [0 1]))
            sibling-game-object (:node-id (outline root [0 0]))]
        (is (g/override? pasted-game-object))
        (is (= "props_embedded" (g/node-value pasted-game-object :id)))
        (is (= ["props"
                "props_embedded"]
               (sort (keys (g/node-value pasted-game-object :id-counts)))))
        (g/set-property! sibling-game-object :id "props_renamed")
        (is (= ["props_embedded"
                "props_renamed"]
               (sort (keys (g/node-value pasted-game-object :id-counts))))))

      ;; Cut "props" game_object and paste it below the root collection.
      (is (= "props" (:label (outline root [0 0 0]))))
      (is (= 1 (child-count root [0 0])))
      (cut! root [0 0 0])
      (is (= 0 (child-count root [0 0])))

      (is (= "Collection" (:label (outline root))))
      (is (= 1 (child-count root)))
      (paste! project app-view root)
      (is (= 2 (child-count root)))

      ;; Verify connections.
      (let [pasted-game-object (:node-id (outline root [1]))
            sibling-coll-instance (:node-id (outline root [0]))]
        (is (false? (g/override? pasted-game-object)))
        (is (= "props" (g/node-value pasted-game-object :id)))
        (is (= ["props"
                "sub_props"]
               (sort (keys (g/node-value pasted-game-object :id-counts)))))
        (g/set-property! sibling-coll-instance :id "sub_props_renamed")
        (is (= ["props"
                "sub_props_renamed"]
               (sort (keys (g/node-value pasted-game-object :id-counts)))))))))

(deftest cut-paste-between-referenced-gui-scenes
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/gui/super_scene.gui")
          sub-scene (test-util/resource-node project "/gui/sub_scene.gui")]
      ;; Original tree
      ;; Gui (gui "/gui/super_scene.gui")
      ;; + Nodes
      ;;   + scene (template "/gui/scene.gui")
      ;;     + scene/box (box)
      ;;       + scene/pie (pie)
      ;;     + scene/sub_scene (template "/gui/sub_scene.gui")
      ;;       + scene/sub_scene/sub_box (box)
      ;;     + scene/box1 (box)
      ;;     + scene/text (text)

      ;; Cut "scene/pie" and paste it below "scene/sub_scene/sub_box".
      (is (= "scene/pie" (:label (outline root [0 0 0 0]))))
      (is (= 1 (child-count root [0 0 0])))
      (cut! root [0 0 0 0])
      (is (= 0 (child-count root [0 0 0])))

      (is (= "scene/sub_scene/sub_box" (:label (outline root [0 0 1 0]))))
      (is (nil? (outline-labeled sub-scene "pie")))
      (is (= 0 (child-count root [0 0 1 0])))
      (paste! project app-view root [0 0 1 0])
      (is (= 1 (child-count root [0 0 1 0])))
      (is (some? (outline-labeled sub-scene "pie")))

      ;; Verify connections.
      (let [sub-box (:node-id (outline root [0 0 1 0]))
            pasted-pie (:node-id (outline root [0 0 1 0 0]))]
        (is (g/override? pasted-pie))
        (is (= "scene/sub_scene/pie" (g/node-value pasted-pie :id)))
        (is (= "scene/sub_scene/sub_box" (g/node-value sub-box :id)))
        (is (= ["scene/sub_scene/pie"
                "scene/sub_scene/sub_box"]
               (sort (keys (g/node-value pasted-pie :id-counts)))))
        (g/set-property! sub-box :id "sub_box_renamed")
        (is (= "scene/sub_scene/sub_box_renamed" (g/node-value sub-box :id)))
        (is (= ["scene/sub_scene/pie"
                "scene/sub_scene/sub_box_renamed"]
               (sort (keys (g/node-value pasted-pie :id-counts)))))
        (g/set-property! sub-box :id "sub_box"))

      ;; Cut "scene/sub_scene/sub_box" and paste it below "Nodes" in the root gui.
      (is (= "scene/sub_scene" (:label (outline root [0 0 1]))))
      (is (= 1 (child-count root [0 0 1])))
      (cut! root [0 0 1 0])
      (is (= 0 (child-count root [0 0 1])))

      (is (= "Nodes" (:label (outline root [0]))))
      (is (nil? (outline-labeled root "sub_box")))
      (is (= 1 (child-count root [0])))
      (paste! project app-view root [0])
      (is (= 2 (child-count root [0])))
      (is (some? (outline-labeled root "sub_box")))

      ;; Verify connections.
      (let [pasted-sub-box (:node-id (outline root [0 1]))
            pasted-pie (:node-id (outline root [0 1 0]))
            sibling-scene (:node-id (outline root [0 0]))]
        (is (false? (g/override? pasted-sub-box)))
        (is (= "sub_box" (g/node-value pasted-sub-box :id)))
        (is (= "pie" (g/node-value pasted-pie :id)))
        (is (= "scene" (g/node-value sibling-scene :id)))
        (is (= ["pie"
                "scene"
                "sub_box"]
               (sort (keys (g/node-value pasted-sub-box :id-counts)))))
        (g/set-property! sibling-scene :id "scene_renamed")
        (g/set-property! pasted-sub-box :id "sub_box_renamed")
        (is (= ["pie"
                "scene_renamed"
                "sub_box_renamed"]
               (sort (keys (g/node-value pasted-sub-box :id-counts)))))))))

(deftest cut-paste-multiple
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/collection/go_hierarchy.collection")]
      ; Original tree
      ; Collection
      ; + left (go)
      ;   + left_child (go)
      ; + right (go)
      ;   + right_child (go)
      (is (= 2 (child-count root)))
      (testing "Cut `left_child` and `right`"
        (is (true? (cut? root [0 0] [1])))
        (cut! root [0 0] [1])
        (is (= 0 (child-count root [0])))
        (is (= 1 (child-count root))))
      (testing "Paste `left_child` and `right` below `root`"
        (paste! project app-view root)
        (is (= 3 (child-count root)))))))

(deftest cut-disallowed-multiple
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/game_object/sprite_with_collision.go")]
      ; Original tree
      ; Game Object
      ; + collisionobject
      ;   + Sphere (shape)
      ;   + Box (shape)
      ;   + Capsule (shape)
      ; + sprite
      (is (= 2 (child-count root)))
      (testing "Cut is disallowed when both `Capsule` and `sprite` are selected"
        (is (false? (cut? root [0 2] [1])))))))

(defn- handler-run [command app-view selection user-data]
  (test-util/handler-run command [{:name :workbench :env {:selection selection :app-view app-view}}] user-data))

(defn- add-collision-shape [app-view collision-object shape-type]
  (handler-run :add app-view [collision-object] {:shape-type shape-type}))

(deftest dnd-collision-shape
  (test-util/with-loaded-project
    (testing "dnd between two embedded"
             (let [root (test-util/resource-node project "/logic/one_embedded.go")
                   collision-object (-> (test-util/outline root [0]) :alt-outline :node-id)]
               ; Original tree:
               ; Game Object
               ; + collisionobject
               (copy-paste! project app-view root [0])
               (add-collision-shape app-view collision-object :type-sphere)
               ; Game Object
               ; + collisionobject
               ;   + sphere
               ; + collisionobject1
               (is (= 1 (child-count root [0])))
               (is (= 0 (child-count root [1])))
               (drag! root [0 0])
               (drop! project app-view root [1])
               ; Game Object
               ; + collisionobject
               ; + collisionobject1
               ;   + sphere
               (is (= 0 (child-count root [0])))
               (is (= 1 (child-count root [1])))))
    (testing "dnd between two references of the same file"
             (let [root (test-util/resource-node project "/game_object/sprite_with_collision.go")]
               ; Original tree:
               ; Game Object
               ; + collisionobject - ref
               ;   + Sphere (shape)
               ;   + Box (shape)
               ;   + Capsule (shape)
               ; + sprite
               (copy-paste! project app-view root [0])
               ; Current tree:
               ; Game Object
               ; + collisionobject - ref
               ;   + Sphere (shape)
               ;   + Box (shape)
               ;   + Capsule (shape)
               ; + collisionobject1 - ref
               ;   + Sphere (shape)
               ;   + Box (shape)
               ;   + Capsule (shape)
               ; + sprite
               (drag! root [0 0])
               ;; Not possible to drag to the second collisionobject1 since they are references to the same file
               (is (not (drop? project root [1])))))))

(deftest alt-outlines
  (test-util/with-loaded-project
    (doseq [root (map #(test-util/resource-node project %) [;; Contains both embedded and referenced components
                                                            "/logic/main.go"
                                                            ;; Contains referenced sub collections
                                                            "/collection/sub_defaults.collection"
                                                            ;; Contains both embedded and referenced game objects
                                                            "/logic/hierarchy.collection"])]
      (let [children (-> (g/node-value root :node-outline) :children)
            node-ids (map :node-id children)
            alt-node-ids (map (comp :node-id :alt-outline) children)]
        (is (every? (fn [[nid alt]] (or (nil? alt) (and nid (not= nid alt)))) (map vector node-ids alt-node-ids)))))))

(deftest add-pfx-emitter-modifier
  (test-util/with-loaded-project
    (let [pfx (test-util/open-tab! project app-view "/particlefx/blob.particlefx")
          children-fn (fn [] (mapv :label (:children (test-util/outline pfx []))))]
      (is (= ["emitter" "Acceleration"] (children-fn)))
      ;; Add emitter through command
      (handler-run :add app-view [pfx] {:emitter-type :emitter-type-circle})
      (is (= ["emitter" "emitter1" "Acceleration"] (children-fn)))
      ;; Copy-paste 'emitter'
      (copy-paste! project app-view pfx [0])
      (is (= ["emitter" "emitter1" "emitter2" "Acceleration"] (children-fn)))
      ;; Add modifier through command
      (handler-run :add-secondary app-view [pfx] {:modifier-type :modifier-type-acceleration})
      (is (= ["emitter" "emitter1" "emitter2" "Acceleration" "Acceleration"] (children-fn)))
      ;; Copy-paste 'Acceleration'
      (copy-paste! project app-view pfx [3])
      (is (= ["emitter" "emitter1" "emitter2" "Acceleration" "Acceleration" "Acceleration"] (children-fn))))))

(deftest add-to-referenced-collection
  (test-util/with-loaded-project
    (let [root (test-util/resource-node project "/collection/sub_sub_props.collection")
          run-handler! (fn [command user-data root path]
                        (let [parent (:node-id (outline root path))
                              env {:app-view app-view :project project :selection [parent] :workspace workspace}]
                          (test-util/handler-run command [{:env env :name :workbench}] user-data)))
          add-game-object-to-collection! (partial run-handler! :add nil)
          add-child-game-object! (partial run-handler! :add-secondary nil)
          sub-props-collection (test-util/resource-node project "/collection/sub_props.collection")
          props-collection (test-util/resource-node project "/collection/props.collection")]
      ;; Original tree
      ;; Collection (collection "/collection/sub_sub_props.collection")
      ;; + sub_props (collection "/collection/sub_props.collection")
      ;;   + props (collection "/collection/props.collection")
      ;;     + props (game_object "/game_object/props.go")
      ;;       + script (script "/script/props.script")
      ;;     + props_embedded (game-object)
      ;;       + script (script "/script/props.script")

      ;; Add game object to "sub_props" collection.
      (is (= "sub_props" (:label (outline root [0]))))
      (is (= 1 (child-count sub-props-collection)))
      (is (= 1 (child-count root [0])))
      (add-game-object-to-collection! root [0])
      (is (= 2 (child-count root [0])))
      (is (= 2 (child-count sub-props-collection)))

      ;; Verify connections.
      (let [added-game-object (:node-id (outline root [0 1]))
            sibling-coll-instance (:node-id (outline root [0 0]))]
        (is (g/override? added-game-object))
        (is (= "go" (g/node-value added-game-object :id)))
        (is (= "props" (g/node-value sibling-coll-instance :id)))
        (is (= ["go"
                "props"]
               (sort (keys (g/node-value added-game-object :id-counts)))))
        (g/set-property! sibling-coll-instance :id "props_renamed")
        (is (= ["go"
                "props_renamed"]
               (sort (keys (g/node-value added-game-object :id-counts)))))
        (g/set-property! added-game-object :id "added_renamed")
        (is (= ["added_renamed"
                "props_renamed"]
               (sort (keys (g/node-value sibling-coll-instance :id-counts)))))
        (g/set-property! sibling-coll-instance :id "props"))

      ;; Add game object to "props" collection.
      (is (= "props" (:label (outline root [0 0]))))
      (is (= 2 (child-count props-collection)))
      (is (= 2 (child-count root [0 0])))
      (add-game-object-to-collection! root [0 0])
      (is (= 3 (child-count root [0 0])))
      (is (= 3 (child-count props-collection)))

      ;; Verify connections.
      (let [added-game-object (:node-id (outline root [0 0 0]))
            sibling-game-object (:node-id (outline root [0 0 1]))]
        (is (g/override? added-game-object))
        (is (= "go" (g/node-value added-game-object :id)))
        (is (= "props" (g/node-value sibling-game-object :id)))
        (is (= ["go"
                "props"
                "props_embedded"]
               (sort (keys (g/node-value added-game-object :id-counts)))))
        (g/set-property! sibling-game-object :id "props_renamed")
        (is (= ["go"
                "props_embedded"
                "props_renamed"]
               (sort (keys (g/node-value added-game-object :id-counts)))))
        (g/set-property! added-game-object :id "added_renamed")
        (is (= ["added_renamed"
                "props_embedded"
                "props_renamed"]
               (sort (keys (g/node-value sibling-game-object :id-counts)))))
        (g/set-property! sibling-game-object :id "props"))

      ;; Add game object instance to "props" game object.
      (is (= "props" (:label (outline root [0 0 1]))))
      (is (= 3 (count (keys (g/node-value props-collection :go-inst-ids)))))
      (is (= 1 (child-count root [0 0 1])))
      (add-child-game-object! root [0 0 1])
      (is (= 2 (child-count root [0 0 1])))
      (is (= 4 (count (keys (g/node-value props-collection :go-inst-ids))))))))

(deftest resolve-id-test
  (testing "Single ids"
    (are [expected-id candidate-id taken-ids]
      (do (is (= expected-id (outline/resolve-id candidate-id taken-ids)))
          (is (= [expected-id] (outline/resolve-ids [candidate-id] taken-ids))))

      "sprite" "sprite" #{""}
      "sprite2" "sprite2" #{"sprite"}
      "sprite1" "sprite" #{"sprite"}
      "sprite2" "sprite" #{"sprite" "sprite1"}
      "sprite2" "sprite1" #{"sprite" "sprite1"}
      "sprite1" "sprite" #{"sprite" "sprite2"}
      "sprite" "sprite1" #{"sprite1" "sprite2"}))

  (testing "Multiple ids"
    (is (= ["sprite" "sprite1" "sprite2"]
           (outline/resolve-ids ["sprite" "sprite" "sprite"] #{})))

    (is (= ["sprite3" "sprite4" "sprite5"]
           (outline/resolve-ids ["sprite" "sprite" "sprite"] #{"sprite" "sprite1" "sprite2"})))

    (is (= ["sprite3" "sprite4" "sprite5"]
           (outline/resolve-ids ["sprite1" "sprite2" "sprite3"] #{"sprite" "sprite1" "sprite2"})))))
