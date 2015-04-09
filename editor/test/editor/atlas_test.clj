(ns editor.atlas-test
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test :refer :all]
            [dynamo.file :as file]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer [with-clean-system tempfile]]
            [dynamo.image :as image]
            [dynamo.types :as t]
            [editor.atlas :as atlas]
            [editor.core :as core]
            [editor.project :as p])
  (:import [dynamo.types Image]
           [javax.imageio ImageIO]))

(def ident (gen/not-empty gen/string-alpha-numeric))

(def image-name (gen/fmap (fn [ns] (str (str/join \/ ns) ".png")) (gen/not-empty (gen/vector ident))))

(def image (gen/fmap #(format "images{\nimage: \"%s\"\n}"  %) image-name))

(def animation-name ident)
(def fps (gen/such-that #(< 0 % 60) gen/pos-int))
(def flip (gen/frequency [[9 (gen/return 0)] [1 (gen/return 1)]]))
(def playback (gen/elements ["PLAYBACK_NONE"
                             "PLAYBACK_ONCE_FORWARD"
                             "PLAYBACK_ONCE_BACKWARD"
                             "PLAYBACK_ONCE_PINGPONG"
                             "PLAYBACK_LOOP_FORWARD"
                             "PLAYBACK_LOOP_BACKWARD"
                             "PLAYBACK_LOOP_PINGPONG"]))

(def animation (gen/fmap (fn [[id imgs fps flip-horiz flip-vert playback]]
                           (format "animations {\nid: \"%s\"\n%s\nfps: %d\nflip_horizontal: %d\nflip_vertical: %d\nplayback: %s\n}"
                             id (str/join \newline imgs) fps flip-horiz flip-vert playback))
                 (gen/tuple animation-name (gen/not-empty (gen/vector image)) fps flip flip playback )))

(def atlas (gen/fmap (fn [[margin borders animations images]]
                       (format "%s\n%s\nmargin: %d\nextrude_borders: %d\n"
                         (str/join \newline images) (str/join \newline animations) margin borders))
             (gen/tuple gen/pos-int gen/pos-int (gen/vector animation) (gen/vector image))))

(defn atlas-tempfile [contents]
  (let [f (tempfile "temp" ".atlas" true)]
    (spit f contents)
    (file/native-path (.getPath f))))

(defn <-text
  [project-node text-format]
  (let [atlas  (some-> (g/make-node (g/node->graph-id project-node) atlas/AtlasNode :filename (atlas-tempfile text-format))
                       g/transact
                       g/tx-nodes-added
                       first)]
    (g/transact (g/connect atlas :self project-node :nodes))
    #_(g/process-one-event atlas {:type :load :project project-node})
    atlas))

(g/defnode WildcardImageResourceNode
  (inherits core/OutlineNode)
  (output outline-label t/Str (g/fnk [filename] (t/local-name filename)))
  (property filename (t/protocol t/PathManipulation) (visible false))
  (output content Image :cached (g/fnk [filename] (assoc image/placeholder-image :path (t/local-path filename)))))

(defn test-project
  [world image-resource-node-type]
  (let [project-node (some-> (g/make-node world p/Project) g/transact g/tx-nodes-added first)
        ppath        (fn [p] (file/make-project-path project-node p))]
    (g/make-nodes
     world
     [frame-01 [image-resource-node-type :filename (ppath "images/frame-01.png")]
      frame-02 [image-resource-node-type :filename (ppath "images/frame-02.png")]
      frame-03 [image-resource-node-type :filename (ppath "images/frame-03.png")]
      small    [image-resource-node-type :filename (ppath "images/small.png")]
      large    [image-resource-node-type :filename (ppath "images/large.png")]]
     (g/connect frame-01 :self project-node :nodes)
     (g/connect frame-02 :self project-node :nodes)
     (g/connect frame-03 :self project-node :nodes)
     (g/connect small    :self project-node :nodes)
     (g/connect large    :self project-node :nodes))
    project-node))

(defn verify-atlas-text-format [fixture-basename]
  (with-clean-system
    (let [project-node (test-project world WildcardImageResourceNode)
          atlas-path   (str fixture-basename ".atlas")
          atlas-text   (slurp (io/resource atlas-path))
          atlas        (<-text project-node atlas-text)]
      (is (= atlas-text (g/node-value atlas :text-format))
          (str "Fixture " atlas-path " did not round-trip through protobuf text format")))))

(deftest text-format-output-matches-input
  (verify-atlas-text-format "atlases/empty")
  (verify-atlas-text-format "atlases/single-image")
  (verify-atlas-text-format "atlases/single-animation")
  (verify-atlas-text-format "atlases/empty-animation")
  (verify-atlas-text-format "atlases/complex")
  (verify-atlas-text-format "atlases/single-image-multiple-references")
  (verify-atlas-text-format "atlases/single-image-multiple-references-in-animation")
  ;; TODO: [#87948214] fails when placeholder image is used
  #_(verify-atlas-text-format "atlases/missing-image")
  #_(verify-atlas-text-format "atlases/missing-image-in-animation")
  #_(verify-atlas-text-format "atlases/missing-image-multiple-references")
  #_(verify-atlas-text-format "atlases/missing-image-multiple-references-in-animation"))

(defn ->text
  [atlas]
  (g/node-value atlas :text-format))

(defn round-trip
  [random-atlas]
  (with-clean-system
    (let [project-node (test-project world WildcardImageResourceNode)
          first-gen    (->text (<-text project-node random-atlas))
          second-gen   (->text (<-text project-node first-gen))]
      (= first-gen second-gen))))

#_(defspec round-trip-preserves-fidelity
  10
  (prop/for-all* [atlas] round-trip))

(defn simple-outline [outline-tree]
  [(:label outline-tree) (mapv simple-outline (:children outline-tree))])

#_(deftest outline
  (with-clean-system
    (let [project-node (test-project world WildcardImageResourceNode)
          atlas-text   (slurp (io/resource "atlases/complex.atlas"))
          atlas        (<-text project-node atlas-text)]
      (is (= ["Atlas" [["frame-01.png" []]
                       ["frame-02.png" []]
                       ["small.png" []]
                       ["large.png" []]
                       ["anim1" [["frame-01.png" []]]]
                       ["anim2" [["frame-02.png" []]]]
                       ["anim3" [["frame-03.png" []]]]
                       ["anim4" [["frame-01.png" []]
                                 ["frame-02.png" []]
                                 ["frame-03.png" []]]]]]
             (simple-outline (g/node-value atlas :outline-tree))))))
  #_(with-clean-system
    (let [project-node (test-project world WildcardImageResourceNode)
          atlas-text   (slurp (io/resource "atlases/single-animation.atlas"))
          atlas        (<-text project-node atlas-text)
          anim1        (ffirst (g/sources now atlas :animations))
          img-frame-01 (t/lookup project-node "images/frame-01.png")
          img-frame-02 (t/lookup project-node "images/frame-02.png")

          ppath        (fn [p] (file/make-project-path project-node p))

          ;; initial load
          outline1     (g/node-value atlas :outline-tree)

          ;; disconnect image from anim
          _            (g/transact (g/disconnect img-frame-02 :content anim1 :images))
          outline2     (g/node-value atlas :outline-tree)

          ;; disconnect anim
          _            (g/transact (g/disconnect anim1 :animation atlas :animations))
          outline3     (g/node-value atlas :outline-tree)

          ;; connect existing image
          _            (g/transact (g/connect img-frame-01 :content atlas :images))
          outline4     (g/node-value atlas :outline-tree)

          ;; disconnect image
          _            (g/transact (g/disconnect img-frame-01 :content atlas :images))
          outline5     (g/node-value atlas :outline-tree)

          ;; add anim
          anim2        (some-> (g/make-node world atlas/AnimationGroupNode :id "anim2")
                               g/transact
                               g/tx-nodes-added
                               first)
          _            (g/transact (g/connect anim2 :animation atlas :animations))
          outline6     (g/node-value atlas :outline-tree)

          ;; connect image to anim
          img-small    (some-> (t/node-for-path project-node (ppath "/images/small.png"))
                               g/transact
                               g/tx-nodes-added
                               first)
          _            (g/transact (g/connect img-small    :content anim2 :images))
          _            (g/transact (g/connect img-frame-01 :content anim2 :images))
          outline7     (g/node-value atlas :outline-tree)

          ;; connect missing (placeholder) image
          img-missing  (some-> (t/node-for-path project-node (ppath "/images/missing.png"))
                               g/transact
                               g/tx-nodes-added
                               first)
          _            (g/transact (g/connect img-missing :content anim2 :images))
          outline8     (g/node-value atlas :outline-tree)

          ;; connect another missing (placeholder) image
          img-missing2 (some-> (t/node-for-path project-node (ppath "/images/missing2.png"))
                               g/transact
                               g/tx-nodes-added
                               first)
          _            (g/transact (g/connect img-missing2 :content anim2 :images))
          outline9     (g/node-value atlas :outline-tree)

          ;; disconnect placeholder
          _            (g/transact (g/disconnect img-missing :content anim2 :images))
          outline10    (g/node-value atlas :outline-tree)

          ;; connect duplicate existing image
          _            (g/transact (g/connect img-frame-01 :content anim2 :images))
          outline11    (g/node-value atlas :outline-tree)

          ;; disconnect duplicate existing image
          _            (g/transact (g/disconnect img-frame-01 :content anim2 :images))
          outline12    (g/node-value atlas :outline-tree)

          ;; connect duplicate missing (placeholder) image
          _            (g/transact (g/connect img-missing2 :content anim2 :images))
          outline13    (g/node-value atlas :outline-tree)

          ;; disconnect duplicate missing (placeholder) image
          _            (g/transact (g/disconnect img-missing2 :content anim2 :images))
          outline14    (g/node-value atlas :outline-tree)]
      (are [outline-tree expected] (= expected (simple-outline outline-tree))
           outline1  ["Atlas" [["anim1" [["frame-01.png" []] ["frame-02.png" []] ["frame-03.png" []]]]]]
           outline2  ["Atlas" [["anim1" [["frame-01.png" []] ["frame-03.png" []]]]]]
           outline3  ["Atlas" []]
           outline4  ["Atlas" [["frame-01.png" []]]]
           outline5  ["Atlas" []]
           outline6  ["Atlas" [["anim2" []]]]
           outline7  ["Atlas" [["anim2" [["small.png" []] ["frame-01.png" []]]]]]
           outline8  ["Atlas" [["anim2" [["small.png" []] ["frame-01.png" []] ["missing.png" []]]]]]
           outline9  ["Atlas" [["anim2" [["small.png" []] ["frame-01.png" []] ["missing.png" []] ["missing2.png" []]]]]]
           outline10 ["Atlas" [["anim2" [["small.png" []] ["frame-01.png" []] ["missing2.png" []]]]]]
           outline11 ["Atlas" [["anim2" [["small.png" []] ["frame-01.png" []] ["missing2.png" []] ["frame-01.png" []]]]]]
           outline12 ["Atlas" [["anim2" [["small.png" []] ["frame-01.png" []] ["missing2.png" []]]]]]
           outline13 ["Atlas" [["anim2" [["small.png" []] ["frame-01.png" []] ["missing2.png" []] ["missing2.png" []]]]]]
           outline14 ["Atlas" [["anim2" [["small.png" []] ["frame-01.png" []] ["missing2.png" []]]]]]))))
