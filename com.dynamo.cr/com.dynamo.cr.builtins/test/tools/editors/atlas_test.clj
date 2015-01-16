(ns editors.atlas-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [dynamo.file :as file]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-world]]
            [dynamo.types :as t]
            [schema.test]
            [editors.atlas :as atlas]
            [editors.image-node :as image])
  (:import [com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage]
           [java.io StringReader]))

(use-fixtures :once schema.test/validate-schemas)

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

(defn <-text
  [text-format]
  (ds/transactional
    (let [locator (reify t/NamingContext (lookup [this name] (ds/add (n/construct image/ImageResourceNode :filename name))))
          atlas   (ds/add (n/construct atlas/AtlasNode))]
      (atlas/construct-ancillary-nodes atlas locator (StringReader. text-format))
      atlas)))

(defn ->text
  [atlas]
  (n/get-node-value atlas :text-format))

(defn round-trip
  [random-atlas]
  (with-clean-world
    (let [first-gen (->text (<-text random-atlas))
          second-gen (->text (<-text first-gen))]
      (= first-gen second-gen))))

(defspec round-trip-preserves-fidelity
  10
  (prop/for-all* [atlas] round-trip))
