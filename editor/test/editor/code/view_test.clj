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

(ns editor.code.view-test
  (:require [clojure.test :refer :all]
            [editor.code.data :as data]
            [editor.code.view :as view]))

(set! *warn-on-reflection* true)

(defn- completion-context [line trigger-characters]
  (let [cursor (data/->Cursor 0 (count line))]
    (view/produce-completion-context
      {:lines [line]
       :cursor-ranges [(data/Cursor->CursorRange cursor)]
       :completion-trigger-characters trigger-characters})))

(deftest produce-completion-context-test
  (testing "dotted prefix"
    (let [context (completion-context "socket.d" #{"." "#"})]
      (is (= "socket" (:context context)))
      (is (= "d" (:query context)))
      (is (= "d" (:trigger context)))))
  (testing "hash at the start of a string"
    (let [context (completion-context "msg.post(\"#" #{"." "#"})]
      (is (= "#" (:context context)))
      (is (= "" (:query context)))
      (is (= "#" (:trigger context)))))
  (testing "hash with a partial component id"
    (let [context (completion-context "msg.post(\"#play_s" #{"." "#"})]
      (is (= "#" (:context context)))
      (is (= "play_s" (:query context)))
      (is (= (data/->Cursor 0 11) (:insert-cursor context)))))
  (testing "hash in a single-quoted string"
    (let [context (completion-context "msg.post('#cam" #{"." "#"})]
      (is (= "#" (:context context)))
      (is (= "cam" (:query context)))))
  (testing "hash as the length operator is not a component id context"
    (let [context (completion-context "local n = #" #{"." "#"})]
      (is (= "" (:context context)))
      (is (= "" (:query context)))))
  (testing "hash context requires the hash trigger character"
    (let [context (completion-context "msg.post(\"#play_s" #{"."})]
      (is (= "" (:context context)))
      (is (= "play_s" (:query context))))))
