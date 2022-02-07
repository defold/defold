;; Copyright 2020 The Defold Foundation
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

(ns editor.yamlparser-test
  (:import [java.io StringReader])
  (:require [editor.yamlparser :as yp]
            [clojure.test :refer :all]))

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

(deftest Loading
  (testing "Can load YAML from a reader."
    (is (= parsed-yaml (yp/load (StringReader. yaml-text)))))
  (testing "Can load YAML from a string."
    (is (= parsed-yaml (yp/load yaml-text))))
  (testing "Can convert map keys to keywords."
    (is (= parsed-yaml-with-keywords (yp/load yaml-text keyword)))))
