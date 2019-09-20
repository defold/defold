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
