;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.

(ns editor.engine-test
  (:require [clojure.test :refer :all]
            [editor.engine :as engine]))

(def ^:private start-unfocused-argument "--config=display.start_unfocused=1")

(deftest launch-arguments-focus-test
  (let [launch-arguments #'engine/launch-arguments]
    (is (= [] (launch-arguments nil nil false 0 true)))
    (is (= [start-unfocused-argument]
           (launch-arguments nil nil false 0 false)))
    (is (= ["--config=bootstrap.debug_init_script=/_defold/debugger/start.luac"]
           (launch-arguments nil nil true 0 true)))
    (is (= ["--config=project.instance_index=2"
            start-unfocused-argument
            "--config=display.start_unfocused=0"
            "--config=custom.value=1"]
           (launch-arguments nil
                             "--config=display.start_unfocused=0\n--config=custom.value=1"
                             false
                             2
                             false)))))

(deftest reboot-arguments-focus-test
  (let [reboot-arguments #'engine/reboot-arguments
        local-url "http://127.0.0.1:1234"
        focused-arguments [(str "--config=resource.uri=" local-url)
                           (str local-url "/game.projectc")]]
    (is (= focused-arguments
           (reboot-arguments {} local-url false true)))
    (is (= (conj focused-arguments start-unfocused-argument)
           (reboot-arguments {} local-url false false)))
    (is (= [(str "--config=resource.uri=" local-url)
            "--config=bootstrap.debug_init_script=/_defold/debugger/start.luac"
            (str local-url "/game.projectc")
            "--config=project.instance_index=2"
            start-unfocused-argument]
           (reboot-arguments {:instance-index 2} local-url true false)))))
