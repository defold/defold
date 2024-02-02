;; Copyright 2020-2024 The Defold Foundation
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

(ns integration.test-util-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.collection :as collection]
            [editor.game-object :as game-object]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.sprite :as sprite]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]))

(deftest run-event-loop-test

  (testing "Exit function terminates event loop."
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (exit-event-loop!)))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-now
                                             (exit-event-loop!))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-later
                                             (exit-event-loop!))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-later
                                             (ui/run-later
                                               (exit-event-loop!)))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-later
                                             (ui/run-now
                                               (exit-event-loop!)))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-now
                                             (ui/run-later
                                               (exit-event-loop!))))))))

  (testing "Throwing terminates event loop."
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (assert false)))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-now
                                      (assert false))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-later
                                      (assert false))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-later
                                      (ui/run-later
                                        (assert false)))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-later
                                      (ui/run-now
                                        (assert false)))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-now
                                      (ui/run-later
                                        (assert false))))))))

  (testing "Blocks until terminated."
    (let [begin (System/currentTimeMillis)]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (Thread/sleep 300)
                                   (exit-event-loop!)))
      (is (<= 230 (- (System/currentTimeMillis) begin)))))

  (testing "Performs side-effects in order."
    (let [side-effects (atom [])]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (ui/run-later
                                     (swap! side-effects conj :b)
                                     (exit-event-loop!))
                                   (swap! side-effects conj :a)))
      (is (= [:a :b] @side-effects))))

  (testing "Executes actions on the calling thread."
    (let [calling-thread (Thread/currentThread)
          action-threads (atom [])]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (swap! action-threads conj (Thread/currentThread))
                                   (ui/run-now
                                     (swap! action-threads conj (Thread/currentThread)))
                                   (ui/run-later
                                     (swap! action-threads conj (Thread/currentThread))
                                     (exit-event-loop!))))
      (is (= [calling-thread calling-thread calling-thread] @action-threads))))

  (testing "UI thread checks."
    (let [errors (atom [])
          results (atom [])]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (swap! results conj (ui/on-ui-thread?))
                                   (ui/run-now
                                     (swap! results conj (ui/on-ui-thread?)))
                                   (ui/run-later
                                     (swap! results conj (ui/on-ui-thread?))
                                     (future
                                       (try
                                         (swap! results conj (ui/on-ui-thread?))
                                         (ui/run-now
                                           (swap! results conj (ui/on-ui-thread?))
                                           (future
                                             (try
                                               (swap! results conj (ui/on-ui-thread?))
                                               (catch Throwable error
                                                 (swap! errors conj error))
                                               (finally
                                                 (exit-event-loop!)))))
                                         (catch Throwable error
                                           (swap! errors conj error)
                                           (exit-event-loop!)))))))
      (is (empty? @errors))
      (is (= [true true true false true false] @results)))))

(defn- embedded?
  ([expected-node-type node-id]
   (embedded? (g/now) expected-node-type node-id))
  ([basis expected-node-type node-id]
   (and (= expected-node-type (g/node-type* basis node-id))
        (resource/memory-resource? (resource-node/resource basis node-id)))))

(deftest collection-creation-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)
              project (test-util/setup-project! workspace)
              sprite-resource-type (workspace/get-resource-type workspace "sprite")
              sprite (test-util/make-resource-node! project "/sprite.sprite")
              chair (test-util/make-resource-node! project "/chair.go")
              chair-referenced-sprite (test-util/add-referenced-component! chair (resource-node/resource sprite))
              chair-embedded-sprite (test-util/add-embedded-component! chair sprite-resource-type)
              room (test-util/make-resource-node! project "/room.collection")
              room-referenced-chair (test-util/add-referenced-game-object! room (resource-node/resource chair))
              room-embedded-chair (test-util/add-embedded-game-object! room)
              room-embedded-chair-referenced-sprite (test-util/add-referenced-component! room-embedded-chair (resource-node/resource sprite))
              room-embedded-chair-embedded-sprite (test-util/add-embedded-component! room-embedded-chair sprite-resource-type)
              house (test-util/make-resource-node! project "/house.collection")
              house-referenced-room (test-util/add-referenced-collection! house (resource-node/resource room))]

          (testing "Created node types."
            (is (= sprite/SpriteNode (g/node-type* sprite)))
            (is (= game-object/GameObjectNode (g/node-type* chair)))
            (is (= game-object/ReferencedComponent (g/node-type* chair-referenced-sprite)))
            (is (= game-object/EmbeddedComponent (g/node-type* chair-embedded-sprite)))
            (is (= collection/CollectionNode (g/node-type* room)))
            (is (= collection/ReferencedGOInstanceNode (g/node-type* room-referenced-chair)))
            (is (= collection/EmbeddedGOInstanceNode (g/node-type* room-embedded-chair)))
            (is (= game-object/ReferencedComponent (g/node-type* room-embedded-chair-referenced-sprite)))
            (is (= game-object/EmbeddedComponent (g/node-type* room-embedded-chair-embedded-sprite)))
            (is (= collection/CollectionInstanceNode (g/node-type* house-referenced-room))))

          (testing "Query functions."

            (testing "On chair game object."
              (is (= [chair-referenced-sprite] (test-util/referenced-components chair)))
              (is (= sprite (test-util/to-component-resource-node-id chair-referenced-sprite)))

              (is (= [chair-embedded-sprite] (test-util/embedded-components chair)))
              (is (embedded? sprite/SpriteNode (test-util/to-component-resource-node-id chair-embedded-sprite))))

            (testing "On room collection."
              (is (= [room-referenced-chair] (test-util/referenced-game-objects room)))
              (is (= chair (g/override-original (test-util/to-game-object-node-id room-referenced-chair))))

              (is (= [room-embedded-chair] (test-util/embedded-game-objects room)))
              (is (embedded? game-object/GameObjectNode (test-util/to-game-object-node-id room-embedded-chair)))

              (is (= [room-embedded-chair-referenced-sprite] (test-util/referenced-components room-embedded-chair)))
              (is (= sprite (test-util/to-component-resource-node-id room-embedded-chair-referenced-sprite)))

              (is (= [room-embedded-chair-embedded-sprite] (test-util/embedded-components room-embedded-chair)))
              (is (embedded? sprite/SpriteNode (test-util/to-component-resource-node-id room-embedded-chair-embedded-sprite))))

            (testing "On house collection."
              (is (= [house-referenced-room] (test-util/referenced-collections house)))
              (is (= room (g/override-original (test-util/to-collection-node-id house-referenced-room)))))))))))
