(ns editor.yamlparser-test
  (:import [java.io StringReader])
  (:require [editor.yamlparser :as yp]
            [clojure.test :as t]))

(def yaml-text "- name: facebook
  type: TABLE
  desc: Functions and constants for interacti
               g with Facebook APIs
               asdasdasd
  members:
  - name: AUDIENCE_EVERYONE
    type: ENUM
    desc: publish permission to reach everyone

  - name: permissions
    type: FUNCTION
    desc: This function returns a table with all the currently granted permission strings.
    parameters:
    - name: asdasd
      type: [hash, msg.url, nil]
      optional: true

    - name: asdasd
      type:
        - name:
    - name: extra_permissions
      type: TABLE
      desc: list of extra permissions to request
      members:
        - name: self
          type: USERDATA
    returns:
    - name: permissions
      type: TABLE
      desc: list of granted permissions")

(def parsed-yaml
  [{"name" "facebook",
    "type" "TABLE",
    "desc"
    "Functions and constants for interacti g with Facebook APIs asdasdasd",
    "members"
    [{"name" "AUDIENCE_EVERYONE",
      "type" "ENUM",
      "desc" "publish permission to reach everyone"}
     {"name" "permissions",
      "type" "FUNCTION",
      "desc"
      "This function returns a table with all the currently granted permission strings.",
      "parameters"
      [{"name" "asdasd", "type" ["hash" "msg.url" "nil"], "optional" true}
       {"name" "asdasd", "type" [{"name" nil}]}
       {"name" "extra_permissions",
        "type" "TABLE",
        "desc" "list of extra permissions to request",
        "members" [{"name" "self", "type" "USERDATA"}]}],
      "returns"
      [{"name" "permissions",
        "type" "TABLE",
        "desc" "list of granted permissions"}]}]}])

(def parsed-yaml-with-keywords
  [{:name "facebook",
    :type "TABLE",
    :desc "Functions and constants for interacti g with Facebook APIs asdasdasd",
    :members
    [{:name "AUDIENCE_EVERYONE",
      :type "ENUM",
      :desc "publish permission to reach everyone"}
     {:name "permissions",
      :type "FUNCTION",
      :desc
      "This function returns a table with all the currently granted permission strings.",
      :parameters
      [{:name "asdasd", :type ["hash" "msg.url" "nil"], :optional true}
       {:name "asdasd", :type [{:name nil}]}
       {:name "extra_permissions",
        :type "TABLE",
        :desc "list of extra permissions to request",
        :members [{:name "self", :type "USERDATA"}]}],
      :returns
      [{:name "permissions",
        :type "TABLE",
        :desc "list of granted permissions"}]}]}])

(t/deftest Loading
  (t/testing "Can load YAML from a reader."
    (t/is (= parsed-yaml (yp/load (StringReader. yaml-text)))))
  (t/testing "Can load YAML from a string."
    (t/is (= parsed-yaml (yp/load yaml-text))))
  (t/testing "Can convert map keys to keywords."
    (t/is (= parsed-yaml-with-keywords (yp/load yaml-text keyword)))))


