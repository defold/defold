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

(ns integration.override-transfer-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.gui :as gui]
            [editor.localization :as localization]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-post-ec]]
            [util.coll :as coll :refer [pair]]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private gui-text-pb-field-index (gui/prop-key->pb-field-index :text))
(def ^:private transferred-properties (with-post-ec properties/transferred-properties))

(defmulti ^:private simplify-save-value (fn [_save-value ext] ext))

(defmethod simplify-save-value :default [save-value _ext] save-value)

(defmethod simplify-save-value "collection" [collection-desc _ext]
  (dissoc collection-desc :scale-along-z))

(defmethod simplify-save-value "gui" [gui-scene-desc _ext]
  (dissoc gui-scene-desc :adjust-reference :material))

(defn- select-save-values
  ([project proj-path-predicate]
   (g/with-auto-evaluation-context evaluation-context
     (select-save-values project proj-path-predicate evaluation-context)))
  ([project proj-path-predicate evaluation-context]
   (let [resources-by-proj-path (g/valid-node-value project :resource-map evaluation-context)
         resource-nodes-by-proj-path (g/valid-node-value project :nodes-by-resource-path evaluation-context)]
     (coll/into-> resource-nodes-by-proj-path (sorted-map)
       (keep (fn [[proj-path resource-node-id]]
               (when (proj-path-predicate proj-path)
                 (let [resource (resources-by-proj-path proj-path)
                       save-value (g/node-value resource-node-id :save-value evaluation-context)]
                   (pair proj-path
                         (cond-> save-value
                                 (not (g/error-value? save-value))
                                 (simplify-save-value (resource/type-ext resource))))))))))))

(defn- pull-up-overrides-plan-alternatives
  [source-node-id source-prop-kws]
  (g/with-auto-evaluation-context evaluation-context
    (when-let [source-prop-infos-by-prop-kw (properties/transferred-properties source-node-id source-prop-kws evaluation-context)]
      (properties/pull-up-overrides-plan-alternatives source-node-id source-prop-infos-by-prop-kw evaluation-context))))

(defn- push-down-overrides-plan-alternatives
  [source-node-id source-prop-kws]
  (g/with-auto-evaluation-context evaluation-context
    (when-let [source-prop-infos-by-prop-kw (properties/transferred-properties source-node-id source-prop-kws evaluation-context)]
      (properties/push-down-overrides-plan-alternatives source-node-id source-prop-infos-by-prop-kw evaluation-context))))

(defn- transfer-overrides-plan-info
  ^String [transfer-overrides-plan]
  {:pre [(properties/transfer-overrides-plan? transfer-overrides-plan)]}
  (g/with-auto-evaluation-context evaluation-context
    (let [status (properties/transfer-overrides-status transfer-overrides-plan)
          description (-> transfer-overrides-plan
                          (properties/transfer-overrides-description evaluation-context)
                          ;; Revert escaped underscores for readability.
                          (localization/transform string/replace "__" "_"))]
      (pair (test-util/localization description) status))))

(defn- set-gui-layout!
  [project gui-scene-proj-path layout-name]
  {:pre [(string? layout-name)]}
  (let [gui-scene (test-util/resource-node project gui-scene-proj-path)
        layout-names (g/valid-node-value gui-scene :layout-names)]
    (if (or (= "" layout-name)
            (coll/any? #(= layout-name %) layout-names))
      (g/set-property! gui-scene :visible-layout layout-name)
      (throw (ex-info (format "Layout '%s' does not exist in Gui Scene '%s'."
                              layout-name
                              gui-scene-proj-path)
                      {:project project
                       :gui-scene-proj-path gui-scene-proj-path
                       :layout-name layout-name
                       :layout-name-candidates (vec (sort layout-names))})))))

(deftest pull-up-go-overrides-from-book-go-to-book-script-disallowed-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('text', hash('text from book.script'))"]

     "/book.go"
     {:components
      [{:id "book_script"
        :component "/book.script"
        :properties
        [{:id "text"
          :value "text from book.go"
          :type :property-type-hash}]}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/book.go" "book_script")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (not (empty? (transferred-properties source-node-id :all))))
        (is (empty? transfer-overrides-plans))))))

(deftest pull-up-go-overrides-from-shelf-collection-to-referenced-book-go-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('text', hash('text from book.script'))"]

     "/book.go"
     {:components
      [{:id "book_script"
        :component "/book.script"}]}

     "/shelf.collection"
     {:name "shelf"
      :instances
      [{:id "referenced_book"
        :prototype "/book.go"
        :component-properties
        [{:id "book_script"
          :properties
          [{:id "text"
            :value "text from shelf.collection"
            :type :property-type-hash}]}]}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf.collection" "referenced_book" "book_script")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"text\" Override to \"book_script\" in \"/book.go\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/book.go"
                {:components
                 [{:id "book_script"
                   :component "/book.script"
                   :properties
                   [{:id "text"
                     :value "text from shelf.collection"
                     :type :property-type-hash}]}]}

                "/shelf.collection"
                {:name "shelf"
                 :instances
                 [{:id "referenced_book"
                   :prototype "/book.go"}]}}
               (select-save-values project #{"/book.go" "/shelf.collection"})))))))

(deftest pull-up-go-overrides-from-room-collection-to-shelf-collection-with-referenced-book-go-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('text', hash('text from book.script'))"]

     "/book.go"
     {:components
      [{:id "book_script"
        :component "/book.script"}]}

     "/shelf.collection"
     {:name "shelf"
      :instances
      [{:id "referenced_book"
        :prototype "/book.go"}]}

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
            [{:id "text"
              :value "text from room.collection"
              :type :property-type-hash}]}]}]}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/room.collection" "referenced_shelf" "referenced_book" "book_script")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"text\" Override to \"referenced_book/book_script\" in \"/shelf.collection\"" :ok]
                ["Pull Up \"text\" Override to \"book_script\" in \"/book.go\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/shelf.collection"
                {:name "shelf"
                 :instances
                 [{:id "referenced_book"
                   :prototype "/book.go"
                   :component-properties
                   [{:id "book_script"
                     :properties
                     [{:id "text"
                       :value "text from room.collection"
                       :type :property-type-hash}]}]}]}

                "/room.collection"
                {:name "room"
                 :collection-instances
                 [{:id "referenced_shelf"
                   :collection "/shelf.collection"}]}}
               (select-save-values project #{"/shelf.collection" "/room.collection"})))))))

(deftest pull-up-go-overrides-from-room-collection-to-shelf-collection-with-embedded-book-go-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('text', hash('text from book.script'))"]

     "/shelf.collection"
     {:name "shelf"
      :embedded-instances
      [{:id "embedded_book"
        :data {:components
               [{:id "book_script"
                 :component "/book.script"}]}}]}

     "/room.collection"
     {:name "room"
      :collection-instances
      [{:id "referenced_shelf"
        :collection "/shelf.collection"
        :instance-properties
        [{:id "embedded_book"
          :properties
          [{:id "book_script"
            :properties
            [{:id "text"
              :value "text from room.collection"
              :type :property-type-hash}]}]}]}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/room.collection" "referenced_shelf" "embedded_book" "book_script")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"text\" Override to \"embedded_book/book_script\" in \"/shelf.collection\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/shelf.collection"
                {:name "shelf"
                 :embedded-instances
                 [{:id "embedded_book"
                   :data {:components
                          [{:id "book_script"
                            :component "/book.script"
                            :properties
                            [{:id "text"
                              :value "text from room.collection"
                              :type :property-type-hash}]}]}}]}

                "/room.collection"
                {:name "room"
                 :collection-instances
                 [{:id "referenced_shelf"
                   :collection "/shelf.collection"}]}}
               (select-save-values project #{"/shelf.collection" "/room.collection"})))))))

(deftest pull-up-go-overrides-from-room-collection-to-remotely-referenced-book-go-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('text', hash('text from book.script'))"]

     "/book.go"
     {:components
      [{:id "book_script"
        :component "/book.script"}]}

     "/shelf.collection"
     {:name "shelf"
      :instances
      [{:id "referenced_book"
        :prototype "/book.go"
        :component-properties
        [{:id "book_script"
          :properties
          [{:id "text"
            :value "text from shelf.collection"
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
            [{:id "text"
              :value "text from room.collection"
              :type :property-type-hash}]}]}]}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/room.collection" "referenced_shelf" "referenced_book" "book_script")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"text\" Override to \"referenced_book/book_script\" in \"/shelf.collection\"" :ok]
                ["Pull Up \"text\" Override to \"book_script\" in \"/book.go\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (second transfer-overrides-plans))
        (is (= {"/book.go"
                {:components
                 [{:id "book_script"
                   :component "/book.script"
                   :properties
                   [{:id "text"
                     :value "text from room.collection"
                     :type :property-type-hash}]}]}

                "/shelf.collection"
                {:name "shelf"
                 :instances
                 [{:id "referenced_book"
                   :prototype "/book.go"
                   :component-properties
                   [{:id "book_script"
                     :properties
                     [{:id "text"
                       :value "text from shelf.collection"
                       :type :property-type-hash}]}]}]}

                "/room.collection"
                {:name "room"
                 :collection-instances
                 [{:id "referenced_shelf"
                   :collection "/shelf.collection"}]}}
               (select-save-values project #{"/book.go" "/shelf.collection" "/room.collection"})))))))

(deftest pull-up-gui-overrides-from-book-to-nothing-test
  (test-util/with-temp-project-content
    {"/book.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from book.gui"}]}}

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book.gui" "")
      (let [source-node-id (test-util/resource-outline-node-id project "/book.gui" (localization/message "outline.gui.nodes") "book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (empty? (transferred-properties source-node-id :all)))
        (is (empty? transfer-overrides-plans))))))

(deftest pull-up-gui-overrides-from-shelf-to-book-test
  (test-util/with-temp-project-content
    {"/book.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from book.gui"}]}

     "/shelf.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_book"
        :template "/book.gui"}
       {:type :type-text
        :template-node-child true
        :id "referenced_book/book_text"
        :parent "referenced_book"
        :text "default text from shelf.gui"
        :overridden-fields [gui-text-pb-field-index]}]}}

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book.gui" "")
      (set-gui-layout! project "/shelf.gui" "")
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf.gui" (localization/message "outline.gui.nodes") "referenced_book" "referenced_book/book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"Text\" Override to \"book_text\" in \"/book.gui\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/book.gui"
                {:nodes
                 [{:type :type-text
                   :id "book_text"
                   :text "default text from shelf.gui"}]}

                "/shelf.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_book"
                   :template "/book.gui"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_book/book_text"
                   :parent "referenced_book"}]}}
               (select-save-values project #{"/book.gui" "/shelf.gui"})))))))

(deftest pull-up-gui-overrides-from-room-to-book-test
  (test-util/with-temp-project-content
    {"/book.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from book.gui"}]}

     "/shelf.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_book"
        :template "/book.gui"}
       {:type :type-text
        :template-node-child true
        :id "referenced_book/book_text"
        :parent "referenced_book"
        :text "default text from shelf.gui"
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
        :text "default text from room.gui"
        :overridden-fields [gui-text-pb-field-index]}]}}

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book.gui" "")
      (set-gui-layout! project "/shelf.gui" "")
      (set-gui-layout! project "/room.gui" "")
      (let [source-node-id (test-util/resource-outline-node-id project "/room.gui" (localization/message "outline.gui.nodes") "referenced_shelf" "referenced_shelf/referenced_book" "referenced_shelf/referenced_book/book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"Text\" Override to \"referenced_book/book_text\" in \"/shelf.gui\"" :ok]
                ["Pull Up \"Text\" Override to \"book_text\" in \"/book.gui\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (second transfer-overrides-plans))
        (is (= {"/book.gui"
                {:nodes
                 [{:type :type-text
                   :id "book_text"
                   :text "default text from room.gui"}]}

                "/shelf.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_book"
                   :template "/book.gui"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_book/book_text"
                   :parent "referenced_book"
                   :text "default text from shelf.gui"
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
               (select-save-values project #{"/book.gui" "/shelf.gui" "/room.gui"})))))))

(deftest pull-up-gui-overrides-from-book-l-to-nothing-test
  (test-util/with-temp-project-content
    {"/book_l.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from book_l.gui"}]

      :layouts
      [{:name "Landscape"
        :nodes
        [{:type :type-text
          :id "book_text"
          :text "landscape text from book_l.gui"
          :overridden-fields [gui-text-pb-field-index]}]}]}}

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book_l.gui" "")
      (let [source-node-id (test-util/resource-outline-node-id project "/book_l.gui" (localization/message "outline.gui.nodes") "book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (empty? (transferred-properties source-node-id :all)))
        (is (empty? transfer-overrides-plans))))

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book_l.gui" "Landscape")
      (let [source-node-id (test-util/resource-outline-node-id project "/book_l.gui" (localization/message "outline.gui.nodes") "book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"Text\" Override to \"book_text\" in Default Layout of \"/book_l.gui\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/book_l.gui"
                {:nodes
                 [{:type :type-text
                   :id "book_text"
                   :text "landscape text from book_l.gui"}]

                 :layouts
                 [{:name "Landscape"}]}}
               (select-save-values project #{"/book_l.gui"})))))))

(deftest pull-up-gui-overrides-from-shelf-to-book-l-test
  (test-util/with-temp-project-content
    {"/book_l.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from book_l.gui"}]

      :layouts
      [{:name "Landscape"
        :nodes
        [{:type :type-text
          :id "book_text"
          :text "landscape text from book_l.gui"
          :overridden-fields [gui-text-pb-field-index]}]}]}

     "/shelf.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_book"
        :template "/book_l.gui"}
       {:type :type-text
        :template-node-child true
        :id "referenced_book/book_text"
        :parent "referenced_book"
        :text "default text from shelf.gui"
        :overridden-fields [gui-text-pb-field-index]}]}}

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book_l.gui" "Landscape")
      (set-gui-layout! project "/shelf.gui" "")
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf.gui" (localization/message "outline.gui.nodes") "referenced_book" "referenced_book/book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"Text\" Override to \"book_text\" in \"/book_l.gui\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/book_l.gui"
                {:nodes
                 [{:type :type-text
                   :id "book_text"
                   :text "default text from shelf.gui"}]

                 :layouts
                 [{:name "Landscape"
                   :nodes
                   [{:type :type-text
                     :id "book_text"
                     :text "landscape text from book_l.gui"
                     :overridden-fields [gui-text-pb-field-index]}]}]}

                "/shelf.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_book"
                   :template "/book_l.gui"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_book/book_text"
                   :parent "referenced_book"}]}}
               (select-save-values project #{"/book_l.gui" "/shelf.gui"})))))))

(deftest pull-up-gui-overrides-from-shelf-l-to-book-test
  (test-util/with-temp-project-content
    {"/book.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from book.gui"}]}

     "/shelf_l.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_book"
        :template "/book.gui"}
       {:type :type-text
        :template-node-child true
        :id "referenced_book/book_text"
        :parent "referenced_book"
        :text "default text from shelf_l.gui"
        :overridden-fields [gui-text-pb-field-index]}]

      :layouts
      [{:name "Landscape"
        :nodes
        [{:type :type-text
          :template-node-child true
          :id "referenced_book/book_text"
          :parent "referenced_book"
          :text "landscape text from shelf_l.gui"
          :overridden-fields [gui-text-pb-field-index]}]}]}}

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book.gui" "")
      (set-gui-layout! project "/shelf_l.gui" "")
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf_l.gui" (localization/message "outline.gui.nodes") "referenced_book" "referenced_book/book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"Text\" Override to \"book_text\" in \"/book.gui\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/book.gui"
                {:nodes
                 [{:type :type-text
                   :id "book_text"
                   :text "default text from shelf_l.gui"}]}

                "/shelf_l.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_book"
                   :template "/book.gui"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_book/book_text"
                   :parent "referenced_book"}]

                 :layouts
                 [{:name "Landscape"
                   :nodes
                   [{:type :type-text
                     :template-node-child true
                     :id "referenced_book/book_text"
                     :parent "referenced_book"
                     :text "landscape text from shelf_l.gui"
                     :overridden-fields [gui-text-pb-field-index]}]}]}}
               (select-save-values project #{"/book.gui" "/shelf_l.gui"})))))

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book.gui" "")
      (set-gui-layout! project "/shelf_l.gui" "Landscape")
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf_l.gui" (localization/message "outline.gui.nodes") "referenced_book" "referenced_book/book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"Text\" Override to \"referenced_book/book_text\" in Default Layout of \"/shelf_l.gui\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/book.gui"
                {:nodes
                 [{:type :type-text
                   :id "book_text"
                   :text "default text from book.gui"}]}

                "/shelf_l.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_book"
                   :template "/book.gui"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_book/book_text"
                   :parent "referenced_book"
                   :text "landscape text from shelf_l.gui"
                   :overridden-fields [gui-text-pb-field-index]}]

                 :layouts
                 [{:name "Landscape"}]}}
               (select-save-values project #{"/book.gui" "/shelf_l.gui"})))))))

(deftest pull-up-gui-overrides-from-shelf-l-to-book-l-test
  (test-util/with-temp-project-content
    {"/book_l.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from book_l.gui"}]

      :layouts
      [{:name "Landscape"
        :nodes
        [{:type :type-text
          :id "book_text"
          :text "landscape text from book_l.gui"
          :overridden-fields [gui-text-pb-field-index]}]}]}

     "/shelf_l.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_book"
        :template "/book_l.gui"}
       {:type :type-text
        :template-node-child true
        :id "referenced_book/book_text"
        :parent "referenced_book"
        :text "default text from shelf_l.gui"
        :overridden-fields [gui-text-pb-field-index]}]

      :layouts
      [{:name "Landscape"
        :nodes
        [{:type :type-text
          :template-node-child true
          :id "referenced_book/book_text"
          :parent "referenced_book"
          :text "landscape text from shelf_l.gui"
          :overridden-fields [gui-text-pb-field-index]}]}]}}

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book_l.gui" "Landscape")
      (set-gui-layout! project "/shelf_l.gui" "")
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf_l.gui" (localization/message "outline.gui.nodes") "referenced_book" "referenced_book/book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"Text\" Override to \"book_text\" in \"/book_l.gui\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/book_l.gui"
                {:nodes
                 [{:type :type-text
                   :id "book_text"
                   :text "default text from shelf_l.gui"}]

                 :layouts
                 [{:name "Landscape"
                   :nodes
                   [{:type :type-text
                     :id "book_text"
                     :text "landscape text from book_l.gui"
                     :overridden-fields [gui-text-pb-field-index]}]}]}

                "/shelf_l.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_book"
                   :template "/book_l.gui"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_book/book_text"
                   :parent "referenced_book"}]

                 :layouts
                 [{:name "Landscape"
                   :nodes
                   [{:type :type-text
                     :template-node-child true
                     :id "referenced_book/book_text"
                     :parent "referenced_book"
                     :text "landscape text from shelf_l.gui"
                     :overridden-fields [gui-text-pb-field-index]}]}]}}
               (select-save-values project #{"/book_l.gui" "/shelf_l.gui"})))))

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book_l.gui" "")
      (set-gui-layout! project "/shelf_l.gui" "Landscape")
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf_l.gui" (localization/message "outline.gui.nodes") "referenced_book" "referenced_book/book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"Text\" Override to \"referenced_book/book_text\" in Default Layout of \"/shelf_l.gui\"" :ok]
                ["Pull Up \"Text\" Override to \"book_text\" in Landscape Layout of \"/book_l.gui\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/book_l.gui"
                {:nodes
                 [{:type :type-text
                   :id "book_text"
                   :text "default text from book_l.gui"}]

                 :layouts
                 [{:name "Landscape"
                   :nodes
                   [{:type :type-text
                     :id "book_text"
                     :text "landscape text from book_l.gui"
                     :overridden-fields [gui-text-pb-field-index]}]}]}

                "/shelf_l.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_book"
                   :template "/book_l.gui"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_book/book_text"
                   :parent "referenced_book"
                   :text "landscape text from shelf_l.gui"
                   :overridden-fields [gui-text-pb-field-index]}]

                 :layouts
                 [{:name "Landscape"}]}}
               (select-save-values project #{"/book_l.gui" "/shelf_l.gui"})))))

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book_l.gui" "")
      (set-gui-layout! project "/shelf_l.gui" "Landscape")
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf_l.gui" (localization/message "outline.gui.nodes") "referenced_book" "referenced_book/book_text")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Pull Up \"Text\" Override to \"referenced_book/book_text\" in Default Layout of \"/shelf_l.gui\"" :ok]
                ["Pull Up \"Text\" Override to \"book_text\" in Landscape Layout of \"/book_l.gui\"" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (second transfer-overrides-plans))
        (is (= {"/book_l.gui"
                {:nodes
                 [{:type :type-text
                   :id "book_text"
                   :text "default text from book_l.gui"}]

                 :layouts
                 [{:name "Landscape"
                   :nodes
                   [{:type :type-text
                     :id "book_text"
                     :text "landscape text from shelf_l.gui"
                     :overridden-fields [gui-text-pb-field-index]}]}]}

                "/shelf_l.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_book"
                   :template "/book_l.gui"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_book/book_text"
                   :parent "referenced_book"
                   :text "default text from shelf_l.gui"
                   :overridden-fields [gui-text-pb-field-index]}]

                 :layouts
                 [{:name "Landscape"}]}}
               (select-save-values project #{"/book_l.gui" "/shelf_l.gui"})))))))

(deftest pull-up-model-overrides-to-material-disallowed-test
  (test-util/with-temp-project-content
    {"/material.material"
     {:name "material"
      :vertex-program ""
      :fragment-program ""

      :attributes
      [{:name "tint"
        :semantic-type :semantic-type-color
        :double-values {:v [1.0 1.0 1.0 1.0]}}]}

     "/book.model"
     {:name "book"
      :mesh "/builtins/assets/meshes/cube.dae"

      :materials
      [{:name "default"
        :material "/material.material"

        :attributes
        [{:name "tint"
          :double-values {:v [1.0 0.0 0.0 1.0]}}]}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/book.model")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (not (empty? (transferred-properties source-node-id :all))))
        (is (empty? transfer-overrides-plans))))))

(deftest pull-up-particlefx-overrides-to-material-disallowed-test
  (test-util/with-temp-project-content
    {"/material.material"
     {:name "material"
      :vertex-program ""
      :fragment-program ""

      :attributes
      [{:name "tint"
        :semantic-type :semantic-type-color
        :double-values {:v [1.0 1.0 1.0 1.0]}}]}

     "/book.particlefx"
     {:emitters
      [{:material "/material.material"
        :type :emitter-type-2dcone
        :space :emission-space-world
        :mode :play-mode-loop
        :tile-source ""
        :animation ""
        :max-particle-count 128

        :attributes
        [{:name "tint"
          :double-values {:v [1.0 0.0 0.0 1.0]}}]}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/book.particlefx" "emitter")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (not (empty? (transferred-properties source-node-id :all))))
        (is (empty? transfer-overrides-plans))))))

(deftest pull-up-sprite-overrides-to-material-disallowed-test
  (test-util/with-temp-project-content
    {"/material.material"
     {:name "material"
      :vertex-program ""
      :fragment-program ""

      :attributes
      [{:name "tint"
        :semantic-type :semantic-type-color
        :double-values {:v [1.0 1.0 1.0 1.0]}}]}

     "/book.sprite"
     {:material "/material.material"
      :default-animation ""

      :attributes
      [{:name "tint"
        :double-values {:v [1.0 0.0 0.0 1.0]}}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/book.sprite")
            transfer-overrides-plans (pull-up-overrides-plan-alternatives source-node-id :all)]
        (is (not (empty? (transferred-properties source-node-id :all))))
        (is (empty? transfer-overrides-plans))))))

(deftest push-down-go-overrides-from-book-go-to-nothing-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('text', hash('text from book.script'))"]

     "/book.go"
     {:components
      [{:id "book_script"
        :component "/book.script"
        :properties
        [{:id "text"
          :value "text from book.go"
          :type :property-type-hash}]}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/book.go" "book_script")
            transfer-overrides-plans (push-down-overrides-plan-alternatives source-node-id :all)]
        (is (not (empty? (transferred-properties source-node-id :all))))
        (is (empty? transfer-overrides-plans))))))

(deftest push-down-go-overrides-from-book-go-to-shelf-collections-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('text', hash('text from book.script'))"]

     "/book.go"
     {:components
      [{:id "book_script"
        :component "/book.script"
        :properties
        [{:id "text"
          :value "text from book.go"
          :type :property-type-hash}]}]}

     "/shelf_one.collection"
     {:name "shelf_one"
      :instances
      [{:id "referenced_book"
        :prototype "/book.go"}]}

     "/shelf_two.collection"
     {:name "shelf_two"
      :instances
      [{:id "referenced_book_one"
        :prototype "/book.go"}
       {:id "referenced_book_two"
        :prototype "/book.go"}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/book.go" "book_script")
            transfer-overrides-plans (push-down-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Push Down \"text\" Override to 3 Descendants Across 2 Resources" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/book.go"
                {:components
                 [{:id "book_script"
                   :component "/book.script"}]}

                "/shelf_one.collection"
                {:name "shelf_one"
                 :instances
                 [{:id "referenced_book"
                   :prototype "/book.go"
                   :component-properties
                   [{:id "book_script"
                     :properties
                     [{:id "text"
                       :value "text from book.go"
                       :type :property-type-hash}]}]}]}

                "/shelf_two.collection"
                {:name "shelf_two"
                 :instances
                 [{:id "referenced_book_one"
                   :prototype "/book.go"
                   :component-properties
                   [{:id "book_script"
                     :properties
                     [{:id "text"
                       :value "text from book.go"
                       :type :property-type-hash}]}]}
                  {:id "referenced_book_two"
                   :prototype "/book.go"
                   :component-properties
                   [{:id "book_script"
                     :properties
                     [{:id "text"
                       :value "text from book.go"
                       :type :property-type-hash}]}]}]}}
               (select-save-values project #{"/book.go" "/shelf_one.collection" "/shelf_two.collection"})))))))

(deftest push-down-go-overrides-from-shelf-collection-to-nothing-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('text', hash('text from book.script'))"]

     "/book.go"
     {:components
      [{:id "book_script"
        :component "/book.script"}]}

     "/shelf.collection"
     {:name "shelf"
      :instances
      [{:id "referenced_book"
        :prototype "/book.go"
        :component-properties
        [{:id "book_script"
          :properties
          [{:id "text"
            :value "text from shelf.collection"
            :type :property-type-hash}]}]}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf.collection" "referenced_book" "book_script")
            transfer-overrides-plans (push-down-overrides-plan-alternatives source-node-id :all)]
        (is (not (empty? (transferred-properties source-node-id :all))))
        (is (empty? transfer-overrides-plans))))))

(deftest push-down-go-overrides-from-shelf-collection-to-room-collections-test
  (test-util/with-temp-project-content
    {"/book.script"
     ["go.property('text', hash('text from book.script'))"]

     "/book.go"
     {:components
      [{:id "book_script"
        :component "/book.script"}]}

     "/shelf.collection"
     {:name "shelf"
      :instances
      [{:id "referenced_book"
        :prototype "/book.go"
        :component-properties
        [{:id "book_script"
          :properties
          [{:id "text"
            :value "text from shelf.collection"
            :type :property-type-hash}]}]}]}

     "/room_one.collection"
     {:name "room_one"
      :collection-instances
      [{:id "referenced_shelf"
        :collection "/shelf.collection"}]}

     "/room_two.collection"
     {:name "room_two"
      :collection-instances
      [{:id "referenced_shelf_one"
        :collection "/shelf.collection"}
       {:id "referenced_shelf_two"
        :collection "/shelf.collection"}]}}

    (test-util/with-changes-reverted project
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf.collection" "referenced_book" "book_script")
            transfer-overrides-plans (push-down-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Push Down \"text\" Override to 3 Descendants Across 2 Resources" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/shelf.collection"
                {:name "shelf"
                 :instances
                 [{:id "referenced_book"
                   :prototype "/book.go"}]}

                "/room_one.collection"
                {:name "room_one"
                 :collection-instances
                 [{:id "referenced_shelf"
                   :collection "/shelf.collection"
                   :instance-properties
                   [{:id "referenced_book"
                     :properties
                     [{:id "book_script"
                       :properties
                       [{:id "text"
                         :value "text from shelf.collection"
                         :type :property-type-hash}]}]}]}]}

                "/room_two.collection"
                {:name "room_two"
                 :collection-instances
                 [{:id "referenced_shelf_one"
                   :collection "/shelf.collection"
                   :instance-properties
                   [{:id "referenced_book"
                     :properties
                     [{:id "book_script"
                       :properties
                       [{:id "text"
                         :value "text from shelf.collection"
                         :type :property-type-hash}]}]}]}
                  {:id "referenced_shelf_two"
                   :collection "/shelf.collection"
                   :instance-properties
                   [{:id "referenced_book"
                     :properties
                     [{:id "book_script"
                       :properties
                       [{:id "text"
                         :value "text from shelf.collection"
                         :type :property-type-hash}]}]}]}]}}
               (select-save-values project #{"/shelf.collection" "/room_one.collection" "/room_two.collection"})))))))

(deftest push-down-gui-overrides-from-shelf-to-nothing-test
  (test-util/with-temp-project-content
    {"/book.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from book.gui"}]}

     "/shelf.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_book"
        :template "/book.gui"}
       {:type :type-text
        :template-node-child true
        :id "referenced_book/book_text"
        :parent "referenced_book"
        :text "default text from shelf.gui"
        :overridden-fields [gui-text-pb-field-index]}]}}

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book.gui" "")
      (set-gui-layout! project "/shelf.gui" "")
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf.gui" (localization/message "outline.gui.nodes") "referenced_book" "referenced_book/book_text")
            transfer-overrides-plans (push-down-overrides-plan-alternatives source-node-id :all)]
        (is (not (empty? (transferred-properties source-node-id :all))))
        (is (empty? transfer-overrides-plans))))))

(deftest push-down-gui-overrides-from-shelf-to-rooms-test
  (test-util/with-temp-project-content
    {"/book.gui"
     {:nodes
      [{:type :type-text
        :id "book_text"
        :text "default text from book.gui"}]}

     "/shelf.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_book"
        :template "/book.gui"}
       {:type :type-text
        :template-node-child true
        :id "referenced_book/book_text"
        :parent "referenced_book"
        :text "default text from shelf.gui"
        :overridden-fields [gui-text-pb-field-index]}]}

     "/room_one.gui"
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
        :parent "referenced_shelf/referenced_book"}]}

     "/room_two.gui"
     {:nodes
      [{:type :type-template
        :id "referenced_shelf_one"
        :template "/shelf.gui"}
       {:type :type-template
        :template-node-child true
        :id "referenced_shelf_one/referenced_book"
        :parent "referenced_shelf_one"}
       {:type :type-text
        :template-node-child true
        :id "referenced_shelf_one/referenced_book/book_text"
        :parent "referenced_shelf_one/referenced_book"}
       {:type :type-template
        :id "referenced_shelf_two"
        :template "/shelf.gui"}
       {:type :type-template
        :template-node-child true
        :id "referenced_shelf_two/referenced_book"
        :parent "referenced_shelf_two"}
       {:type :type-text
        :template-node-child true
        :id "referenced_shelf_two/referenced_book/book_text"
        :parent "referenced_shelf_two/referenced_book"}]}}

    (test-util/with-changes-reverted project
      (set-gui-layout! project "/book.gui" "")
      (set-gui-layout! project "/shelf.gui" "")
      (set-gui-layout! project "/room_one.gui" "")
      (set-gui-layout! project "/room_two.gui" "")
      (let [source-node-id (test-util/resource-outline-node-id project "/shelf.gui" (localization/message "outline.gui.nodes") "referenced_book" "referenced_book/book_text")
            transfer-overrides-plans (push-down-overrides-plan-alternatives source-node-id :all)]
        (is (= [["Push Down \"Text\" Override to 3 Descendants Across 2 Resources" :ok]]
               (mapv transfer-overrides-plan-info transfer-overrides-plans)))
        (properties/transfer-overrides! (first transfer-overrides-plans))
        (is (= {"/shelf.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_book"
                   :template "/book.gui"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_book/book_text"
                   :parent "referenced_book"}]}

                "/room_one.gui"
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
                   :text "default text from shelf.gui"
                   :overridden-fields [gui-text-pb-field-index]}]}

                "/room_two.gui"
                {:nodes
                 [{:type :type-template
                   :id "referenced_shelf_one"
                   :template "/shelf.gui"}
                  {:type :type-template
                   :template-node-child true
                   :id "referenced_shelf_one/referenced_book"
                   :parent "referenced_shelf_one"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_shelf_one/referenced_book/book_text"
                   :parent "referenced_shelf_one/referenced_book"
                   :text "default text from shelf.gui"
                   :overridden-fields [gui-text-pb-field-index]}
                  {:type :type-template
                   :id "referenced_shelf_two"
                   :template "/shelf.gui"}
                  {:type :type-template
                   :template-node-child true
                   :id "referenced_shelf_two/referenced_book"
                   :parent "referenced_shelf_two"}
                  {:type :type-text
                   :template-node-child true
                   :id "referenced_shelf_two/referenced_book/book_text"
                   :parent "referenced_shelf_two/referenced_book"
                   :text "default text from shelf.gui"
                   :overridden-fields [gui-text-pb-field-index]}]}}
               (select-save-values project #{"/shelf.gui" "/room_one.gui" "/room_two.gui"})))))))
