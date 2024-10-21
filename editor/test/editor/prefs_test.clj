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

(ns editor.prefs-test
  (:require [clojure.edn :as edn]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.prefs :as prefs]))

(defmacro with-schemas [id->schema & body]
  `(try
     ~@(map (fn [[id schema]]
              `(prefs/register-schema! ~id ~schema))
            id->schema)
     (do ~@body)
     (finally
       ~@(map (fn [id]
                `(prefs/unregister-schema! ~id))
              (keys id->schema)))))

(deftest prefs-types-test
  (with-schemas {::types {:type :object
                          :properties {:types {:type :object
                                               :properties {:any {:type :any}
                                                            :boolean {:type :boolean :default true}
                                                            :string {:type :string}
                                                            :keyword {:type :keyword :default :none}
                                                            :integer {:type :integer :default -1}
                                                            :number {:type :number :default 0.5}
                                                            :array {:type :array :item {:type :string}}
                                                            :set {:type :set :item {:type :string}}
                                                            :enum {:type :enum :values [:foo :bar]}
                                                            :tuple {:type :tuple :items [{:type :string} {:type :keyword :default :code-view}]}}}}}}
    (let [p (prefs/make :scopes {:global (fs/create-temp-file! "global" "test.editor_settings")}
                        :schemas [::types])]
      ;; ensure defaults are properly typed
      (is (= {:types {:any nil
                      :boolean true
                      :string ""
                      :keyword :none
                      :integer -1
                      :number 0.5
                      :array []
                      :set #{}
                      :enum :foo
                      :tuple ["" :code-view]}}
             (prefs/get p [])))
      ;; change values
      (prefs/set! p [] {:types {:any 'foo/bar
                                :boolean false
                                :string "str"
                                :keyword :something
                                :integer 42
                                :number 42
                                :array ["heh"]
                                :set #{"heh"}
                                :enum :bar
                                :tuple ["/game.project" :form-view]}})
      ;; ensure updated values are as expected
      (is (= {:types {:any 'foo/bar
                      :boolean false
                      :string "str"
                      :keyword :something
                      :integer 42
                      :number 42
                      :array ["heh"]
                      :set #{"heh"}
                      :enum :bar
                      :tuple ["/game.project" :form-view]}}
             (prefs/get p [])))
      ;; set to invalid values (expect a single correct field — :string)
      (prefs/set! p [] {:types {:boolean "not-a-boolean"
                                :string "STILL A STRING"
                                :keyword "not-a-keyword"
                                :integer "not-an-int"
                                :number "NaN"
                                :array true
                                :set false
                                :enum 12
                                :tuple nil}})
      ;; only a valid preference is applied
      (is (= {:types {:any 'foo/bar
                      :boolean false
                      :string "STILL A STRING"
                      :keyword :something
                      :integer 42
                      :number 42
                      :array ["heh"]
                      :set #{"heh"}
                      :enum :bar
                      :tuple ["/game.project" :form-view]}}
             (prefs/get p []))))))

(deftest get-unregistered-key-test
  (with-schemas {::unregistered-key {:type :object :properties {:name {:type :string}}}}
    (let [p (prefs/make :scopes {:global (fs/create-temp-file! "global" "test.editor_settings")}
                        :schemas [::unregistered-key])]
      (is (= {:name ""} (prefs/get p [])))
      (is (nil? (prefs/get p [:undefined]))))))

(deftest utf8-handling-test
  (with-schemas {::utf8 {:type :string}}
    (let [file (fs/create-temp-file! "global" "test.editor_settings")
          p (prefs/make :scopes {:global file}
                        :schemas [::utf8])]
      (is (= "" (prefs/get p [])))
      (prefs/set! p [] "τεστ")
      (is (= "τεστ" (prefs/get p [])))
      (prefs/sync!)
      (is (= "\"τεστ\"" (slurp file))))))

(deftest invalid-edn-test
  (with-schemas {::invalid-edn {:type :object :properties {:name {:type :string}}}}
    (let [file (fs/create-temp-file! "global" "test.editor_settings")
          _ (spit file "{")
          p (prefs/make :scopes {:global file}
                        :schemas [::invalid-edn])]
      (is (= "" (prefs/get p [:name]))))))

(deftest scopes-test
  (with-schemas {::scopes
                 {:type :object
                  :properties {:global-property {:type :boolean :scope :global}
                               :project-property {:type :boolean :scope :project}}}}
    (testing "write to scope files"
      (let [global-file (fs/create-temp-file! "global" "test.editor_settings")
            project-file (fs/create-temp-file! "project" "test.editor_settings")
            p (prefs/make :scopes {:global global-file :project project-file}
                          :schemas [::scopes])]
        (is (= {:global-property false
                :project-property false}
               (prefs/get p [])))
        (prefs/set! p [] {:global-property true
                          :project-property true})
        (is (= {:global-property true
                :project-property true}
               (prefs/get p [])))
        (prefs/sync!)
        (is (= {:global-property true} (edn/read-string (slurp global-file))))
        (is (= {:project-property true} (edn/read-string (slurp project-file))))))
    (testing "read from scope files"
      (let [global-file (doto (fs/create-temp-file! "global" "test.editor_settings")
                          (spit "{:global-property true}"))
            project-file (doto (fs/create-temp-file! "project" "test.editor_settings")
                           (spit "{:project-property true}"))
            p (prefs/make :scopes {:global global-file :project project-file}
                          :schemas [::scopes])]
        (is (= {:global-property true
                :project-property true}
               (prefs/get p [])))))))

(deftest scope-composition-test
  (with-schemas {::composition-ios
                 {:type :object
                  :properties {:bundle {:type :object
                                        :properties {:ios {:type :object
                                                           :properties {:install {:type :boolean :scope :project}
                                                                        :ios-deploy-path {:type :string}}}}}}}
                 ::composition-android
                 {:type :object
                  :properties {:bundle {:type :object
                                        :properties {:android {:type :object
                                                               :properties {:install {:type :boolean :scope :project}
                                                                            :adb-path {:type :string}}}}}}}}
    (let [global-file (fs/create-temp-file! "global" "test.editor_settings")
          project-file (fs/create-temp-file! "project" "test.editor_settings")
          p (prefs/make :scopes {:global global-file
                                 :project project-file}
                        :schemas [::composition-ios ::composition-android])]
      (testing "get prefs defined by multiple schemas returns a merged map"
        (is (= {:bundle {:ios {:install false
                               :ios-deploy-path ""}
                         :android {:install false
                                   :adb-path ""}}}
               (prefs/get p []))))
      (testing "set/get of prefs works when multiple schemas define the pref map"
        (prefs/set! p [:bundle] {:ios {:install true
                                       :ios-deploy-path "/opt/homebrew/bin/ios-deploy"}
                                 :android {:install true
                                           :adb-path "/opt/homebrew/bin/adb"}})
        (is (= {:bundle {:ios {:install true
                               :ios-deploy-path "/opt/homebrew/bin/ios-deploy"}
                         :android {:install true
                                   :adb-path "/opt/homebrew/bin/adb"}}}
               (prefs/get p []))))
      (prefs/sync!)
      (testing "multi-schema prefs write to the expected scope files"
        (is (= {:bundle {:android {:adb-path "/opt/homebrew/bin/adb"}
                         :ios {:ios-deploy-path "/opt/homebrew/bin/ios-deploy"}}}
               (edn/read-string (slurp global-file))))
        (is (= {:bundle {:android {:install true}
                         :ios {:install true}}}
               (edn/read-string (slurp project-file))))))
    (testing "same file for both scopes"
      (let [file (fs/create-temp-file! "both" "test.editor_settings")
            p (prefs/make :scopes {:global file
                                   :project file}
                          :schemas [::composition-ios ::composition-android])]
        (is (= {:bundle {:ios {:install false
                               :ios-deploy-path ""}
                         :android {:install false
                                   :adb-path ""}}}
               (prefs/get p [])))
        (prefs/set! p [] {:bundle {:ios {:install true
                                         :ios-deploy-path "/opt/homebrew/bin/ios-deploy"}
                                   :android {:install true
                                             :adb-path "/opt/homebrew/bin/adb"}}})
        (is (= {:bundle {:ios {:install true
                               :ios-deploy-path "/opt/homebrew/bin/ios-deploy"}
                         :android {:install true
                                   :adb-path "/opt/homebrew/bin/adb"}}}
               (prefs/get p [])))
        (prefs/sync!)
        (is (= {:bundle {:android {:adb-path "/opt/homebrew/bin/adb"
                                   :install true}
                         :ios {:install true
                               :ios-deploy-path "/opt/homebrew/bin/ios-deploy"}}}
               (edn/read-string (slurp file))))))))

(deftest conflicting-schemas-test
  (with-schemas {::composition-conflict-str
                 {:type :object
                  :properties {:timeout {:type :string :description "e.g. 30s, 10ms"}}}
                 ::composition-conflict-num
                 {:type :object
                  :properties {:timeout {:type :number :description "timeout in milliseconds"}}}}
    (let [global-file (fs/create-temp-file! "global" "test.editor_settings")
          p-str (prefs/make :scopes {:global global-file}
                            :schemas [::composition-conflict-str ::composition-conflict-num])
          p-num (prefs/make :scopes {:global global-file}
                            :schemas [::composition-conflict-num ::composition-conflict-str])]
      (testing "former schema takes precedence over latter schemas"
        (is (= {:timeout ""} (prefs/get p-str [])))
        (is (= {:timeout 0.0} (prefs/get p-num []))))
      (testing "values set by conflicting prefs instance are not visible to other prefs"
        (prefs/set! p-str [:timeout] "10ms")
        (is (= "10ms" (prefs/get p-str [:timeout])))
        (is (= 0.0 (prefs/get p-num [:timeout])))
        (prefs/set! p-num [:timeout] 10.0)
        (is (= 10.0 (prefs/get p-num [:timeout])))
        (is (= "" (prefs/get p-str [:timeout])))))))
