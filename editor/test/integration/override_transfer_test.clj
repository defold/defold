;; Copyright 2020-2025 The Defold Foundation
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

(ns integration.override-transfer-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.gui :as gui]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-post-ec]]
            [util.coll :as coll :refer [pair]])
  (:import [java.lang AutoCloseable]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private pull-up-overrides-plan-alternatives (with-post-ec properties/pull-up-overrides-plan-alternatives))
(def ^:private push-down-overrides-plan-alternatives (with-post-ec properties/push-down-overrides-plan-alternatives))
(def ^:private transfer-overrides-tx-data (with-post-ec properties/transfer-overrides-tx-data))
(def ^:private transferred-properties (with-post-ec properties/transferred-properties))

(defn- transfer-overrides-description
  ^String [transfer-overrides-plan]
  (g/with-auto-evaluation-context evaluation-context
    (-> transfer-overrides-plan
        (properties/transfer-overrides-description evaluation-context)
        (string/replace "__" "_")))) ; Revert escaped underscores for readability.

(defn- outline-node-id [project proj-path & outline-labels]
  (let [resource-node-id (test-util/resource-node project proj-path)]
    (apply test-util/outline-node-id resource-node-id outline-labels)))

(defn- make-project-graph-reverter
  ^AutoCloseable [project]
  (let [project-graph-id (g/node-id->graph-id project)]
    (test-util/make-graph-reverter project-graph-id)))

(defn- set-gui-layout! [project gui-scene-proj-path layout-name]
  {:pre [(string? layout-name)]}
  (let [gui-scene (test-util/resource-node project gui-scene-proj-path)]
    (g/set-property! gui-scene :visible-layout layout-name)))

(defmulti ^:private simplify-save-value (fn [_save-value ext] ext))

(defmethod simplify-save-value :default [save-value _ext] save-value)

(defmethod simplify-save-value "collection" [collection-desc _ext]
  (dissoc collection-desc :scale-along-z))

(defmethod simplify-save-value "gui" [gui-scene-desc _ext]
  (dissoc gui-scene-desc :adjust-reference :material))

(defn- select-save-values [project proj-paths]
  (g/with-auto-evaluation-context evaluation-context
    (let [workspace (project/workspace project evaluation-context)]
      (coll/transfer proj-paths {}
        (map (fn [proj-path]
               {:pre [(string? proj-path)]}
               (let [resource (workspace/find-resource workspace proj-path evaluation-context)
                     ext (resource/type-ext resource)]
                 (pair proj-path
                       (-> (project/get-resource-node project resource evaluation-context)
                           (g/valid-node-value :save-value evaluation-context)
                           (simplify-save-value ext))))))))))

(deftest pull-up-referenced-game-object-overrides-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('author', hash('author from book.script'))"
      "go.property('title', hash('title from book.script'))"]

     "/book.go"
     {:components
      [{:id "book_script"
        :component "/book.script"
        :properties
        [{:id "title"
          :value "title from /book.go"
          :type :property-type-hash}]}]}

     "/shelf.collection"
     {:name "shelf"
      :instances
      [{:id "referenced_book"
        :prototype "/book.go"
        :component-properties
        [{:id "book_script"
          :properties
          [{:id "title"
            :value "title from /shelf.collection"
            :type :property-type-hash}]}]}]}

     "/room.collection"
     {:name "room"
      :collection-instances
      [{:id "referenced_shelf"
        :collection "/shelf.collection"
        :instance-properties
        [{:id "referenced_book"
          :properties
          [{:id "book_script"
            :properties
            [{:id "title"
              :value "title from /room.collection"
              :type :property-type-hash}]}]}]}]}}

    (testing "Can't pull up overrides from '/book.go' to '/book.script'."
      (let [transfer-overrides-plans
            (-> (outline-node-id project "/book.go" "book_script")
                (pull-up-overrides-plan-alternatives :all))]
        (is (empty? transfer-overrides-plans))))

    (testing "Pull up overrides from '/shelf.collection' to '/book.go'."
      (with-open [_ (make-project-graph-reverter project)]
        (let [transfer-overrides-plans
              (-> (outline-node-id project "/shelf.collection" "referenced_book" "book_script")
                  (pull-up-overrides-plan-alternatives :all))]
          (is (= ["Pull Up Title Override to Ancestor in '/book.go'"]
                 (mapv transfer-overrides-description transfer-overrides-plans)))

          (properties/transfer-overrides! (first transfer-overrides-plans))
          (is (= {"/book.go"
                  {:components
                   [{:id "book_script"
                     :component "/book.script"
                     :properties
                     [{:id "title"
                       :value "title from /shelf.collection"
                       :type :property-type-hash}]}]}

                  "/shelf.collection"
                  {:name "shelf"
                   :instances
                   [{:id "referenced_book"
                     :prototype "/book.go"}]}}
                 (select-save-values project ["/book.go" "/shelf.collection"]))))))

    (testing "Pull up overrides from '/room.collection' to '/shelf.collection'."
      (with-open [_ (make-project-graph-reverter project)]
        (let [transfer-overrides-plans
              (-> (outline-node-id project "/room.collection" "referenced_shelf" "referenced_book" "book_script")
                  (pull-up-overrides-plan-alternatives :all))]
          (is (= ["Pull Up Title Override to Ancestor in '/shelf.collection'"
                  "Pull Up Title Override to Ancestor in '/book.go'"]
                 (mapv transfer-overrides-description transfer-overrides-plans)))
          (properties/transfer-overrides! (first transfer-overrides-plans))
          (is (= {"/book.go"
                  {:components
                   [{:id "book_script"
                     :component "/book.script"
                     :properties
                     [{:id "title"
                       :value "title from /book.go"
                       :type :property-type-hash}]}]}

                  "/shelf.collection"
                  {:name "shelf"
                   :instances
                   [{:id "referenced_book"
                     :prototype "/book.go"
                     :component-properties
                     [{:id "book_script"
                       :properties
                       [{:id "title"
                         :value "title from /room.collection"
                         :type :property-type-hash}]}]}]}

                  "/room.collection"
                  {:name "room"
                   :collection-instances
                   [{:id "referenced_shelf"
                     :collection "/shelf.collection"}]}}
                 (select-save-values project ["/book.go" "/shelf.collection" "/room.collection"]))))))

    (testing "Pull up overrides from '/room.collection' to '/book.go'."
      (with-open [_ (make-project-graph-reverter project)]
        (let [transfer-overrides-plans
              (-> (outline-node-id project "/room.collection" "referenced_shelf" "referenced_book" "book_script")
                  (pull-up-overrides-plan-alternatives :all))]
          (is (= ["Pull Up Title Override to Ancestor in '/shelf.collection'"
                  "Pull Up Title Override to Ancestor in '/book.go'"]
                 (mapv transfer-overrides-description transfer-overrides-plans)))
          (properties/transfer-overrides! (second transfer-overrides-plans))
          (is (= {"/book.go"
                  {:components
                   [{:id "book_script"
                     :component "/book.script"
                     :properties
                     [{:id "title"
                       :value "title from /room.collection"
                       :type :property-type-hash}]}]}

                  "/shelf.collection"
                  {:name "shelf"
                   :instances
                   [{:id "referenced_book"
                     :prototype "/book.go"
                     :component-properties
                     [{:id "book_script"
                       :properties
                       [{:id "title"
                         :value "title from /shelf.collection"
                         :type :property-type-hash}]}]}]}

                  "/room.collection"
                  {:name "room"
                   :collection-instances
                   [{:id "referenced_shelf"
                     :collection "/shelf.collection"}]}}
                 (select-save-values project ["/book.go" "/shelf.collection" "/room.collection"]))))))))

(def ^:private gui-text-pb-field-index (gui/prop-key->pb-field-index :text))

(deftest pull-up-gui-template-overrides-test
  (test-util/with-temp-project-content
    {"/book.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "text from /book.gui"}]}

     "/shelf.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_book"
        :template "/book.gui"}
       {:type :type-text
        :template-node-child true
        :id "referenced_book/book_text"
        :parent "referenced_book"
        :text "text from /shelf.gui"
        :overridden-fields [gui-text-pb-field-index]}]}

     "/room.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_shelf"
        :template "/shelf.gui"}
       {:type :type-template
        :template-node-child true
        :id "referenced_shelf/referenced_book"
        :parent "referenced_shelf"}
       {:type :type-text
        :template-node-child true
        :id "referenced_shelf/referenced_book/book_text"
        :parent "referenced_shelf/referenced_book"
        :text "text from /room.gui"
        :overridden-fields [gui-text-pb-field-index]}]}}

    (testing "Can't pull up overrides from '/book.gui'."
      (let [transfer-overrides-plans
            (-> (outline-node-id project "/book.gui" "Nodes" "book_text")
                (pull-up-overrides-plan-alternatives :all))]
        (is (empty? transfer-overrides-plans))))

    (testing "Pull up overrides from '/shelf.gui' to '/book.gui'."
      (with-open [_ (make-project-graph-reverter project)]
        (let [transfer-overrides-plans
              (-> (outline-node-id project "/shelf.gui" "Nodes" "referenced_book" "referenced_book/book_text")
                  (pull-up-overrides-plan-alternatives :all))]
          (is (= ["Pull Up Text Override to 'book_text' in '/book.gui'"]
                 (mapv transfer-overrides-description transfer-overrides-plans)))
          (properties/transfer-overrides! (first transfer-overrides-plans))
          (is (= {"/book.gui"
                  {:nodes
                   [{:type :type-text
                     :id "book_text"
                     :text "text from /shelf.gui"}]}

                  "/shelf.gui"
                  {:nodes
                   [{:type :type-template
                     :id "referenced_book"
                     :template "/book.gui"}
                    {:type :type-text
                     :template-node-child true
                     :id "referenced_book/book_text"
                     :parent "referenced_book"}]}}
                 (select-save-values project ["/book.gui" "/shelf.gui"]))))))

    (testing "Pull up overrides from '/room.gui' to '/shelf.gui'."
      (with-open [_ (make-project-graph-reverter project)]
        (let [transfer-overrides-plans
              (-> (outline-node-id project "/room.gui" "Nodes" "referenced_shelf" "referenced_shelf/referenced_book" "referenced_shelf/referenced_book/book_text")
                  (pull-up-overrides-plan-alternatives :all))]
          (is (= ["Pull Up Text Override to 'referenced_book/book_text' in '/shelf.gui'"
                  "Pull Up Text Override to 'book_text' in '/book.gui'"]
                 (mapv transfer-overrides-description transfer-overrides-plans)))
          (properties/transfer-overrides! (first transfer-overrides-plans))
          (is (= {"/book.gui"
                  {:nodes
                   [{:type :type-text
                     :id "book_text"
                     :text "text from /book.gui"}]}

                  "/shelf.gui"
                  {:nodes
                   [{:type :type-template
                     :id "referenced_book"
                     :template "/book.gui"}
                    {:type :type-text
                     :template-node-child true
                     :id "referenced_book/book_text"
                     :parent "referenced_book"
                     :text "text from /room.gui"
                     :overridden-fields [gui-text-pb-field-index]}]}

                  "/room.gui"
                  {:nodes
                   [{:type :type-template
                     :id "referenced_shelf"
                     :template "/shelf.gui"}
                    {:type :type-template
                     :template-node-child true
                     :id "referenced_shelf/referenced_book"
                     :parent "referenced_shelf"}
                    {:type :type-text
                     :template-node-child true
                     :id "referenced_shelf/referenced_book/book_text"
                     :parent "referenced_shelf/referenced_book"}]}}
                 (select-save-values project ["/book.gui" "/shelf.gui" "/room.gui"]))))))

    (testing "Pull up overrides from '/room.gui' to '/book.gui'."
      (with-open [_ (make-project-graph-reverter project)]
        (let [transfer-overrides-plans
              (-> (outline-node-id project "/room.gui" "Nodes" "referenced_shelf" "referenced_shelf/referenced_book" "referenced_shelf/referenced_book/book_text")
                  (pull-up-overrides-plan-alternatives :all))]
          (is (= ["Pull Up Text Override to 'referenced_book/book_text' in '/shelf.gui'"
                  "Pull Up Text Override to 'book_text' in '/book.gui'"]
                 (mapv transfer-overrides-description transfer-overrides-plans)))
          (properties/transfer-overrides! (second transfer-overrides-plans))
          (is (= {"/book.gui"
                  {:nodes
                   [{:type :type-text
                     :id "book_text"
                     :text "text from /room.gui"}]}

                  "/shelf.gui"
                  {:nodes
                   [{:type :type-template
                     :id "referenced_book"
                     :template "/book.gui"}
                    {:type :type-text
                     :template-node-child true
                     :id "referenced_book/book_text"
                     :parent "referenced_book"
                     :text "text from /shelf.gui"
                     :overridden-fields [gui-text-pb-field-index]}]}

                  "/room.gui"
                  {:nodes
                   [{:type :type-template
                     :id "referenced_shelf"
                     :template "/shelf.gui"}
                    {:type :type-template
                     :template-node-child true
                     :id "referenced_shelf/referenced_book"
                     :parent "referenced_shelf"}
                    {:type :type-text
                     :template-node-child true
                     :id "referenced_shelf/referenced_book/book_text"
                     :parent "referenced_shelf/referenced_book"}]}}
                 (select-save-values project ["/book.gui" "/shelf.gui" "/room.gui"]))))))))

(deftest pull-up-gui-template-layout-overrides-test
  (test-util/with-temp-project-content
    {"/book.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from /book.gui"}]

      :layouts
      [{:name "Landscape"
        :nodes
        [{:type :type-text
          :id "book_text"
          :text "landscape text from /book.gui"
          :overridden-fields [gui-text-pb-field-index]}]}]}

     "/shelf.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_book"
        :template "/book.gui"}
       {:type :type-text
        :template-node-child true
        :id "referenced_book/book_text"
        :parent "referenced_book"
        :text "default text from /shelf.gui"
        :overridden-fields [gui-text-pb-field-index]}]

      :layouts
      [{:name "Landscape"
        :nodes
        [{:type :type-text
          :template-node-child true
          :id "referenced_book/book_text"
          :parent "referenced_book"
          :text "landscape text from /shelf.gui"
          :overridden-fields [gui-text-pb-field-index]}]}]}

     "/room.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_shelf"
        :template "/shelf.gui"}
       {:type :type-template
        :template-node-child true
        :id "referenced_shelf/referenced_book"
        :parent "referenced_shelf"}
       {:type :type-text
        :template-node-child true
        :id "referenced_shelf/referenced_book/book_text"
        :parent "referenced_shelf/referenced_book"
        :text "default text from /room.gui"
        :overridden-fields [gui-text-pb-field-index]}]

      :layouts
      [{:name "Landscape"
        :nodes
        [{:type :type-text
          :template-node-child true
          :id "referenced_shelf/referenced_book/book_text"
          :parent "referenced_shelf/referenced_book"
          :text "landscape text from /room.gui"
          :overridden-fields [gui-text-pb-field-index]}]}]}}

    (testing "Can't pull up overrides from layout in '/book.gui'."
      (with-open [_ (make-project-graph-reverter project)]
        (set-gui-layout! project "/book.gui" "Landscape")
        (let [transfer-overrides-plans
              (-> (outline-node-id project "/book.gui" "Nodes" "book_text")
                  (pull-up-overrides-plan-alternatives :all))]
          (is (empty? transfer-overrides-plans)))))

    (testing "Pull up overrides from layout in '/shelf.gui' to layout in '/book.gui'."
      (with-open [_ (make-project-graph-reverter project)]
        (set-gui-layout! project "/shelf.gui" "Landscape")
        (let [transfer-overrides-plans
              (-> (outline-node-id project "/shelf.gui" "Nodes" "referenced_book" "referenced_book/book_text")
                  (pull-up-overrides-plan-alternatives :all))]
          (is (= ["Pull Up Text Override to 'book_text' in '/book.gui'"]
                 (mapv transfer-overrides-description transfer-overrides-plans)))
          (properties/transfer-overrides! (first transfer-overrides-plans))
          (is (= {"/book.gui"
                  {:nodes
                   [{:type :type-text
                     :id "book_text"
                     :text "default text from /book.gui"}]

                   :layouts
                   [{:name "Landscape"
                     :nodes
                     [{:type :type-text
                       :id "book_text"
                       :text "landscape text from /shelf.gui"
                       :overridden-fields [gui-text-pb-field-index]}]}]}

                  "/shelf.gui"
                  {:nodes
                   [{:type :type-template
                     :id "referenced_book"
                     :template "/book.gui"}
                    {:type :type-text
                     :template-node-child true
                     :id "referenced_book/book_text"
                     :parent "referenced_book"
                     :text "default text from /shelf.gui"
                     :overridden-fields [gui-text-pb-field-index]}]

                   :layouts
                   [{:name "Landscape"}]}}
                 (select-save-values project ["/book.gui" "/shelf.gui"]))))))

    (testing "Pull up overrides from layout in '/room.gui' to layout in '/book.gui'."
      (with-open [_ (make-project-graph-reverter project)]
        (set-gui-layout! project "/room.gui" "Landscape")
        (let [transfer-overrides-plans
              (-> (outline-node-id project "/room.gui" "Nodes" "referenced_shelf" "referenced_shelf/referenced_book" "referenced_shelf/referenced_book/book_text")
                  (pull-up-overrides-plan-alternatives :all))]
          (is (= ["Pull Up Text Override to 'referenced_book/book_text' in '/shelf.gui'"
                  "Pull Up Text Override to 'book_text' in '/book.gui'"]
                 (mapv transfer-overrides-description transfer-overrides-plans)))
          (properties/transfer-overrides! (second transfer-overrides-plans))
          (is (= {"/book.gui"
                  {:nodes
                   [{:type :type-text
                     :id "book_text"
                     :text "default text from /book.gui"}]

                   :layouts
                   [{:name "Landscape"
                     :nodes
                     [{:type :type-text
                       :id "book_text"
                       :text "landscape text from /room.gui"
                       :overridden-fields [gui-text-pb-field-index]}]}]}

                  "/shelf.gui"
                  {:nodes
                   [{:type :type-template
                     :id "referenced_book"
                     :template "/book.gui"}
                    {:type :type-text
                     :template-node-child true
                     :id "referenced_book/book_text"
                     :parent "referenced_book"
                     :text "default text from /shelf.gui"
                     :overridden-fields [gui-text-pb-field-index]}]

                   :layouts
                   [{:name "Landscape"
                     :nodes
                     [{:type :type-text
                       :template-node-child true
                       :id "referenced_book/book_text"
                       :parent "referenced_book"
                       :text "landscape text from /shelf.gui"
                       :overridden-fields [gui-text-pb-field-index]}]}]}

                  "/room.gui"
                  {:nodes
                   [{:type :type-template
                     :id "referenced_shelf"
                     :template "/shelf.gui"}
                    {:type :type-template
                     :template-node-child true
                     :id "referenced_shelf/referenced_book"
                     :parent "referenced_shelf"}
                    {:type :type-text
                     :template-node-child true
                     :id "referenced_shelf/referenced_book/book_text"
                     :parent "referenced_shelf/referenced_book"
                     :text "default text from /room.gui"
                     :overridden-fields [gui-text-pb-field-index]}]

                   :layouts
                   [{:name "Landscape"}]}}
                 (select-save-values project ["/book.gui" "/shelf.gui" "/room.gui"]))))))))
