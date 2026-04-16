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

(ns editor.prefs-test
  (:require [clojure.edn :as edn]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.prefs :as prefs]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [integration.test-util :as test-util]
            [service.log :as log]))

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

(defn- value-error-data? [path]
  (fn [x]
    (and (= :value (::prefs/error x))
         (= path (:path x)))))

(defn- path-error-data? [path]
  (fn [x]
    (and (= :path (::prefs/error x))
         (= path (:path x)))))

(defspec boolean-schema-valid-spec 100
  (prop/for-all [b gen/boolean]
    (prefs/valid? {:type :boolean} b)))

(defspec boolean-schema-invalid-spec 10
  (prop/for-all [b (gen/such-that (complement boolean?) gen/any)]
    (not (prefs/valid? {:type :boolean} b))))

(defspec string-schema-valid-spec 100
  (prop/for-all [x gen/string]
    (prefs/valid? {:type :string} x)))

(defspec string-schema-invalid-spec 10
  (prop/for-all [x (gen/such-that (complement string?) gen/any)]
    (not (prefs/valid? {:type :string} x))))

(defspec password-schema-valid-spec 100
  (prop/for-all [x gen/string]
    (prefs/valid? {:type :password} x)))

(defspec password-schema-invalid-spec 10
  (prop/for-all [x (gen/such-that (complement string?) gen/any)]
    (not (prefs/valid? {:type :password} x))))

(defspec keyword-schema-valid-spec 100
  (prop/for-all [x gen/keyword]
    (prefs/valid? {:type :keyword} x)))

(defspec keyword-schema-invalid-spec 10
  (prop/for-all [x (gen/such-that (complement keyword?) gen/any)]
    (not (prefs/valid? {:type :keyword} x))))

(defspec integer-schema-valid-spec 100
  (prop/for-all [x gen/small-integer]
    (prefs/valid? {:type :integer} x)))

(defspec integer-schema-invalid-spec 10
  (prop/for-all [x (gen/such-that (complement int?) gen/any)]
    (not (prefs/valid? {:type :integer} x))))

(defspec number-schema-valid-spec 100
  (prop/for-all [x (gen/one-of [gen/double gen/small-integer])]
    (prefs/valid? {:type :number} x)))

(defspec number-schema-invalid-spec 10
  (prop/for-all [x (gen/such-that (complement number?) gen/any)]
    (not (prefs/valid? {:type :number} x))))

(defspec one-of-schema-valid-spec 100
  (prop/for-all [m (gen/one-of [gen/string gen/small-integer])]
    (prefs/valid? {:type :one-of :schemas [{:type :number} {:type :string}]} m)))

(defspec one-of-schema-invalid-spec 100
  (prop/for-all [m (gen/such-that #(and (not (string? %)) (not (number? %))) gen/any)]
    (not (prefs/valid? {:type :one-of :schemas [{:type :number} {:type :string}]} m))))

(defspec array-schema-valid-spec 100
  (prop/for-all [x (gen/vector gen/string)]
    (prefs/valid? {:type :array :item {:type :string}} x)))

(defspec array-schema-invalid-spec 10
  (prop/for-all [x (gen/such-that (complement vector?) gen/any)]
    (not (prefs/valid? {:type :array :item {:type :string}} x))))

(defspec array-schema-invalid-item-spec 10
  (prop/for-all [x (gen/not-empty (gen/vector (gen/such-that (complement string?) gen/any)))]
    (not (prefs/valid? {:type :array :item {:type :string}} x))))

(defspec set-schema-valid-spec 100
  (prop/for-all [x (gen/set gen/string)]
    (prefs/valid? {:type :set :item {:type :string}} x)))

(defspec set-schema-invalid-spec 10
  (prop/for-all [x (gen/such-that (complement set?) gen/any)]
    (not (prefs/valid? {:type :set :item {:type :string}} x))))

(defspec set-schema-invalid-item-spec 10
  (prop/for-all [x (gen/not-empty (gen/set (gen/such-that (complement string?) gen/any)))]
    (not (prefs/valid? {:type :set :item {:type :string}} x))))

(defspec enum-schema-valid-spec 100
  (prop/for-all [x (gen/elements [:a :b :c])]
    (prefs/valid? {:type :enum :values [:a :b :c]} x)))

(defspec enum-schema-invalid-spec 10
  (prop/for-all [x (gen/such-that (complement #{:a :b :c}) gen/any)]
    (not (prefs/valid? {:type :enum :values [:a :b :c]} x))))

(defspec tuple-schema-valid-spec 100
  (prop/for-all [s gen/string
                 i gen/small-integer]
    (prefs/valid? {:type :tuple :items [{:type :string} {:type :integer}]} [s i])))

(defspec tuple-schema-invalid-size-spec 100
  (prop/for-all [strings (gen/such-that #(not= 2 (count %)) (gen/vector gen/string))]
    (not (prefs/valid? {:type :tuple :items [{:type :string} {:type :string}]} strings))))

(defspec tuple-schema-invalid-type-spec 100
  (prop/for-all [not-s (gen/such-that (complement string?) gen/any)
                 not-i (gen/such-that (complement integer?) gen/any)]
    (not (prefs/valid? {:type :tuple :items [{:type :string} {:type :integer}]} [not-s not-i]))))

(defspec object-of-schema-valid-spec 100
  (prop/for-all [m (gen/map gen/string gen/double)]
    (prefs/valid? {:type :object-of :key {:type :string} :val {:type :number}} m)))

(defspec object-of-schema-invalid-spec 10
  (prop/for-all [m (gen/not-empty
                     (gen/map (gen/such-that (complement string?) gen/any)
                              (gen/such-that (complement number?) gen/any)))]
    (not (prefs/valid? {:type :object-of :key {:type :string} :val {:type :number}} m))))

(deftest prefs-types-test
  (with-schemas {::types {:type :object
                          :properties {:types {:type :object
                                               :properties {:boolean {:type :boolean :default true}
                                                            :string {:type :string}
                                                            :password {:type :password}
                                                            :keyword {:type :keyword :default :none}
                                                            :integer {:type :integer :default -1}
                                                            :number {:type :number :default 0.5}
                                                            :one-of {:type :one-of :schemas [{:type :number} {:type :string}]}
                                                            :array {:type :array :item {:type :string}}
                                                            :set {:type :set :item {:type :string}}
                                                            :enum {:type :enum :values [:foo :bar]}
                                                            :tuple {:type :tuple :items [{:type :string} {:type :keyword :default :code-view}]}
                                                            :object-of {:type :object-of :key {:type :string} :val {:type :string}}}}}}}
    (let [p (prefs/make :scopes {:global (fs/create-temp-file! "global" "test.editor_settings")}
                        :schemas [::types])]
      ;; ensure defaults are properly typed
      (is (= {:types {:boolean true
                      :string ""
                      :password ""
                      :keyword :none
                      :integer -1
                      :number 0.5
                      :one-of 0.0
                      :array []
                      :set #{}
                      :enum :foo
                      :tuple ["" :code-view]
                      :object-of {}}}
             (prefs/get p [])))
      ;; change values
      (prefs/set! p [] {:types {:boolean false
                                :string "str"
                                :password "1337"
                                :keyword :something
                                :integer 42
                                :number 42
                                :one-of "string"
                                :array ["heh"]
                                :set #{"heh"}
                                :enum :bar
                                :tuple ["/game.project" :form-view]
                                :object-of {"foo" "bar"}}})
      ;; ensure updated values are as expected
      (is (= {:types {:boolean false
                      :string "str"
                      :password "1337"
                      :keyword :something
                      :integer 42
                      :number 42
                      :one-of "string"
                      :array ["heh"]
                      :set #{"heh"}
                      :enum :bar
                      :tuple ["/game.project" :form-view]
                      :object-of {"foo" "bar"}}}
             (prefs/get p [])))
      ;; set to invalid values
      (test-util/check-thrown-with-data!
        (value-error-data? [:types :boolean])
        (prefs/set! p [] {:types {:boolean "not-a-boolean"}}))
      (test-util/check-thrown-with-data!
        (value-error-data? [:types :boolean])
        (prefs/set! p [:types] {:boolean "not-a-boolean"}))
      (run!
        (fn [[path value]]
          (test-util/check-thrown-with-data! (value-error-data? path) (prefs/set! p path value)))
        {[:types :boolean] "not-a-boolean"
         [:types :string] 12
         [:types :password] 1337
         [:types :keyword] "not-a-keyword"
         [:types :integer] "not-an-int"
         [:types :number] "NaN"
         [:types :one-of] false
         [:types :array] true
         [:types :set] false
         [:types :enum] 12
         [:types :tuple] nil
         [:types :object-of] {:foo :bar}})
      ;; No invalid changes are recorded
      (is (= {:types {:boolean false
                      :string "str"
                      :password "1337"
                      :keyword :something
                      :integer 42
                      :number 42
                      :one-of "string"
                      :array ["heh"]
                      :set #{"heh"}
                      :enum :bar
                      :tuple ["/game.project" :form-view]
                      :object-of {"foo" "bar"}}}
             (prefs/get p []))))))

(deftest set?-test
  (with-schemas {::test {:type :string}}
    (let [p (prefs/make :scopes {:global (fs/create-temp-file! "is-set-test" "test.editor_settings")}
                        :schemas [::test])]
      (is (false? (prefs/set? p [])))
      (prefs/set! p [] "new-string")
      (is (true? (prefs/set? p [])))))
  (with-schemas {::nested {:type :object
                           :properties
                           {:root {:type :object
                                   :properties
                                   {:a {:type :object
                                        :properties
                                        {:leaf {:type :string}}}
                                    :b {:type :object
                                        :properties
                                        {:leaf {:type :string}}}}}}}}
    (let [p (prefs/make :scopes {:global (fs/create-temp-file! "is-set-nested-test" "test.editor_settings")}
                        :schemas [::nested])]
      (is (not (prefs/set? p [:root :a :leaf])))
      (is (not (prefs/set? p [:root :a])))
      (is (not (prefs/set? p [:root :b :leaf])))
      (is (not (prefs/set? p [:root :b])))
      (is (not (prefs/set? p [:root])))
      (is (not (prefs/set? p [])))

      (prefs/set! p [:root :a :leaf] "changed")

      (is (prefs/set? p [:root :a :leaf]))
      (is (prefs/set? p [:root :a]))
      (is (not (prefs/set? p [:root :b :leaf])))
      (is (not (prefs/set? p [:root :b])))
      (is (prefs/set? p [:root]))
      (is (prefs/set? p [])))))

(deftest get-unregistered-key-test
  (with-schemas {::unregistered-key {:type :object :properties {:name {:type :string}}}}
    (let [p (prefs/make :scopes {:global (fs/create-temp-file! "global" "test.editor_settings")}
                        :schemas [::unregistered-key])]
      (is (= {:name ""} (prefs/get p [])))
      (test-util/check-thrown-with-data! (path-error-data? [:undefined]) (prefs/get p [:undefined])))))

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
    (log/without-logging ;; no need to additionally log the error here
      (let [file (fs/create-temp-file! "global" "test.editor_settings")
            _ (spit file "{")
            p (prefs/make :scopes {:global file}
                          :schemas [::invalid-edn])]
        (is (= "" (prefs/get p [:name])))))))

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

(deftest schema-merge-test
  (testing "no conflicts"
    (let [conflicts (atom {})]
      (is (= {:type :object
              :properties {:only-in-a {:type :string}
                           :only-in-b {:type :string}}}
             (prefs/merge-schemas
               (fn [a b path]
                 (swap! conflicts assoc path [a b])
                 a)
               {:type :object
                :properties {:only-in-a {:type :string}}}
               {:type :object
                :properties {:only-in-b {:type :string}}})))
      (is (= {} @conflicts))))
  (testing "a conflict: prefer left"
    ;; left -> integer, right -> string
    (let [conflicts (atom {})]
      (is (= {:type :object
              :properties {:only-in-a {:type :string}
                           :only-in-b {:type :string}
                           :present-in-both {:type :integer}}}
             (prefs/merge-schemas
               (fn [a b path]
                 (swap! conflicts assoc path [a b])
                 a)
               {:type :object
                :properties {:only-in-a {:type :string}
                             :present-in-both {:type :integer}}}
               {:type :object
                :properties {:present-in-both {:type :string}
                             :only-in-b {:type :string}}})))
      (is (= {[:present-in-both] [{:type :integer} {:type :string}]} @conflicts))))
  (testing "a conflict: prefer right"
    ;; left -> integer, right -> string
    (let [conflicts (atom {})]
      (is (= {:type :object
              :properties {:only-in-a {:type :string}
                           :only-in-b {:type :string}
                           :present-in-both {:type :string}}}
             (prefs/merge-schemas
               (fn [a b path]
                 (swap! conflicts assoc path [a b])
                 b)
               {:type :object
                :properties {:only-in-a {:type :string}
                             :present-in-both {:type :integer}}}
               {:type :object
                :properties {:present-in-both {:type :string}
                             :only-in-b {:type :string}}})))
      (is (= {[:present-in-both] [{:type :integer} {:type :string}]} @conflicts))))
  (testing "a conflict: exclude"
    ;; left -> integer, right -> string
    (let [conflicts (atom {})]
      (is (= {:type :object
              :properties {:only-in-a {:type :string}
                           :only-in-b {:type :string}}}
             (prefs/merge-schemas
               (fn [a b path]
                 (swap! conflicts assoc path [a b])
                 nil)
               {:type :object
                :properties {:only-in-a {:type :string}
                             :present-in-both {:type :integer}}}
               {:type :object
                :properties {:present-in-both {:type :string}
                             :only-in-b {:type :string}}})))
      (is (= {[:present-in-both] [{:type :integer} {:type :string}]} @conflicts)))))

(deftest schema-difference-test
  (testing "no conflict"
    (let [conflicts (atom {})]
      (is (= {:type :object
              :properties {:only-in-a {:type :string}}}
             (prefs/subtract-schemas
               (fn [a b path]
                 (swap! conflicts assoc path [a b]))
               {:type :object
                :properties {:only-in-a {:type :string}}}
               {:type :object
                :properties {:only-in-b {:type :string}}})))
      (is (= {} @conflicts))))
  (testing "top-level conflict"
    (let [conflicts (atom {})]
      (is (nil?
            (prefs/subtract-schemas
              (fn [a b path]
                (swap! conflicts assoc path [a b]))
              {:type :integer}
              {:type :string})))
      (is (= {[] [{:type :integer} {:type :string}]}
             @conflicts))))
  (testing "depth-1 conflict"
    (let [conflicts (atom {})]
      (is (= {:type :object
              :properties {:only-in-a {:type :string}}}
             (prefs/subtract-schemas
               (fn [a b path]
                 (swap! conflicts assoc path [a b]))
               {:type :object
                :properties {:only-in-a {:type :string}
                             :present-in-both {:type :integer}}}
               {:type :object
                :properties {:only-in-b {:type :string}
                             :present-in-both {:type :string}}})))
      (is (= {[:present-in-both] [{:type :integer} {:type :string}]}
             @conflicts))))
  (testing "depth-2 conflict"
    (let [conflicts (atom {})]
      (is (= {:type :object
              :properties {:only-in-a {:type :string}
                           :present-in-both {:type :object
                                             :properties {:only-in-a {:type :boolean}}}}}
             (prefs/subtract-schemas
               (fn [a b path]
                 (swap! conflicts assoc path [a b]))
               {:type :object
                :properties {:only-in-a {:type :string}
                             :present-in-both {:type :object
                                               :properties {:only-in-a {:type :boolean}
                                                            :conflict {:type :integer}}}}}
               {:type :object
                :properties {:only-in-b {:type :string}
                             :present-in-both {:type :object
                                               :properties {:conflict {:type :string}}}}})))
      (is (= {[:present-in-both :conflict] [{:type :integer} {:type :string}]}
             @conflicts)))))

(deftest safe-assoc-in-test
  (is (= "val" (prefs/safe-assoc-in {} [] "val")))
  (is (= {:a "val"} (prefs/safe-assoc-in {} [:a] "val")))
  (is (= {:a "val"} (prefs/safe-assoc-in 42 [:a] "val")))
  (is (= {:a {:b "val"}} (prefs/safe-assoc-in {:a 1} [:a :b] "val")))
  (is (= {:a {:b "val"}} (prefs/safe-assoc-in ::prefs/not-found [:a :b] "val")))
  (is (= {:a {:b "val" :other true}} (prefs/safe-assoc-in {:a {:other true}} [:a :b] "val")))
  (is (= {:a {:other true} :x "val"} (prefs/safe-assoc-in {:a {:other true}} [:x] "val"))))

(deftest secure-password-storage-test
  (with-schemas {::test {:type :password :default "pass"}}
    (testing "storage"
      (let [global-file (fs/create-temp-file! "global" "test.editor_settings")
            p (prefs/make :scopes {:global global-file}
                          :schemas [::test])]
        (is (= "pass" (prefs/get p [])))
        (prefs/set! p [] "secret")
        (is (= "secret" (prefs/get p [])))
        (prefs/sync!)
        (let [stored-value (edn/read-string (slurp global-file))]
          (is (string? stored-value))
          (is (not= "secret" stored-value)))))
    (testing "loading invalid human-edited password strings"
      (let [global-file (doto (fs/create-temp-file! "global" "test.editor_settings")
                          (spit "\"invalid base64 string\""))
            p (prefs/make :scopes {:global global-file}
                          :schemas [::test])]
        (is (not (prefs/set? p [])))
        (is (= "pass" (prefs/get p [])))))))

(deftest update-test
  (with-schemas {::test {:type :object
                         :properties {:counter {:type :integer}}}}
    (let [p (prefs/make :scopes {:global (fs/create-temp-file! "global" "test.editor_settings")}
                        :schemas [::test])
          thread-count 20
          inc-count-per-thread 1000]
      (->> #(future
              (dotimes [_ inc-count-per-thread]
                (prefs/update! p [:counter] inc)))
           (repeatedly thread-count)
           (vec)
           (run! deref))
      (is (= (* thread-count inc-count-per-thread) (prefs/get p [:counter]))))))