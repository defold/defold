(ns editor.prefs-test
  (:require [clojure.edn :as edn]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.prefs :as prefs]))

(prefs/register-schema!
  ::types
  {:type :object
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
                                     :tuple {:type :tuple :items [{:type :string} {:type :keyword :default :code-view}]}}}}})
(deftest prefs-types-test
  (let [p (prefs/make :scopes {:user (fs/create-temp-file! "user" "test-prefs.edn")}
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
           (prefs/get p [])))))

(prefs/register-schema! ::unregistered-key {:type :object :properties {:name {:type :string}}})
(deftest get-unregistered-key-test
  (let [p (prefs/make :scopes {:user (fs/create-temp-file! "user" "test-prefs.edn")}
                      :schemas [::unregistered-key])]
    (is (= {:name ""} (prefs/get p [])))
    (is (nil? (prefs/get p [:undefined])))))

(prefs/register-schema! ::utf8 {:type :string})
(deftest utf8-handling-test
  (let [file (fs/create-temp-file! "user" "test-prefs.edn")
        p (prefs/make :scopes {:user file}
                      :schemas [::utf8])]
    (is (= "" (prefs/get p [])))
    (prefs/set! p [] "τεστ")
    (is (= "τεστ" (prefs/get p [])))
    (prefs/sync!)
    (is (= "\"τεστ\"" (slurp file)))))

(prefs/register-schema! ::invalid-edn {:type :object :properties {:name {:type :string}}})
(deftest invalid-edn-test
  (let [file (fs/create-temp-file! "user" "test-prefs.edn")
        _ (spit file "{")
        p (prefs/make :scopes {:user file}
                      :schemas [::invalid-edn])]
    (is (= "" (prefs/get p [:name])))))

(prefs/register-schema!
  ::scopes
  {:type :object
   :properties {:user-property {:type :boolean :scope :user}
                :project-property {:type :boolean :scope :project}}})
(deftest scopes-test
  (testing "write to scope files"
    (let [user-file (fs/create-temp-file! "user" "test-prefs.edn")
          project-file (fs/create-temp-file! "project" "test-prefs.edn")
          p (prefs/make :scopes {:user user-file :project project-file}
                        :schemas [::scopes])]
      (is (= {:user-property false
              :project-property false}
             (prefs/get p [])))
      (prefs/set! p [] {:user-property true
                        :project-property true})
      (is (= {:user-property true
              :project-property true}
             (prefs/get p [])))
      (prefs/sync!)
      (is (= {:user-property true} (edn/read-string (slurp user-file))))
      (is (= {:project-property true} (edn/read-string (slurp project-file))))))
  (testing "read from scope files"
    (let [user-file (doto (fs/create-temp-file! "user" "test-prefs.edn")
                      (spit "{:user-property true}"))
          project-file (doto (fs/create-temp-file! "project" "test-prefs.edn")
                         (spit "{:project-property true}"))
          p (prefs/make :scopes {:user user-file :project project-file}
                        :schemas [::scopes])]
      (is (= {:user-property true
              :project-property true}
             (prefs/get p []))))))

(prefs/register-schema!
  ::composition-ios
  {:type :object
   :properties {:bundle {:type :object
                         :properties {:ios {:type :object
                                            :properties {:install {:type :boolean :scope :project}
                                                         :ios-deploy-path {:type :string}}}}}}})
(prefs/register-schema!
  ::composition-android
  {:type :object
   :properties {:bundle {:type :object
                         :properties {:android {:type :object
                                                :properties {:install {:type :boolean :scope :project}
                                                             :adb-path {:type :string}}}}}}})
(deftest scope-composition-test
  (let [user-file (fs/create-temp-file! "user" "test-prefs.edn")
        project-file (fs/create-temp-file! "project" "test-prefs.edn")
        p (prefs/make :scopes {:user user-file
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
             (edn/read-string (slurp user-file))))
      (is (= {:bundle {:android {:install true}
                       :ios {:install true}}}
             (edn/read-string (slurp project-file))))))
  (testing "same file for both scopes"
    (let [file (fs/create-temp-file! "both" "test-prefs.edn")
          p (prefs/make :scopes {:user file
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
             (edn/read-string (slurp file)))))))

(prefs/register-schema!
  ::composition-conflict-str
  {:type :object
   :properties {:timeout {:type :string :description "e.g. 30s, 10ms"}}})
(prefs/register-schema!
  ::composition-conflict-num
  {:type :object
   :properties {:timeout {:type :number :description "timeout in milliseconds"}}})
(deftest conflicting-schemas-test
  (let [user-file (fs/create-temp-file! "user" "test-prefs.edn")
        p-str (prefs/make :scopes {:user user-file}
                          :schemas [::composition-conflict-str ::composition-conflict-num])
        p-num (prefs/make :scopes {:user user-file}
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
      (is (= "" (prefs/get p-str [:timeout]))))))
