;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.yaml-test
  (:require [clojure.test :refer :all]
            [editor.yaml :as yaml])
  (:import [java.io StringReader]))

(def yaml-text "
- name: facebook
  type: table
  desc: Functions and constants for interacti
               g with Facebook APIs
               asdasdasd
  members:
  - name: AUDIENCE_EVERYONE
    type: enum
    desc: publish permission to reach everyone

  - name: permissions
    type: function
    desc: This function returns a table with all the currently granted permission strings.
    parameters:
    - name: asdasd
      type: [hash, msg.url, nil]
      optional: true

    - name: asdasd
      type:
        - name:
    - name: extra_permissions
      type: table
      desc: list of extra permissions to request
      members:
        - name: self
          type: userdata
    returns:
    - name: permissions
      type: table
      desc: list of granted permissions")

(def parsed-yaml
  [{"name" "facebook",
    "type" "table",
    "desc"
    "Functions and constants for interacti g with Facebook APIs asdasdasd",
    "members"
    [{"name" "AUDIENCE_EVERYONE",
      "type" "enum",
      "desc" "publish permission to reach everyone"}
     {"name" "permissions",
      "type" "function",
      "desc"
      "This function returns a table with all the currently granted permission strings.",
      "parameters"
      [{"name" "asdasd", "type" ["hash" "msg.url" "nil"], "optional" true}
       {"name" "asdasd", "type" [{"name" nil}]}
       {"name" "extra_permissions",
        "type" "table",
        "desc" "list of extra permissions to request",
        "members" [{"name" "self", "type" "userdata"}]}],
      "returns"
      [{"name" "permissions",
        "type" "table",
        "desc" "list of granted permissions"}]}]}])

(def parsed-yaml-with-keywords
  [{:name "facebook",
    :type "table",
    :desc "Functions and constants for interacti g with Facebook APIs asdasdasd",
    :members
    [{:name "AUDIENCE_EVERYONE",
      :type "enum",
      :desc "publish permission to reach everyone"}
     {:name "permissions",
      :type "function",
      :desc
      "This function returns a table with all the currently granted permission strings.",
      :parameters
      [{:name "asdasd", :type ["hash" "msg.url" "nil"], :optional true}
       {:name "asdasd", :type [{:name nil}]}
       {:name "extra_permissions",
        :type "table",
        :desc "list of extra permissions to request",
        :members [{:name "self", :type "userdata"}]}],
      :returns
      [{:name "permissions",
        :type "table",
        :desc "list of granted permissions"}]}]}])

(deftest loading
  (testing "Can load YAML from a reader."
    (is (= parsed-yaml (yaml/load (StringReader. yaml-text)))))
  (testing "Can load YAML from a string."
    (is (= parsed-yaml (yaml/load yaml-text))))
  (testing "Can convert map keys to keywords."
    (is (= parsed-yaml-with-keywords (yaml/load yaml-text keyword)))))

(def dumped-yaml-text
  "- name: facebook
  type: table
  desc: Functions and constants for interacti g with Facebook APIs asdasdasd
  members:
  - {name: AUDIENCE_EVERYONE, type: enum, desc: publish permission to reach everyone}
  - name: permissions
    type: function
    desc: This function returns a table with all the currently granted permission strings.
    parameters:
    - name: asdasd
      type: [hash, msg.url, nil]
      optional: true
    - name: asdasd
      type:
      - {name: null}
    - name: extra_permissions
      type: table
      desc: list of extra permissions to request
      members:
      - {name: self, type: userdata}
    returns:
    - {name: permissions, type: table, desc: list of granted permissions}
")

(deftest dumping
  (testing "Can dump parsed YAML"
    (is (= dumped-yaml-text (yaml/dump parsed-yaml))))
  (testing "Can dump YAML with keywords"
    (is (= dumped-yaml-text (yaml/dump parsed-yaml-with-keywords)))))

(deftest round-tripping
  (testing "Round-trips data structures"
    (is (= parsed-yaml (yaml/load (yaml/dump parsed-yaml)))))
  (testing "Round-trips data structures with keywords"
    (is (= parsed-yaml-with-keywords (yaml/load (yaml/dump parsed-yaml-with-keywords) keyword))))
  (testing "Round-trips yaml text"
    (is (= dumped-yaml-text (yaml/dump (yaml/load dumped-yaml-text)))))
  (testing "Round-trips yaml text with keywords"
    (is (= dumped-yaml-text (yaml/dump (yaml/load dumped-yaml-text keyword))))))

(deftest order-test
  (testing "Simple Reordering"
    (is (= "foo: 1\nbar: [2]\n" (yaml/dump {:foo 1 :bar [2]})))
    (is (= "bar: [2]\nfoo: 1\n" (yaml/dump {:foo 1 :bar [2]} :order-pattern [:bar :foo]))))
  (testing "Nested reordering"
    (is (= "foo:\n  bar:\n    baz:\n    - y: 1\n      x: [2]\n"
           (yaml/dump {:foo {:bar {:baz [{:y 1 :x [2]}]}}})))
    (is (= "foo:\n  bar:\n    baz:\n    - x: [2]\n      y: 1\n"
           (yaml/dump {:foo {:bar {:baz [{:y 1 :x [2]}]}}}
                      :order-pattern [[:foo [[:bar [[:baz [:x :y]]]]]]]))))
  (testing "Order pattern is applied across collections"
    (is (= "foo: {x: 2, y: 1}\n" (yaml/dump {:foo {:y 1 :x 2}}
                                            :order-pattern [[:foo [:x :y]]])))
    (is (= "foo:\n- {x: 2, y: 1}\n" (yaml/dump {:foo [{:y 1 :x 2}]}
                                               :order-pattern [[:foo [:x :y]]])))))