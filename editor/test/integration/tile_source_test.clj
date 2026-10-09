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

(ns integration.tile-source-test
  (:require [cljfx.api :as fx]
            [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.attachment :as attachment]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.properties :as properties]
            [editor.properties-view :as properties-view]
            [editor.protobuf :as protobuf]
            [editor.texture-util :as texture-util]
            [editor.tile-source :as tile-source]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.coll :as coll])
  (:import [com.dynamo.gamesys.proto TextureSetProto$TextureSet Tile$TileSet]
           [java.awt.image BufferedImage]
           [java.io File]
           [javafx.scene.control TextField]
           [javafx.scene.input KeyCode KeyEvent]
           [javax.imageio ImageIO]))

(defn- vertex-buffer->vertices
  [vertex-buffer]
  (mapv #(get vertex-buffer %) (range (count vertex-buffer))))

(defn- quad-triangles?
  [vertices]
  (and (= 6 (count vertices))
       (= (nth vertices 2) (nth vertices 3))
       (= (nth vertices 0) (nth vertices 5))))

(defn- quad-lines?
  [vertices]
  (and (= 8 (count vertices))
       (= (nth vertices 0) (nth vertices 7))
       (= (nth vertices 1) (nth vertices 2))
       (= (nth vertices 3) (nth vertices 4))
       (= (nth vertices 5) (nth vertices 6))))

(deftest tile-source-quad-producers-use-core-topologies
  (let [tile-source-attributes {:width 2
                                :height 3
                                :tiles-per-column 1
                                :tiles-per-row 1}]
    (testing "tiles"
      (let [vertices (-> (tile-source/gen-tiles-vbuf tile-source-attributes [nil] [1.0 1.0])
                         (vertex-buffer->vertices))]
        (is (quad-triangles? vertices))
        (is (= [[3.0 3.0 0.0 0.0 1.0]
                [3.0 6.0 0.0 0.0 0.0]
                [5.0 6.0 0.0 1.0 0.0]
                [5.0 6.0 0.0 1.0 0.0]
                [5.0 3.0 0.0 1.0 1.0]
                [3.0 3.0 0.0 0.0 1.0]]
               vertices))))

    (testing "collision overlays"
      (let [vertices (-> (tile-source/gen-tile-outlines-vbuf tile-source-attributes [nil] [1.0 1.0])
                         (vertex-buffer->vertices))]
        (is (quad-lines? vertices))
        (is (= (repeat 8 (vec (repeat 4 (float 0.15))))
               (map #(subvec % 3) vertices)))))))

(deftest tile-source-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/tilesource/valid.tilesource")]
      (testing "image dim error"
        (test-util/with-prop [node-id :collision (workspace/resolve-workspace-resource workspace "/graphics/paddle.png")]
          (is (some? (test-util/prop-error node-id :image)))
          (is (= (test-util/prop-error node-id :image)
                 (test-util/prop-error node-id :collision)))))
      (testing "tile dim error"
        (test-util/with-prop [node-id :tile-width 5000]
          (is (some? (test-util/prop-error node-id :image)))
          (is (= (test-util/prop-error node-id :image)
                 (test-util/prop-error node-id :collision)
                 (test-util/prop-error node-id :tile-width)
                 (test-util/prop-error node-id :tile-margin)))))
      (testing "save and build data errors"
        (test-util/with-prop [node-id :tile-width -1]
          (is (g/error? (g/node-value node-id :build-targets)))
          (is (not (g/error? (g/node-value node-id :save-data)))))))))

(defn- add-collision-group! [app-view node-id]
  (tile-source/add-collision-group-node! node-id (fn [node-ids] (app-view/select app-view node-ids)))
  (first (g/node-value app-view :selected-node-ids)))

(defn- add-animation! [app-view node-id]
  (tile-source/add-animation-node! node-id (fn [node-ids] (app-view/select app-view node-ids)))
  (first (g/node-value app-view :selected-node-ids)))

(deftest collision-group-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")]
      (app-view/select! app-view [node-id])
      (testing "collision-group-id"
        (let [group (add-collision-group! app-view node-id)]
          (test-util/with-prop [group :id ""]
            (is (g/error? (test-util/prop-error group :id))))))
      (testing "collision-group-max"
        (let [groups (mapv (fn [_] (add-collision-group! app-view node-id)) (range 17))
              project-error (g/flatten-errors (g/node-value project :build-errors))]
          (is (g/error-warning? project-error))
          (is (coll/every? nil? (mapv #(test-util/prop-error % :id) groups)))
          (g/transact
            (g/delete-nodes groups))
          (is (nil? (g/flatten-errors (g/node-value project :build-errors)))))))))

(deftest animation-validation
  (test-util/with-loaded-project
    (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")]
      (app-view/select! app-view [node-id])
      (testing "animation-id"
        (let [anim (add-animation! app-view node-id)]
          (test-util/with-prop [anim :id ""]
            (is (some? (test-util/prop-error anim :id))))))
      (testing "animation-frames"
        (let [anim (add-animation! app-view node-id)]
          (doseq [frames [[] [0] [2000]]]
            (test-util/with-prop [anim :frames frames]
              (is (g/error? (test-util/prop-error anim :frames)))
              (is (g/error? (g/node-value node-id :build-targets)))
              (is (not (g/error? (g/node-value node-id :save-data))))))
          (test-util/check-thrown-with-root-cause-msg! #"Invalid animation frames"
                                                       (test-util/prop! anim :frames "1, invalid"))
          (is (= [1] (g/node-value anim :frames)))
          (is (not (g/error? (g/node-value node-id :save-data)))))))))

;; Frame input must ignore empty entries, preserve order and duplicates, and reject malformed or unbounded input without changing the model.
(deftest animation-frame-input
  (test-util/with-loaded-project
    (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")
          anim (add-animation! app-view node-id)]
      (doseq [[text expected] [["1, 3, 6, 8, 3" [1 3 6 8 3]]
                               ["1-4, 6, 8" [1 2 3 4 6 8]]
                               [" 4 - 2, 2 " [4 3 2 2]]
                               ["1, 3," [1 3]]
                               ["1,,3" [1 3]]
                               [" 1-3, , 8 " [1 2 3 8]]
                               [", 4-2, 2, ," [4 3 2 2]]
                               ["" []]
                               [", , ," []]
                               ["1-1" [1]]]]
        (testing text
          (properties/set-values!
            (get-in (properties/coalesce [(g/node-value anim :_properties)]) [:properties :frames])
            [text])
          (is (= expected (test-util/prop anim :frames)))
          (if (coll/empty? expected)
            (is (g/error? (test-util/prop-error anim :frames)))
            (is (nil? (test-util/prop-error anim :frames))))
          (is (not (g/error? (g/node-value node-id :save-data))))))
      (doseq [text ["a"
                    "1.5"
                    "-1"
                    "1-"
                    "1--3"
                    "2147483648"
                    "4294967295"
                    "1-4294967295"
                    "999999999999999999999"
                    "1-2147483647"
                    "1-600000,1-600000"]]
        (testing text
          (test-util/check-thrown-with-root-cause-msg! #"Invalid animation frames"
                                                       (properties/set-values!
                                                         (get-in (properties/coalesce [(g/node-value anim :_properties)]) [:properties :frames])
                                                         [text]))
          (is (= [1] (g/node-value anim :frames)))
          (is (nil? (test-util/prop-error anim :frames)))
          (is (not (g/error? (g/node-value node-id :save-data)))))))))

;; Frames must filter typed and pasted characters without preventing incomplete input, deletion, or ordinary string edits.
(deftest animation-frame-field-input-filter
  @(fx/on-fx-thread
     (test-util/with-loaded-project
       (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")
             anim (add-animation! app-view node-id)]
         (doseq [prop-kw [:frames :id]]
           (let [property
                 (get-in (properties/coalesce [(g/node-value anim :_properties)]) [:properties prop-kw])

                 component
                 (fx/create-component (properties-view/make-control-view property {} @test-util/localization))

                 component
                 (fx/advance-component component (properties-view/make-control-view property {} @test-util/localization))

                 ^TextField field (fx/instance component)]
             (try
               (if (= :id prop-kw)
                 (do
                   (.replaceText field 0 (.getLength field) "walk/run.2")
                   (is (= "walk/run.2" (.getText field))))
                 (do
                   (doseq [text ["0123456789" " " "," "-" "1-" "1,,2" "4 - 2, 2"]]
                     (.replaceText field 0 (.getLength field) text)
                     (is (= text (.getText field))))
                   (.replaceText field 0 (.getLength field) "1-3, 8")
                   (doseq [text ["a" "." "+" "/" "é" "١" "１"]]
                     (testing text
                       (.appendText field text)
                       (is (= "1-3, 8" (.getText field)))))
                   (.selectAll field)
                   (.replaceSelection field "1, invalid, 2")
                   (is (= "1-3, 8" (.getText field)))
                   (.selectAll field)
                   (.replaceSelection field "4 - 2, 2")
                   (is (= "4 - 2, 2" (.getText field)))
                   (.deleteText field 0 (.getLength field))
                   (is (= "" (.getText field)))))
               (finally
                 (fx/delete-component component)))))))))

;; Model updates must stay visible in an existing Frames field without disabling filtering or preventing correction.
(deftest animation-frame-field-model-updates
  @(fx/on-fx-thread
     (test-util/with-loaded-project
       (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")
             anim (add-animation! app-view node-id)

             control-description
             (fn []
               (let [property (get-in (properties/coalesce [(g/node-value anim :_properties)]) [:properties :frames])]
                 (properties-view/make-control-view property {} @test-util/localization)))

             component (volatile! (fx/create-component (control-description)))
             ^TextField field (fx/instance @component)]
         (try
           (testing "out-of-range model values remain visible"
             (test-util/prop! anim :frames [0 9])
             (vswap! component fx/advance-component (control-description))
             (is (identical? field (fx/instance @component)))
             (is (= "0, 9" (.getText field)))
             (is (g/error? (test-util/prop-error anim :frames)))
             (.appendText field "x")
             (is (= "0, 9" (.getText field))))
           (testing "user input can correct the same field"
             (test-util/set-control-value! field "4, 2, 4")
             (vswap! component fx/advance-component (control-description))
             (is (identical? field (fx/instance @component)))
             (is (= [4 2 4] (g/node-value anim :frames)))
             (is (= "4, 2, 4" (.getText field)))
             (is (nil? (test-util/prop-error anim :frames))))
           (testing "subsequent model changes keep filtering active"
             (test-util/prop! anim :frames [3 1])
             (vswap! component fx/advance-component (control-description))
             (is (identical? field (fx/instance @component)))
             (is (= "3, 1" (.getText field)))
             (.appendText field "x")
             (is (= "3, 1" (.getText field))))
           (finally
             (fx/delete-component @component)))))))

;; Empty and out-of-range sequences must remain saveable, fail the build at Frames, and recover when corrected.
(deftest animation-frame-field-build-errors
  @(fx/on-fx-thread
     (test-util/with-scratch-project test-util/project-path
       (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")
             anim (add-animation! app-view node-id)]
         (doseq [text ["" ", ," "0" "9" "1-10" "1, 3,, 1,"]]
           (testing text
             (let [property
                   (get-in (properties/coalesce [(g/node-value anim :_properties)]) [:properties :frames])

                   component
                   (fx/create-component (properties-view/make-control-view property {} @test-util/localization))]
               (try
                 (test-util/set-control-value! (fx/instance component) text)
                 (finally
                   (fx/delete-component component))))
             (is (not (g/error? (g/node-value node-id :save-data))))
             (if (= "1, 3,, 1," text)
               (do
                 (is (= [1 3 1] (g/node-value anim :frames)))
                 (is (nil? (test-util/prop-error anim :frames)))
                 (with-open [_ (test-util/build! node-id)]
                   (is (test-util/built-pb node-id TextureSetProto$TextureSet))))
               (let [build-error (test-util/build-error! node-id)
                     causes (into [] (coll/tree-xf :causes :causes) [build-error])]
                 (is (g/error? (test-util/prop-error anim :frames)))
                 (is (g/error? build-error))
                 (is (coll/any? #(and (= anim (:_node-id %))
                                      (= :frames (:_label %)))
                                causes))))))))))

;; Malformed field commits must retain the last sequence and keep unrelated edits dirty, saveable, and intact after reload.
(deftest animation-frame-malformed-input-preserves-saving
  @(fx/on-fx-thread
     (test-util/with-scratch-project "test/resources/image_project"
       (let [node-id (test-util/resource-node project "/main/main.tilesource")
             anim (:node-id (first (g/node-value node-id :animation-data)))
             saved-project-path (str (workspace/project-directory workspace))]
         (test-util/prop! anim :frames [1 1])
         (test-util/prop! anim :fps 12)
         (let [property
               (get-in (properties/coalesce [(g/node-value anim :_properties)]) [:properties :frames])

               rejected (atom 0)

               description
               (assoc (properties-view/make-control-view property {} @test-util/localization)
                 :on-invalid-value (fn [_] (swap! rejected inc)))

               component (volatile! (fx/create-component description))
               ^TextField field (fx/instance @component)]
           (try
             (doseq [[^long index text]
                     (into []
                           (map-indexed vector)
                           ["-" "-1" "1-" "1--3" "2147483648" "1-2147483647" "1-600000,1-600000"])]
               (testing text
                 (test-util/set-control-value! field text)
                 ;; Render pending local state between simulated user events.
                 (vswap! component fx/advance-component description)
                 (is (= (inc index) @rejected))
                 (is (= text (.getText field)))
                 (is (= [1 1] (g/node-value anim :frames)))
                 (is (:dirty (g/node-value node-id :save-data)))
                 (is (coll/any? #(= node-id (:node-id %)) (project/dirty-save-data project)))))
             (test-util/save-project! project)
             (is (false? (:dirty (g/node-value node-id :save-data))))
             (with-open [reader (io/reader (g/node-value node-id :resource))]
               (is (= [{:id "diamond"
                        :frames [1 1]
                        :fps 12}]
                      (:animations (protobuf/read-map-without-defaults Tile$TileSet reader)))))
             (.fireEvent field (KeyEvent. field field KeyEvent/KEY_PRESSED "" "" KeyCode/ESCAPE false false false false))
             (vswap! component fx/advance-component description)
             (is (= "1, 1" (.getText field)))
             (finally
               (fx/delete-component @component))))
         (test-util/with-loaded-project saved-project-path
           (let [reloaded (test-util/resource-node project "/main/main.tilesource")]
             (is (= [{:id "diamond"
                      :frames [1 1]
                      :fps 12}]
                    (:animations (g/node-value reloaded :save-value))))))))))

;; Loading migrates inclusive and wrapped legacy ranges silently and retains ranges that cannot yet be resolved.
(deftest animation-frame-migration
  (test-util/with-scratch-project test-util/project-path
    (let [node-id (test-util/resource-node project "/tilesource/valid.tilesource")
          tile-count (g/node-value node-id :tile-count)
          tile-source (select-keys (g/node-value node-id :save-value) [:image :tile-width :tile-height])
          legacy {:id "legacy"
                  :start-tile 2
                  :end-tile 4}
          cases [["/tilesource/inclusive.tilesource"
                  {:animations [legacy]}
                  [{:id "legacy"
                    :frames [2 3 4]}]]
                 ["/tilesource/wrapped.tilesource"
                  {:animations [{:id "legacy"
                                 :start-tile tile-count
                                 :end-tile 2}]}
                  [{:id "legacy"
                    :frames [tile-count 1 2]}]]
                 ["/tilesource/explicit.tilesource"
                  {:animations [(assoc legacy :frames [3 1 3])]}
                  [{:id "legacy"
                    :frames [3 1 3]}]]
                 ["/tilesource/missing-image.tilesource"
                  {:image "/missing.png"
                   :animations [legacy]}
                  [legacy]]]]
      (doseq [[proj-path overrides _expected] cases]
        (test-util/write-file-resource! workspace proj-path (merge tile-source overrides)))
      (workspace/resource-sync! workspace)
      (test-util/clear-cached-save-data! project)
      (doseq [[proj-path _overrides expected] cases]
        (testing proj-path
          (let [loaded (test-util/resource-node project proj-path)]
            (is (= expected (:animations (g/node-value loaded :save-value))))
            (is (false? (:dirty (g/node-value loaded :save-data))))))))))

;; Resizing a sheet must save a silently migrated wrapped range before Bob can interpret its old bounds differently.
(deftest animation-frame-migration-after-image-resize
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/image_project")
          image-file (io/file (workspace/project-directory workspace) "images/diamond.png")

          write-sheet!
          (fn [^long tile-count]
            (let [image (BufferedImage. (int (* 16 tile-count)) 16 BufferedImage/TYPE_4BYTE_ABGR)]
              (test-support/do-until-new-mtime
                (fn [^File file]
                  (ImageIO/write image "png" file))
                image-file)))]
      (write-sheet! 4)
      (test-util/write-file-resource!
        workspace "/main/main.tilesource"
        {:image "/images/diamond.png"
         :tile-width 16
         :tile-height 16
         :animations [{:id "wrapped"
                       :start-tile 4
                       :end-tile 2}]})
      (workspace/resource-sync! workspace)
      (let [project (test-util/setup-project! workspace)
            node-id (test-util/resource-node project "/main/main.tilesource")
            migrated-animations [{:id "wrapped"
                                  :frames [4 1 2]}]]
        (test-util/clear-cached-save-data! project)
        (is (= migrated-animations (:animations (g/node-value node-id :save-value))))
        (is (false? (:dirty (g/node-value node-id :save-data))))
        (doseq [[tile-count expected-dirty] [[6 true] [8 false]]]
          (write-sheet! tile-count)
          (workspace/resource-sync! workspace)
          (is (= tile-count (g/node-value node-id :tile-count)))
          (is (= migrated-animations (:animations (g/node-value node-id :save-value))))
          (is (= expected-dirty (:dirty (g/node-value node-id :save-data))))
          (test-util/save-project! project)
          (is (false? (:dirty (g/node-value node-id :save-data))))
          (with-open [reader (io/reader (g/node-value node-id :resource))]
            (let [saved (protobuf/read-map-without-defaults Tile$TileSet reader)
                  texture-set (g/node-value node-id :texture-set)
                  animation (first (:animations texture-set))]
              (is (= migrated-animations (:animations saved)))
              (is (= [3 0 1] (subvec (:frame-indices texture-set) (:start animation) (:end animation)))))))))))

;; Deferred legacy ranges must keep their saved sequences through resizing, image loss, renames, and undo.
(deftest animation-frame-deferred-migration-survives-saving
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/image_project")
          image-file (io/file (workspace/project-directory workspace) "images/recovered.png")

          write-sheet!
          (fn [^long tile-count]
            (let [image (BufferedImage. (int (* 16 tile-count)) 16 BufferedImage/TYPE_4BYTE_ABGR)]
              (test-support/do-until-new-mtime
                (fn [^File file]
                  (ImageIO/write image "png" file))
                image-file)))

          legacy-animations [{:id "early"
                              :start-tile 4
                              :end-tile 2}
                             {:id "later"
                              :start-tile 6
                              :end-tile 2}]
          migrated-animations [{:id "early"
                                :frames [4 1 2]}
                               {:id "later"
                                :frames [6 1 2]}]]
      (test-util/write-file-resource!
        workspace "/main/main.tilesource"
        {:image "/images/recovered.png"
         :tile-width 16
         :tile-height 16
         :animations legacy-animations})
      (workspace/resource-sync! workspace)
      (let [project (test-util/setup-project! workspace)
            node-id (test-util/resource-node project "/main/main.tilesource")
            animation-id (:node-id (first (g/node-value node-id :animation-data)))]
        (test-util/clear-cached-save-data! project)
        (is (= legacy-animations (:animations (g/node-value node-id :save-value))))
        (doseq [[tile-count expected-animations expected-dirty]
                [[4 [(first migrated-animations) (second legacy-animations)] true]
                 [6 migrated-animations true]
                 [8 migrated-animations false]]]
          (write-sheet! tile-count)
          (workspace/resource-sync! workspace)
          (is (= tile-count (g/node-value node-id :tile-count)))
          (is (= expected-animations (:animations (g/node-value node-id :save-value))))
          (is (= expected-dirty (:dirty (g/node-value node-id :save-data))))
          (test-util/save-project! project)
          (test-util/clear-cached-save-data! project)
          (is (false? (:dirty (g/node-value node-id :save-data))))
          (with-open [reader (io/reader (g/node-value node-id :resource))]
            (is (= expected-animations
                   (:animations (protobuf/read-map-without-defaults Tile$TileSet reader))))))
        (test-util/with-prop [node-id :image nil]
          (is (= migrated-animations (:animations (g/node-value node-id :save-value))))
          (is (not (g/error? (g/node-value node-id :save-data)))))
        (test-util/prop! animation-id :id "first")
        (test-util/save-project! project)
        (is (= [4 1 2] (g/node-value animation-id :frames)))
        (test-util/prop! animation-id :frames [2 1])
        (test-util/save-project! project)
        (g/undo! :undo/global)
        (is (= [4 1 2] (g/node-value animation-id :frames)))
        (test-util/save-project! project)
        (with-open [reader (io/reader (g/node-value node-id :resource))]
          (is (= (assoc-in migrated-animations [0 :id] "first")
                 (:animations (protobuf/read-map-without-defaults Tile$TileSet reader)))))))))

;; Built texture sets and editor previews must preserve explicit frame order, repetitions, and animation settings.
(deftest animation-frame-build
  (test-util/with-scratch-project test-util/project-path
    (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")
          anim (add-animation! app-view node-id)
          frames [1 3 6 8 3]]
      (test-util/prop! anim :id "sequence")
      (test-util/prop! anim :frames frames)
      (test-util/prop! anim :fps 12)
      (test-util/prop! anim :flip-horizontal true)
      (test-util/prop! anim :flip-vertical true)
      (doseq [playback [:playback-none
                        :playback-once-forward
                        :playback-once-backward
                        :playback-once-pingpong
                        :playback-loop-forward
                        :playback-loop-backward
                        :playback-loop-pingpong]]
        (test-util/prop! anim :playback playback)
        (testing playback
          (with-open [_ (test-util/build! node-id)]
            (let [built-texture-set
                  (protobuf/pb->map-with-defaults (test-util/built-pb node-id TextureSetProto$TextureSet))

                  animation (coll/first-where #(= "sequence" (:id %)) (:animations built-texture-set))
                  preview-texture-set (g/node-value node-id :texture-set)]
              (is (= [0 2 5 7 2] (subvec (:frame-indices built-texture-set) (:start animation) (:end animation))))
              (is (= {:playback playback
                      :fps 12
                      :flip-horizontal 1
                      :flip-vertical 1}
                     (select-keys animation [:playback :fps :flip-horizontal :flip-vertical])))
              (is (= (:frame-indices built-texture-set) (:frame-indices preview-texture-set)))
              (is (= (:animations built-texture-set) (:animations preview-texture-set))))))))))

;; Editing through the Frames control must save only the new format and reload repeated frames unchanged.
(deftest animation-frame-save-reload
  (test-util/with-scratch-project "test/resources/image_project"
    (let [node-id (test-util/open-tab! project app-view "/main/main.tilesource")
          anim (add-animation! app-view node-id)
          saved-project-path (str (workspace/project-directory workspace))]
      (test-util/prop! anim :id "repeated")
      (properties/set-values!
        (get-in (properties/coalesce [(g/node-value anim :_properties)]) [:properties :frames])
        ["1, 1, 1"])
      (is (= [1 1 1] (g/node-value anim :frames)))
      (test-util/save-project! project)
      (with-open [reader (io/reader (g/node-value node-id :resource))]
        (let [saved (protobuf/read-map-without-defaults Tile$TileSet reader)]
          (is (= [{:id "diamond"
                   :frames [1]}
                  {:id "repeated"
                   :frames [1 1 1]}]
                 (:animations saved)))))
      (test-util/with-loaded-project saved-project-path
        (let [reloaded (test-util/resource-node project "/main/main.tilesource")]
          (is (= [1 1 1] (:frames (coll/first-where #(= "repeated" (:id %))
                                                    (:animations (g/node-value reloaded :save-value))))))
          (is (not (:dirty (g/node-value reloaded :save-data)))))))))

;; Frames must remain available when selecting several animations and apply the same sequence to each.
(deftest animation-frame-multi-edit
  @(fx/on-fx-thread
     (test-util/with-loaded-project
       (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")
             animations [(add-animation! app-view node-id) (add-animation! app-view node-id)]
             property
             (get-in (properties/coalesce (mapv #(g/node-value % :_properties) animations)) [:properties :frames])]
         (is property)
         (let [component (fx/create-component (properties-view/make-control-view property {} @test-util/localization))]
           (try
             (test-util/set-control-value! (fx/instance component) "1-3, ,8, 1,")
             (is (= [[1 2 3 8 1] [1 2 3 8 1]] (mapv #(g/node-value % :frames) animations)))
             (test-util/set-control-value! (fx/instance component) "1--3")
             (is (= [[1 2 3 8 1] [1 2 3 8 1]] (mapv #(g/node-value % :frames) animations)))
             (finally
               (fx/delete-component component))))))))

;; Accepted ranges from property edits and attachments must remain saveable when tile dimensions or image availability change.
(deftest animation-frame-ranges-survive-tile-count-changes
  (test-util/with-loaded-project
    (let [node-id (test-util/open-tab! project app-view "/tilesource/valid.tilesource")
          edited-animation (add-animation! app-view node-id)
          attached-animation
          (first
            (g/tx-nodes-added
              (g/transact
                (attachment/add workspace node-id :animations tile-source/TileAnimationNode
                                (fn [_ animation-node]
                                  (g/set-properties animation-node
                                    :id "attached"
                                    :frames "1-3"))
                                (constantly nil)))))
          animations [edited-animation attached-animation]]
      (properties/set-values!
        (get-in (properties/coalesce [(g/node-value edited-animation :_properties)]) [:properties :frames])
        ["1-3"])
      (is (= [[1 2 3] [1 2 3]] (mapv #(g/node-value % :frames) animations)))
      (test-util/prop! node-id :tile-width 32)
      (test-util/prop! node-id :tile-height 32)
      (is (= 2 (g/node-value node-id :tile-count)))
      (doseq [animation animations]
        (is (= [1 2 3] (g/node-value animation :frames)))
        (is (g/error? (test-util/prop-error animation :frames)))
        (is (= [1 2 3] (:frames (g/node-value animation :ddf-message)))))
      (is (g/error? (g/node-value node-id :build-targets)))
      (is (not (g/error? (g/node-value node-id :save-data))))
      (test-util/with-prop [node-id :image nil]
        (is (= [[1 2 3] [1 2 3]] (mapv #(g/node-value % :frames) animations)))
        (test-util/prop! edited-animation :frames "4-2, ,4,")
        (is (= [4 3 2 4] (g/node-value edited-animation :frames)))
        (is (not (g/error? (g/node-value node-id :save-data))))))))

;; Script attachments must normalize frame ranges before an image is assigned and reject malformed input atomically.
(deftest animation-frame-attachment-input
  (test-util/with-loaded-project
    (let [node-id (test-util/resource-node project "/tilesource/valid.tilesource")]
      (test-util/prop! node-id :image nil)
      (let [add-attachment
            (fn [text]
              (attachment/add workspace node-id :animations tile-source/TileAnimationNode
                              (fn [_ animation-node]
                                (g/set-properties animation-node
                                  :id "attached"
                                  :frames text))
                              (constantly nil)))

            attached-animation (first (g/tx-nodes-added (g/transact (add-attachment "4-2, ,4,"))))
            animation-data (g/node-value node-id :animation-data)]
        (is (= [4 3 2 4] (g/node-value attached-animation :frames)))
        (is (not (g/error? (g/node-value node-id :save-data))))
        (test-util/check-thrown-with-root-cause-msg! #"Invalid animation frames"
                                                     (g/transact (add-attachment "1--3")))
        (is (= animation-data (g/node-value node-id :animation-data)))
        (is (not (g/error? (g/node-value node-id :save-data))))))))

(deftest missing-tilesource-image
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/tilesource/invalid.tilesource")]
      (is (g/error? (test-util/prop-error node-id :image)))
      ;; collision being nil is not an error
      (is (not (g/error? (test-util/prop-error node-id :collision))))
      (is (nil? (g/node-value node-id :collision))))))

(deftest sprite-trim-mode-image-io-error
  (test-support/with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! "test/resources/image_project")
          project (test-util/setup-project! workspace)
          tile-source (project/get-resource-node project "/main/main.tilesource")
          image-file (io/as-file (g/node-value tile-source :image))
          image-bytes (fs/read-bytes image-file)
          texture-set-data-generator (g/node-value tile-source :texture-set-data-generator)
          packed-image-generator (g/node-value tile-source :packed-image-generator)]

      (testing "Initial project state"
        (is (not= :sprite-trim-mode-off (g/node-value tile-source :sprite-trim-mode)))
        (testing "Generators"
          (is (not (g/error? (texture-util/call-generator texture-set-data-generator))))
          (is (not (g/error? (texture-util/call-generator packed-image-generator)))))
        (testing "Graph"
          (is (not (g/error? (g/node-value tile-source :scene))))
          (is (not (g/error? (g/node-value tile-source :build-targets))))
          (is (not (g/error? (g/node-value tile-source :save-data))))))

      (testing "Corrupting referenced image file"
        (test-support/spit-until-new-mtime image-file "This is no longer an image file.")
        (g/clear-system-cache!)
        (testing "Stale generators"
          (is (g/error? (texture-util/call-generator texture-set-data-generator)))
          (is (g/error? (texture-util/call-generator packed-image-generator))))
        (testing "Graph before resource-sync"
          (is (g/error? (g/node-value tile-source :scene)))
          (is (g/error? (g/node-value tile-source :build-targets)))
          (is (not (g/error? (g/node-value tile-source :save-data)))))
        (testing "Graph after resource-sync"
          (workspace/resource-sync! workspace)
          (is (g/error? (g/node-value tile-source :scene)))
          (is (g/error? (g/node-value tile-source :build-targets)))
          (is (not (g/error? (g/node-value tile-source :save-data))))))

      (testing "Restoring referenced image file"
        (test-support/write-until-new-mtime image-file image-bytes)
        (g/clear-system-cache!)
        (testing "Stale generators"
          (is (not (g/error? (texture-util/call-generator texture-set-data-generator))))
          (is (not (g/error? (texture-util/call-generator packed-image-generator)))))
        (testing "Graph before resource-sync"
          (is (not (g/error? (g/node-value tile-source :scene))))
          (is (not (g/error? (g/node-value tile-source :build-targets))))
          (is (not (g/error? (g/node-value tile-source :save-data)))))
        (testing "Graph after resource-sync"
          (workspace/resource-sync! workspace)
          (is (not (g/error? (g/node-value tile-source :scene))))
          (is (not (g/error? (g/node-value tile-source :build-targets))))
          (is (not (g/error? (g/node-value tile-source :save-data))))))

      (testing "Deleting referenced image file"
        (fs/delete! image-file)
        (g/clear-system-cache!)
        (testing "Stale generators"
          (is (g/error? (texture-util/call-generator texture-set-data-generator)))
          (is (g/error? (texture-util/call-generator packed-image-generator))))
        (testing "Graph before resource-sync"
          (is (g/error? (g/node-value tile-source :scene)))
          (is (g/error? (g/node-value tile-source :build-targets)))
          (is (not (g/error? (g/node-value tile-source :save-data)))))
        (testing "Graph after resource-sync"
          (workspace/resource-sync! workspace)
          (is (g/error? (g/node-value tile-source :scene)))
          (is (g/error? (g/node-value tile-source :build-targets)))
          (is (not (g/error? (g/node-value tile-source :save-data)))))))))
