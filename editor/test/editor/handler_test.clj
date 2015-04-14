(ns editor.handler-test
  (:require [clojure.test :refer :all]
            [editor.handler :as handler]))

(defn fixture [f]
  (reset! handler/*handlers* {})
  (f))

(use-fixtures :each fixture)

(deftest run
  (handler/defhandler my :open :project
    (visible? [instance] (= instance :foo))
    (enabled? [instance] (= instance :foo))
    (run [instance] 123))
  (are [ctx inst exp] (= exp (handler/enabled? :open ctx {:instance inst}))
       :project :foo true
       :project :bar false
       :assets :foo false
       :assets :bar false)
  (are [ctx inst exp] (= exp (handler/visible? :open ctx {:instance inst}))
       :project :foo true
       :project :bar false
       :assets :foo false
       :assets :bar false)
  (is (= 123 (handler/run :open :project {:instance :foo}))))

