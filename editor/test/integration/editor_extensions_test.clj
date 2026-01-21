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

(ns integration.editor-extensions-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions :as extensions]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.graph :as graph]
            [editor.editor-extensions.prefs-functions :as prefs-functions]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.vm :as vm]
            [editor.fs :as fs]
            [editor.future :as future]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.os :as os]
            [editor.pipeline.bob :as bob]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.web-server :as web-server]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [util.diff :as diff]
            [util.http-server :as http-server])
  (:import [java.nio.file Files]
           [java.nio.file.attribute PosixFilePermission]
           [java.util.zip ZipEntry]
           [org.apache.commons.compress.archivers.zip ZipArchiveInputStream]
           [org.luaj.vm2 LuaError]))

(set! *warn-on-reflection* true)

(deftest read-bind-test
  (test-support/with-clean-system
    (let [rt (rt/make)
          p (rt/read "return 1")]
      (is (= 1 (rt/->clj rt (rt/invoke-immediate-1 rt (rt/bind rt p))))))))

(deftest thread-safe-access-test
  (test-support/with-clean-system
    (let [rt (rt/make)
          _ (rt/invoke-immediate-1 rt (rt/bind rt (rt/read "global = -1")))
          inc-and-get (rt/read "return function () global = global + 1; return global end")
          lua-inc-and-get (rt/invoke-immediate-1 rt (rt/bind rt inc-and-get))
          ec (g/make-evaluation-context)
          threads 10
          per-thread-calls 1000
          iterations 100]
      (dotimes [i iterations]
        (let [results (->> (fn []
                             (future
                               (->> #(rt/invoke-immediate-1 rt lua-inc-and-get ec)
                                    (repeatedly per-thread-calls)
                                    (vec))))
                           (repeatedly threads)
                           (vec)                 ;; launch all threads in parallel
                           (mapcat deref)        ;; await
                           (map #(rt/->clj rt %)))
              expected-count (* threads per-thread-calls)]
          (when-not (and (is (= expected-count (count results)))
                         (is (distinct? results))
                         (is (= (range (* i expected-count) (* (inc i) expected-count))
                                (sort results))))
            (throw (Exception. "Lua runtime is not thread-safe!"))))))))

(deftest immediate-invocations-complete-while-suspending-invocations-are-suspended
  (test-support/with-clean-system
    (let [completable-future (future/make)
          rt (rt/make :env {"suspend_with_promise" (rt/suspendable-lua-fn [_] completable-future)
                            "no_suspend" (rt/lua-fn [_] (rt/->lua "immediate-result"))})
          calls-suspending (rt/invoke-immediate-1 rt (rt/bind rt (rt/read "return function() return suspend_with_promise() end ")))
          calls-immediate (rt/invoke-immediate-1 rt (rt/bind rt (rt/read "return function() return no_suspend() end")))
          suspended-future (rt/invoke-suspending-1 rt calls-suspending)]
      (is (false? (future/done? completable-future)))
      (is (= "immediate-result" (rt/->clj rt (rt/invoke-immediate-1 rt calls-immediate))))
      (future/complete! completable-future "suspended-result")
      (when (is (true? (future/done? completable-future)))
        (is (= "suspended-result" (rt/->clj rt @suspended-future)))))))

(deftest suspending-calls-without-suspensions-complete-immediately
  (test-support/with-clean-system
    (let [rt (rt/make)
          lua-fib (->> (rt/read "local function fib(n)
                                   if n <= 1 then
                                     return n
                                   else
                                     return fib(n - 1) + fib(n - 2)
                                   end
                                 end

                                 return fib")
                       (rt/bind rt)
                       (rt/invoke-immediate-1 rt))]
      ;; 30th fibonacci takes awhile to complete, but still done immediately
      (is (future/done? (rt/invoke-suspending-1 rt lua-fib (rt/->lua 30)))))))

(deftest suspending-calls-in-immediate-mode-are-disallowed
  (test-support/with-clean-system
    (let [rt (rt/make :env {"suspending" (rt/suspendable-lua-fn [_] (future/make))})
          calls-suspending (->> (rt/read "return function () suspending() end")
                                (rt/bind rt)
                                (rt/invoke-immediate-1 rt))]
      (is (thrown-with-msg?
            LuaError
            #"Cannot use long-running editor function in immediate context"
            (rt/invoke-immediate-1 rt calls-suspending))))))

(deftest user-coroutines-are-separated-from-system-coroutines
  (test-support/with-clean-system
    (let [rt (rt/make :env {"suspending" (rt/suspendable-lua-fn [{:keys [rt]} x]
                                           (inc (rt/->clj rt x)))})
          coromix (->> (rt/read "local function yield_twice(x)
                                   local y = coroutine.yield(suspending(x))
                                   coroutine.yield(suspending(y))
                                   return 'done'
                                 end

                                 return function(n)
                                   local co = coroutine.create(yield_twice)
                                   local success1, result1 = coroutine.resume(co, n)
                                   local success2, result2 = coroutine.resume(co, result1)
                                   local success3, result3 = coroutine.resume(co, result2)
                                   local success4, result4 = coroutine.resume(co, result3)
                                   return {
                                     {success1, result1},
                                     {success2, result2},
                                     {success3, result3},
                                     {success4, result4},
                                   }
                                 end")
                       (rt/bind rt)
                       (rt/invoke-immediate-1 rt))]
      (is (= [;; first yield: incremented input
              [true 6]
              ;; second yield: incremented again
              [true 7]
              ;; not a yield, but a return value
              [true "done"]
              ;; user coroutine done, nothing to return
              [false "cannot resume dead coroutine"]]
             (rt/->clj rt @(rt/invoke-suspending-1 rt coromix (rt/->lua 5))))))))

(deftest user-coroutines-work-normally-in-immediate-mode
  (test-support/with-clean-system
    (let [rt (rt/make)
          lua-fn (->> (rt/read "local function yields_twice()
                                  coroutine.yield(1)
                                  coroutine.yield(2)
                                  return 'done'
                                end

                                return function()
                                  local co = coroutine.create(yields_twice)
                                  local success1, result1 = coroutine.resume(co)
                                  local success2, result2 = coroutine.resume(co)
                                  local success3, result3 = coroutine.resume(co)
                                  local success4, result4 = coroutine.resume(co)
                                  return {
                                    {success1, result1},
                                    {success2, result2},
                                    {success3, result3},
                                    {success4, result4},
                                  }
                                end")
                      (rt/bind rt)
                      (rt/invoke-immediate-1 rt))]
      (is (= [;; first yield: 1
              [true 1]
              ;; second yield: 2
              [true 2]
              ;; not a yield, but a return value
              [true "done"]
              ;; user coroutine done, nothing to return
              [false "cannot resume dead coroutine"]]
             (rt/->clj rt (rt/invoke-immediate-1 rt lua-fn)))))))

(g/defnode TestNode
  (property value g/Any)
  (output value g/Any :cached (gu/passthrough value)))

(deftest suspendable-functions-can-refresh-contexts
  (test-support/with-clean-system
    (let [node-id (g/make-node! world TestNode :value 1)
          rt (rt/make :env {"get_value" (rt/lua-fn [{:keys [evaluation-context]}]
                                          (rt/->lua (g/node-value node-id :value evaluation-context)))
                            "set_value" (rt/suspendable-lua-fn [{:keys [rt]} n]
                                          (let [f (future/make)]
                                            (let [set-val! (bound-fn []
                                                             (g/set-property! node-id :value (rt/->clj rt n)))]
                                              (ui/run-later
                                                (set-val!)
                                                (future/complete! f (rt/and-refresh-context true))))
                                            f))})
          lua-fn (->> (rt/read "return function()
                                  local v1 = get_value()
                                  local change_result = set_value(2)
                                  local v2 = get_value()
                                  return {v1, change_result, v2}
                                end")
                      (rt/bind rt)
                      (rt/invoke-immediate-1 rt))]
      (is (= [;; initial value
              1
              ;; success notification about change
              true
              ;; updated value
              2]
             (rt/->clj rt @(rt/invoke-suspending-1 rt lua-fn)))))))


(deftest suspending-lua-failure-test
  (test-support/with-clean-system
    (let [rt (rt/make :env {"suspend_fail_immediately" (rt/suspendable-lua-fn [_]
                                                         (throw (LuaError. "failed immediately")))
                            "suspend_fail_async" (rt/suspendable-lua-fn [_]
                                                   (future/failed (LuaError. "failed async")))})
          lua-fn (->> (rt/read "return function()
                                  local success1, value1 = pcall(suspend_fail_immediately)
                                  local success2, value2 = pcall(suspend_fail_async)
                                  return {
                                    {success1, value1},
                                    {success2, value2},
                                  }
                                end")
                      (rt/bind rt)
                      (rt/invoke-immediate-1 rt))]
      (is (= [[false "failed immediately"]
              [false "failed async"]]
             (rt/->clj rt @(rt/invoke-suspending-1 rt lua-fn)))))))

(deftest immediate-failures-test
  (test-support/with-clean-system
    (let [rt (rt/make :env {"immediate_error" (rt/lua-fn [_]
                                                (throw (Exception. "fail")))})]
      (is
        (= [false "fail"]
           (->> (rt/read "local success1, result1 = pcall(immediate_error)
                          return {success1, result1}")
                (rt/bind rt)
                (rt/invoke-immediate-1 rt)
                (rt/->clj rt)))))))

(deftype StaticSelection [selection]
  handler/SelectionProvider
  (selection [_this _evaluation-context] selection)
  (succeeding-selection [_this _evaluation-context] [])
  (alt-selection [_this _evaluation-context] []))

(defn- make-reload-resources-fn [workspace]
  (let [resource-sync (bound-fn* workspace/resource-sync!)]
    (fn reload-resources! []
      (resource-sync workspace)
      (future/completed nil))))

(defn- make-save-fn [project]
  (let [save-project! (bound-fn* test-util/save-project!)]
    (fn save! []
      (save-project! project)
      (future/completed nil))))

(defn- open-resource-noop! [_]
  (future/completed nil))

(defn- make-invoke-bob-fn [project]
  (fn invoke-bob! [options commands _]
    (future/io
      (let [ret (bob/invoke! project options commands)]
        (when (or (:error ret) (:exception ret))
          (throw (LuaError. "Bob invocation failed")))))))

(def ^:private stopped-server
  (http-server/stop! (http-server/start! (web-server/make-dynamic-handler [])) 0))

(defn- reload-editor-scripts! [project & {:keys [display-output! open-resource! prefs web-server]
                                          :or {display-output! println
                                               open-resource! open-resource-noop!
                                               web-server stopped-server}}]
  (extensions/reload! project :all
                      :prefs (or prefs (test-util/make-test-prefs))
                      :localization test-util/localization
                      :reload-resources! (make-reload-resources-fn (project/workspace project))
                      :display-output! display-output!
                      :save! (make-save-fn project)
                      :open-resource! open-resource!
                      :invoke-bob! (make-invoke-bob-fn project)
                      :web-server web-server))

(defn- eval-handler-contexts [context-name selection]
  (let [selection-provider (->StaticSelection selection)
        command-context (handler/->context context-name {} selection-provider)]
    (g/with-auto-evaluation-context evaluation-context
      (handler/eval-contexts [command-context] false evaluation-context))))

(deftest editor-scripts-commands-test
  (test-util/with-loaded-project "test/resources/editor_extensions/commands_project"
    (let [sprite-outline (:node-id (test-util/outline (test-util/resource-node project "/main/main.collection") [0 0]))]
      (reload-editor-scripts! project)
      (let [handler+context (handler/active
                              (:command (first (handler/realize-menu :editor.outline-view/context-menu-end)))
                              (eval-handler-contexts :outline [sprite-outline])
                              {})]
        (is (= [0.0 0.0 0.0] (test-util/prop sprite-outline :position)))
        (is (= 1.0 (test-util/prop sprite-outline :playback-rate)))
        (is (some? handler+context))
        (is (handler/enabled? handler+context))
        (is (nil?
              (try
                @(handler/run handler+context)
                nil
                (catch Throwable e e))))
        (is (= [1.5 1.5 1.5] (test-util/prop sprite-outline :position)))
        (is (= 2.5 (test-util/prop sprite-outline :playback-rate))))
      (let [handler+context (handler/active
                              (:command (first (handler/realize-menu :editor.scene-selection/context-menu-end)))
                              (eval-handler-contexts :global [sprite-outline])
                              {})]
        (is (= [1.0 1.0 1.0] (test-util/prop sprite-outline :scale)))
        (is (some? handler+context))
        (is (handler/enabled? handler+context))
        (is (nil?
              (try
                @(handler/run handler+context)
                nil
                (catch Throwable e e))))
        (is (= [2 2 2] (test-util/prop sprite-outline :scale)))))))

(deftest refresh-context-after-write-test
  (test-util/with-scratch-project "test/resources/editor_extensions/refresh_context_project"
    (let [output (atom [])
          _ (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
          handler+context (handler/active
                            (:command (first (handler/realize-menu :editor.asset-browser/context-menu-end)))
                            (eval-handler-contexts :asset-browser [(test-util/resource-node project "/test.txt")])
                            {})]
      @(handler/run handler+context)
      ;; see test.editor_script:
      ;; first, it gets the selection text (Initial content)
      ;; then, it sets it to another value (Another text!) and gets the text again
      ;; finally, to clean up, it sets the text to initial value (Initial content)
      ;; In the end, it prints the line with old, new and reverted texts that we
      ;; capture in the test output.
      (is (= [[:out "old = Initial content, new = Another text!, reverted = Initial content"]]
             @output)))))

(defn- run-edit-menu-test-command! []
  (let [handler+context (handler/active
                          (:command (last (handler/realize-menu :editor.app-view/edit-end)))
                          (eval-handler-contexts :global [])
                          {})]
    (assert handler+context "Test bug: undefined test command")
    @(handler/run handler+context)))

(deftest execute-test
  (test-util/with-loaded-project "test/resources/editor_extensions/execute_test"
    (let [output (atom [])]
      (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
      (run-edit-menu-test-command!)
      ;; see test.editor_script:
      ;; first, it tries to execute `git bleh`, catches the error, then prints it.
      ;; second, it captures the output of `git log --oneline --max-count=10` and
      ;; prints the hashes.
      (is (= (into [[:out "false\tCommand \"git bleh\" exited with code 1"]]
                   (map (fn [line]
                          [:out (re-find #"\w+" line)]))
                   (string/split-lines
                     (process/exec! "git" "log" "--oneline" "--max-count=10")))
            @output)))))

(deftest transact-test
  (test-util/with-loaded-project "test/resources/editor_extensions/transact_test"
    (let [output (atom [])
          _ (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
          node (:node-id (test-util/outline (test-util/resource-node project "/main/main.collection") [0 0]))
          handler+context (handler/active
                            (:command (first (handler/realize-menu :editor.outline-view/context-menu-end)))
                            (eval-handler-contexts :outline [node])
                            {})
          test-initial-state! (fn test-initial-state! []
                                (is (= "properties" (test-util/prop node :id)))
                                (is (= 0.0 (test-util/prop node :__num)))
                                (is (= false (test-util/prop node :__boolean)))
                                (is (nil? (test-util/prop node :__resource)))
                                (is (= [0.0 0.0 0.0] (test-util/prop node :__vec3)))
                                (is (= [0.0 0.0 0.0 0.0] (test-util/prop node :__vec4))))]

      ;; initial state of the node
      (test-initial-state!)

      ;; see test.editor_script: it records old id, then transacts 6 properties
      ;; at once, then prints new id and old id
      @(handler/run handler+context)
      (is (= [[:out "old id = properties, new id = My node id"]] @output))

      ;; properties changed
      (is (= "My node id" (test-util/prop node :id)))
      (is (= 15.5 (test-util/prop node :__num)))
      (is (= true (test-util/prop node :__boolean)))
      (is (some? (test-util/prop node :__resource)))
      (is (= [1 2 3] (test-util/prop node :__vec3)))
      (is (= [1 2 3 4] (test-util/prop node :__vec4)))

      ;; single undo
      (g/undo! (g/node-id->graph-id project))

      ;; all the changes should be reverted — a single transaction!
      (test-initial-state!))))

(deftest save-test
  (test-util/with-scratch-project "test/resources/editor_extensions/save_test"
    (let [output (atom [])
          _ (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
          handler+context (handler/active
                            (:command (first (handler/realize-menu :editor.asset-browser/context-menu-end)))
                            (eval-handler-contexts :asset-browser [])
                            {})]
      @(handler/run handler+context)
      ;; see test.editor_script: it uses editor.transact() to set a file text, then reads
      ;; the file text from file system, then saves, then reads it again.
      (is (= [[:out "file read: before save = 'Initial text', after save = 'New text'"]]
             @output)))))

(deftest resource-attributes-test
  (test-util/with-loaded-project "test/resources/editor_extensions/resource_attributes_project"
    (let [output (atom [])]
      (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
      (run-edit-menu-test-command!)
      ;; see test.editor script: it uses editor.resource_attributes with different resource
      ;; paths and prints results
      (is (= [[:out "test '/': exists = true, file = false, directory = true)"]
              [:out "test '/game.project': exists = true, file = true, directory = false)"]
              [:out "test '/does_not_exist.txt': exists = false, file = false, directory = false)"]
              [:out "test 'not_a_resource_path.go': error"]]
             @output)))))

(deftest open-resource-test
  (test-util/with-loaded-project "test/resources/editor_extensions/open_resource_project"
    (let [output (atom [])]
      (reload-editor-scripts! project
                              :display-output! #(swap! output conj [%1 %2])
                              :open-resource! #(swap! output conj [:open-resource (resource/proj-path %)]))
      (run-edit-menu-test-command!)
      ;; see test.editor script: it uses editor.open_resource with different resource
      ;; paths and prints results
      (is (= [[:open-resource "/game.project"]
              [:out "Open '/game.project': ok"]
              [:out "Open '/does_not_exist.txt': ok"]
              [:out "Open 'not_a_resource_path.go': error"]]
             @output)))))

(deftest coercer-test
  (let [vm (vm/make)
        coerce #(coerce/coerce vm %1 (apply rt/->varargs %&))]

    (testing "enum"
      (let [foo-str "foo"
            enum (coerce/enum 1 4 false foo-str :bar)]
        (is (= 1 (coerce enum 1)))
        (is (= false (coerce enum false)))
        (is (identical? foo-str (coerce enum "foo")))
        (is (= :bar (coerce enum "bar")))
        (is (thrown? LuaError (coerce enum "something else")))))

    (testing "const"
      (let [foo-str "foo"
            const (coerce/const foo-str)]
        (is (identical? foo-str (coerce const "foo")))
        (is (thrown? LuaError (coerce const 12)))))

    (testing "string"
      (is (= "foo" (coerce coerce/string "foo")))
      (is (thrown? LuaError (coerce coerce/string 1))))

    (testing "integer"
      (is (= 15 (coerce coerce/integer 15)))
      (is (thrown? LuaError (coerce coerce/integer 15.5))))

    (testing "number"
      (is (= 15 (coerce coerce/number 15)))
      (is (= 15.5 (coerce coerce/number 15.5)))
      (is (thrown? LuaError (coerce coerce/number "12"))))

    (testing "tuple"
      (let [tuple (coerce/tuple coerce/string (coerce/one-of coerce/null coerce/integer))]
        (is (= ["asd" 4] (coerce tuple ["asd" 4])))
        (is (= ["asd" 4] (coerce tuple ["asd" 4 "skipped-extra"])))
        (is (= ["asd" nil] (coerce tuple ["asd"])))
        (is (= ["asd" 4] (coerce tuple {1 "asd" 10 "foo" 2 4})))
        (is (thrown? LuaError (coerce tuple [])))
        (is (thrown? LuaError (coerce tuple "not-a-table")))
        (is (thrown? LuaError (coerce tuple 42)))))

    (testing "untouched"
      (let [v (vm/->lua {:a 1})]
        (is (identical? v (coerce coerce/untouched v)))))

    (testing "null"
      (is (nil? (coerce coerce/null nil)))
      (is (thrown? LuaError (coerce coerce/null false)))
      (is (thrown? LuaError (coerce coerce/null 12))))

    (testing "boolean"
      (is (true? (coerce coerce/boolean true)))
      (is (false? (coerce coerce/boolean false)))
      (is (thrown? LuaError (coerce coerce/boolean nil)))
      (is (thrown? LuaError (coerce coerce/boolean "asd"))))

    (testing "to-boolean"
      (is (true? (coerce coerce/to-boolean true)))
      (is (false? (coerce coerce/to-boolean false)))
      (is (false? (coerce coerce/to-boolean nil)))
      (is (true? (coerce coerce/to-boolean "asd"))))

    (testing "any"
      (is (= {:a 1} (coerce coerce/any {:a 1})))
      (is (= "foo" (coerce coerce/any "foo")))
      (is (= [1 2 3] (coerce coerce/any [1 2 3])))
      (is (= false (coerce coerce/any false))))

    (testing "userdata"
      (let [obj (Object.)]
        (is (identical? obj (coerce coerce/userdata obj))))
      (is (= :key (coerce coerce/userdata (vm/wrap-userdata :key))))
      (is (thrown? LuaError (coerce coerce/userdata "key")))
      (is (= inc (coerce coerce/userdata inc))))

    (testing "function"
      (let [f (vm/invoke-1 vm (vm/bind (vm/read "return function() end" "test.lua") vm))]
        (is (identical? f (coerce coerce/function f)))
        ;; Clojure's functions are wrapped in userdata
        (is (thrown? LuaError (coerce coerce/function inc)))))

    (testing "wrap-with-pred"
      (let [with-pred (coerce/wrap-with-pred coerce/integer even? "should be even")]
        (is (= 2 (coerce with-pred 2)))
        (is (thrown? LuaError (coerce with-pred 1)))
        (is (thrown? LuaError (coerce with-pred "not an int")))))

    (testing "vector-of"
      (is (= [] (coerce (coerce/vector-of coerce/string) {:a 1}))) ;; non-integer keys are ignored
      (is (= [] (coerce (coerce/vector-of coerce/string) {})))
      (is (= ["a"] (coerce (coerce/vector-of coerce/string) {1 "a" :key "val"})))
      (is (= ["a"] (coerce (coerce/vector-of coerce/string) {1 "a" -1 "b" 0 "c"}))) ;; non-positive keys are ignored
      (is (= [1 2 3] (coerce (coerce/vector-of coerce/integer) [1 2 3])))
      ;; nil usually ends the array part...
      (is (= [1 2 3] (coerce (coerce/vector-of coerce/any) [1 2 3 nil 5])))
      ;; ...but not always:
      (is (= [1 2 3 4 5 6 7 8 9 10 nil 5] (coerce (coerce/vector-of (coerce/one-of coerce/integer coerce/null))
                                                  [1 2 3 4 5 6 7 8 9 10 nil 5])))
      (is (= [] (coerce (coerce/vector-of (coerce/one-of coerce/integer coerce/null)) {5 5})))
      (is (thrown? LuaError (coerce (coerce/vector-of coerce/integer) [1 2 3 4 5 6 7 8 9 10 nil 5])))
      (testing "min-count"
        (is (= [1 2] (coerce (coerce/vector-of coerce/any :min-count 2) [1 2])))
        (is (= [1 2 3] (coerce (coerce/vector-of coerce/any :min-count 2) [1 2 3])))
        (is (thrown? LuaError (coerce (coerce/vector-of coerce/any :min-count 2) [1]))))
      (testing "distinct"
        (is (= [1 2 3] (coerce (coerce/vector-of coerce/any :distinct true) [1 2 3])))
        (is (thrown? LuaError (coerce (coerce/vector-of coerce/any :distinct true) [1 2 1])))))

    (testing "hash-map"
      (is (= {:a 1} (coerce (coerce/hash-map :req {:a coerce/integer}) {:a 1})))
      (is (= {"a" 1} (coerce (coerce/hash-map :req {"a" coerce/integer}) {:a 1})))
      (is (thrown? LuaError (coerce (coerce/hash-map :req {:a coerce/integer}) {})))
      (is (thrown? LuaError (coerce (coerce/hash-map :req {:a coerce/integer}) {:a "not-an-int"})))
      (is (thrown? LuaError (coerce (coerce/hash-map :req {:a coerce/integer}) "not a table")))
      (is (= {} (coerce (coerce/hash-map :opt {:a coerce/integer}) {})))
      (is (= {} (coerce (coerce/hash-map :opt {:a coerce/integer}) {:x 1 :y 2})))
      (is (thrown? LuaError (coerce (coerce/hash-map :opt {:a coerce/integer}) {:x 1 :y 2 :a "not-an-int"})))
      (is (= {:a 1} (coerce (coerce/hash-map :opt {:a coerce/integer}) {:x 1 :y 2 :a 1})))
      (is (= {:a 1} (coerce (coerce/hash-map
                              :req {:a coerce/integer}
                              :opt {:b coerce/integer})
                            {:a 1})))
      (is (= {:a 1 :b 2} (coerce (coerce/hash-map
                                   :req {:a coerce/integer}
                                   :opt {:b coerce/integer})
                                 {:a 1 :b 2})))
      (is (thrown? LuaError (coerce (coerce/hash-map :extra-keys false) {:a 1})))
      (is (thrown? LuaError (coerce (coerce/hash-map :opt {:b coerce/integer} :extra-keys false) {:a 1})))
      (is (= {:a 1} (coerce (coerce/hash-map :opt {:a coerce/integer} :extra-keys false) {:a 1})))
      (is (= {:a 1} (coerce (coerce/hash-map :req {:a coerce/integer} :extra-keys false) {:a 1}))))

    (testing "one-of"
      (is (= "foo" (coerce (coerce/one-of coerce/string coerce/integer) "foo")))
      (is (= 12 (coerce (coerce/one-of coerce/string coerce/integer) 12)))
      (is (thrown? LuaError (coerce (coerce/one-of coerce/string coerce/integer) false))))

    (testing "by-key"
      (let [by-key (coerce/by-key :type
                                  {:rect (coerce/hash-map :req {:width coerce/number
                                                                :height coerce/number})
                                   :circle (coerce/hash-map :req {:radius coerce/number})})]
        (is (= {:type :rect :width 15 :height 20}
               (coerce by-key {"type" "rect" "width" 15.0 "height" 20.0})))
        (is (= {:type :circle :radius 42.5}
               (coerce by-key {"type" "circle" "radius" 42.5 "extra_key" "ignored"})))
        (is (thrown? LuaError (coerce by-key {"type" "rect" "radius" 25})))
        (is (thrown? LuaError (coerce by-key {"type" "dot"})))
        (is (thrown? LuaError (coerce by-key {"kind" "rect"})))
        (is (thrown? LuaError (coerce by-key "not a table")))
        (is (thrown? LuaError (coerce by-key 42)))))

    (testing "regex"
      (let [empty (coerce/regex)]
        (is (= {} (coerce empty)))
        (is (thrown-with-msg? LuaError #"nil is unexpected" (coerce empty nil)))
        (is (thrown-with-msg? LuaError #"1 is unexpected" (coerce empty 1))))
      (let [all-required (coerce/regex :first-name coerce/string
                                       :last-name coerce/string
                                       :age coerce/integer)]
        (is (= {:first-name "Foo"
                :last-name "Bar"
                :age 18}
               (coerce all-required "Foo" "Bar" 18)))
        (is (thrown-with-msg? LuaError #"more arguments expected" (coerce all-required "Foo" "Bar")))
        (is (thrown-with-msg? LuaError #"0 is unexpected" (coerce all-required "Foo" "Bar" 12 0))))

      (let [some-optional (coerce/regex :path coerce/string
                                        :method :? (coerce/enum :get :post :put :delete :head :options)
                                        :as :? (coerce/enum :string :json))]
        (is (= {:path "/foo" :method :get :as :json}
               (coerce some-optional "/foo" :get :json)))
        (is (= {:path "/foo" :as :json} (coerce some-optional "/foo" :json)))
        (is (= {:path "/foo" :method :options} (coerce some-optional "/foo" :options)))
        (is (= {:path "/foo"} (coerce some-optional "/foo")))
        (is (thrown-with-msg? LuaError #"true is not a string" (coerce some-optional true))))
      (let [required-after-optional (coerce/regex :string coerce/string
                                                  :boolean :? coerce/boolean
                                                  :integer coerce/integer)]
        (is (= {:string "foo" :boolean true :integer 1} (coerce required-after-optional "foo" true 1)))
        (is (= {:string "foo" :integer 1} (coerce required-after-optional "foo" 1)))
        (is (thrown-with-msg? LuaError #"more arguments expected" (coerce required-after-optional "foo")))
        (is (thrown-with-msg? LuaError #"(\"bar\" is not a boolean)\n.+(\"bar\" is not an integer)" (coerce required-after-optional "foo" "bar")))))))

(defn- normalize-pprint-output [output-string]
  (let [hash->stable-id (volatile! {})]
    (string/replace
      output-string
      #"0x[0-9a-f]+"
      (fn [s]
        (or (@hash->stable-id s)
            ((vswap! hash->stable-id #(assoc % s (str "0x" (count %)))) s))))))

(defn- expect-script-output [expected actual]
  (let [actual (normalize-pprint-output (str actual))]
    (let [output-matches-expectation (= expected actual)]
      (is output-matches-expectation (when-not output-matches-expectation (string/join "\n" (diff/make-diff-output-lines expected actual 3)))))))

(deftest external-file-attributes-test
  (test-util/with-loaded-project "test/resources/editor_extensions/external_file_attributes_project"
    (let [output (atom [])]
      (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
      (run-edit-menu-test-command!)
      ;; see test.editor_script: it uses editor.external_file_attributes() to
      ;; get fs information about 3 paths, and then prints it
      (is (= [[:out "path = '.', exists = true, file = false, directory = true"]
              [:out "path = 'game.project', exists = true, file = true, directory = false"]
              [:out "path = 'does_not_exist.txt', exists = false, file = false, directory = false"]]
             @output)))))

(def expected-ui-test-output
  "editor.ui.image({}) => {} must have the \"image\" key
editor.ui.image({image = false}) => false is not a string
editor.ui.image({image = 'foo', width = false}) => false is not a number
editor.ui.image({image = 'foo', width = -1}) => -1 is not positive
")

(deftest ui-test
  (test-util/with-loaded-project "test/resources/editor_extensions/ui_project"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      ;; see test.editor_script: it creates a lot of ui components that should
      ;; form a valid UI tree. In case of any errors the output will get error
      ;; entries.
      (expect-script-output expected-ui-test-output out))))

(deftest prefs-round-trip-test
  (test-util/with-loaded-project "test/resources/editor_extensions/prefs_round_trip_project"
    (let [output (atom [])
          prefs (prefs/project
                  "test/resources/editor_extensions/prefs_round_trip_project"
                  (prefs/global test-util/shared-test-prefs-file))
          _ (reload-editor-scripts! project
                                    :prefs prefs
                                    :display-output! #(swap! output conj [%1 %2]))
          all-keys (mapv (comp name key) (:properties (prefs/schema prefs [])))
          _ (prefs/set! prefs [:test :prefs-keys] all-keys)
          initial-prefs (prefs/get prefs [])
          ;; See test.editor_script: it iterates over every 'test.prefs-keys'
          ;; preference, gets the value, and then sets the same value back.
          ;; This get->set->get round-tripping should not cause any errors, and
          ;; should not change any values
          _ (run-edit-menu-test-command!)]
      ;; no output => no runtime errors
      (is (= [] @output))
      ;; the prefs are unchanged
      (is (= initial-prefs (prefs/get prefs []))))))

(deftest prefs-set-test
  (test-util/with-loaded-project "test/resources/editor_extensions/prefs_get_set_test"
    (let [output (atom [])
          prefs (prefs/project
                  "test/resources/editor_extensions/prefs_get_set_test"
                  (prefs/global test-util/shared-test-prefs-file))]
      (reload-editor-scripts! project
                              :prefs prefs
                              :display-output! #(swap! output conj [%1 %2]))
      (run-edit-menu-test-command!)
      ;; See test.editor_script: it defines prefs for every available schema,
      ;; then, for every pref it sets a value, gets it and prints it (possibly
      ;; with some metadata), then sets it to another value, and then gets and
      ;; prints it again
      (is (= [[:out "array: table 0"]
              [:out "array: table 2 foo bar"]
              [:out "boolean: boolean false"]
              [:out "boolean: boolean true"]
              [:out "enum: number 1"]
              [:out "enum: string foo"]
              [:out "integer: 42"]
              [:out "integer: 43"]
              [:out "keyword: string foo-bar"]
              [:out "keyword: string code-view"]
              [:out "number: 12.3"]
              [:out "number: 0.1"]
              [:out "one_of: a string"]
              [:out "one_of: 17"]
              [:out "object: table foo"]
              [:out "object: table bar"]
              [:out "object: table baz"]
              [:out "object of: table foo = true, bar = false"]
              [:out "object of: table foo = false, bar = true"]
              [:out "set: table foo = true, bar = true"]
              [:out "set: table foo = true, bar = nil"]
              [:out "string: a string"]
              [:out "string: another_string"]
              [:out "password: password"]
              [:out "password: another_password"]
              [:out "tuple: a string 12"]
              [:out "tuple: another string 42"]]
             @output)))))

(deftest prefs-schema-test
  (test-util/with-loaded-project "test/resources/editor_extensions/prefs_schema_errors"
    (let [output (atom [])
          prefs (prefs/project
                  "test/resources/editor_extensions/prefs_schema_errors"
                  (prefs/global test-util/shared-test-prefs-file))]
      (reload-editor-scripts! project
                              :prefs prefs
                              :display-output! #(swap! output conj [%1 %2]))
      (run-edit-menu-test-command!)
      ;; See the test project:
      ;; - /not_an_object_schema.editor_script defines a schema that is not
      ;;   an object
      ;; - /conflict_a.editor_script and /conflict_b.editor_script define
      ;;   conflicting schemas
      ;; - /builtin_conflict.editor_script defines a schema that conflicts with
      ;;   a built-in editor schema (sets 'bundle' to boolean)
      ;; - /test.editor_script accesses prefs defined by other editor scripts
      (is (= #{[:err "Omitting prefs schema definition for path '': '/not_an_object_schema.editor_script' defines a schema that conflicts with the editor schema"]
               [:err "Omitting prefs schema definition for path 'bundle': '/builtin_conflict.editor_script' defines a schema that conflicts with the editor schema"]
               [:err "Omitting prefs schema definition for path 'test.conflict': conflicts with another editor script schema"]
               [:out "pcall(editor.prefs.get, 'test.only-in-a') => true, a"]
               [:out "pcall(editor.prefs.get, 'test.only-in-b') => true, b"]
               [:out "pcall(editor.prefs.get, 'test.conflict') => false, No schema defined for prefs path 'test.conflict'"]
               [:out "pcall(editor.prefs.get, 'bundle.variant') => true, debug"]}
             (set @output))))))

(deftest prefs-test
  (testing "dot-separated paths parsing"
    (are [s path] (= path (prefs-functions/parse-dot-separated-path s))
      "foo.bar.baz" [:foo :bar :baz]
      "foo-bar.baz" [:foo-bar :baz]
      "foo-bar_baz" [:foo-bar_baz]
      "3d" [:3d])
    (are [s re] (thrown-with-msg? LuaError (re-pattern re) (prefs-functions/parse-dot-separated-path s))
      "" "Key path element cannot be empty"
      "." "Key path element cannot be empty"
      "foo." "Key path element cannot be empty"
      ".foo" "Key path element cannot be empty"
      "foo..bar" "Key path element cannot be empty"
      "foo.bar..." "Key path element cannot be empty"

      "with space" "Invalid identifier character"
      "brace{" "Invalid identifier character"
      "brace[" "Invalid identifier character"
      ":colon" "Invalid identifier character"
      "hash#" "Invalid identifier character"
      "hash#" "Invalid identifier character"
      "\\backslash" "Invalid identifier character"
      "/slash" "Invalid identifier character"
      "^hat" "Invalid identifier character"
      "%percent" "Invalid identifier character")))

(deftest json-test
  (test-util/with-loaded-project "test/resources/editor_extensions/json_project"
    (let [output (atom [])]
      (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
      (run-edit-menu-test-command!)
      ;; See test.editor_script: it prints various json encode/decode results
      (is (= [[:out "Testing encoding..."]
              [:out "json.encode(1) => 1"]
              [:out "json.encode(\"foo\") => \"foo\""]
              [:out "json.encode(nil) => null"]
              [:out "json.encode(true) => true"]
              [:out "json.encode({num = 1}) => {\"num\":1}"]
              [:out "json.encode({bools = {true, false, true}}) => {\"bools\":[true,false,true]}"]
              [:out "json.encode({empty_table_as_object = {}}) => {\"empty_table_as_object\":{}}"]
              [:out "json.encode({[{\"object\"}] = {}}) => error"]
              [:out "json.encode({fn = function() end}) => error"]
              [:out "Testing decoding..."]
              [:out "json.decode('1') => 1"]
              [:out "json.decode('{\"a\":1}') => {a = 1}"]
              [:out "json.decode('{\"null_is_omitted\": null}') => {}"]
              [:out "json.decode('[false, true, null, 4, \"string\"]') => {false, true, nil, 4, \"string\"}"]
              [:out "json.decode('{\"a\": [{\"b\": 4},42]}') => {a = {{b = 4}, 42}}"]
              [:out "json.decode('true false \"string\" [] {}', {all = true}) => {true, false, \"string\", {}, {}}"]
              [:out "json.decode('fals') => error"]
              [:out "json.decode('true {', {all = true}) => error"]]
             @output)))))

(def expected-pprint-output
  "scalars:
1
true
<function: print>
\"string\"
empty:
{} --[[0x0]]
array only:
{ --[[0x1]]
  1,
  2,
  3,
  \"a\"
}
hash only:
{ --[[0x2]]
  a = 1,
  b = 2
}
array and hash:
{ --[[0x3]]
  1,
  2,
  a = 3,
  b = 4
}
non-identifier keys:
{ --[[0x4]]
  [\"foo-bar\"] = 1
}
circular refs:
{ --[[0x5]]
  1,
  2,
  circular_ref = <table: 0x5>
}
nesting:
{ --[[0x6]]
  1,
  false,
  { --[[0x7]]
    a = 1
  },
  { --[[0x8]]
    [{ --[[0x9]]
      \"table\",
      \"key\"
    }] = 1
  },
  \"a\"
}
")

(deftest pprint-test
  (test-util/with-loaded-project "test/resources/editor_extensions/pprint-test"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      (expect-script-output expected-pprint-output out))))

(def expected-http-test-output
  "GET http://localhost:23000 => error (ConnectException)
GET not-an-url => error (URI with undefined scheme)
HEAD http://localhost:23456 => 200
GET http://localhost:23456 => 200
GET http://localhost:23456/redirect/foo as string => 200
\"successfully redirected\"
GET http://localhost:23456/json as json => 200
{ --[[0x0]]
  a = 1,
  b = { --[[0x1]]
    true
  }
}
POST http://localhost:23456/echo {\"y\":\"foo\",\"x\":4} as json => 200
{ --[[0x2]]
  y = \"foo\",
  x = 4
}
POST http://localhost:23456/echo hello world! as string => 200
\"hello world!\"
")

(deftest http-test
  (test-util/with-loaded-project "test/resources/editor_extensions/http_project"
    (let [server (http-server/start!
                   (http-server/router-handler
                     {"/redirect/foo" {"GET" (constantly (http-server/redirect "/foo"))}
                      "/foo" {"GET" (constantly (http-server/response 200 "successfully redirected"))}
                      "/" {"GET" (constantly (http-server/response 200 ""))}
                      "/json" {"GET" (constantly (http-server/json-response {:a 1 :b [true]}))}
                      "/echo" {"POST" (fn [request] (http-server/response 200 (:body request)))}})
                   :port 23456)
          out (StringBuilder.)]
      (try
        (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
        ;; See test.editor_script: the test invokes http.request with various options and prints results
        (run-edit-menu-test-command!)
        (expect-script-output expected-http-test-output out)
        (finally
          (http-server/stop! server 0))))))

(def ^:private resource-io-test-output
  "editor.create_resources({{\"/test/config.json\", \"{\\\"test\\\": true}\"}}) => ok!
/test
  /config.json
/test/config.json:
{ --[[0x0]]
  text = \"{\\\"test\\\": true}\"
}
editor.create_resources({\"/test/npc.go\", \"/test/npc.collection\"}) => ok!
/test
  /config.json
  /npc.collection
  /npc.go
/test/npc.go:
{ --[[0x1]]
  components = {} --[[0x2]]
}
/test/npc.collection:
{ --[[0x3]]
  name = \"npc\"
}
editor.create_resources({\"/test/UPPER.COLLECTION\"}) => ok!
/test
  /config.json
  /npc.collection
  /npc.go
  /UPPER.COLLECTION
/test/UPPER.COLLECTION:
{ --[[0x4]]
  name = \"UPPER\"
}
editor.create_resources({\"/test/../../../outside.txt\"}) => Can't create /test/../../../outside.txt: outside of project directory
/test
  /config.json
  /npc.collection
  /npc.go
  /UPPER.COLLECTION
editor.create_resources({\"/test/npc.go\"}) => Resource already exists: /test/npc.go
/test
  /config.json
  /npc.collection
  /npc.go
  /UPPER.COLLECTION
editor.create_resources({\"/test/repeated.go\", \"/test/repeated.go\"}) => Resource repeated more than once: /test/repeated.go
/test
  /config.json
  /npc.collection
  /npc.go
  /UPPER.COLLECTION
")

(deftest resources-io-test
  (test-util/with-scratch-project "test/resources/editor_extensions/resources_io_project"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      (expect-script-output resource-io-test-output out))))

(defn- expected-zip-test-output [root]
  (str "Testing zip.pack...
A file:
zip.pack('gitignore.zip', {'.gitignore'}) => ok
A directory:
zip.pack('foo.zip', {'foo'}) => ok
Multiple:
zip.pack('multiple.zip', {'foo', {'foo', 'bar'}, '.gitignore', {'game.project', 'settings.ini'}}) => ok
Outside:
zip.pack('outside.zip', {{'" (System/getenv "PWD") "/project.clj', 'project.clj'}}) => ok
Stored method:
zip.pack('stored.zip', {method = 'stored'}, {'foo'}) => ok
Compression level 0:
zip.pack('level_0.zip', {level = 0}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Compression level 1:
zip.pack('level_1.zip', {level = 1}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Compression level 2:
zip.pack('level_2.zip', {level = 2}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Compression level 3:
zip.pack('level_3.zip', {level = 3}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Compression level 4:
zip.pack('level_4.zip', {level = 4}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Compression level 5:
zip.pack('level_5.zip', {level = 5}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Compression level 6:
zip.pack('level_6.zip', {level = 6}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Compression level 7:
zip.pack('level_7.zip', {level = 7}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Compression level 8:
zip.pack('level_8.zip', {level = 8}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Compression level 9:
zip.pack('level_9.zip', {level = 9}, {'foo', {'foo', 'bar'}, '.gitignore', 'game.project'}) => ok
Unix:
zip.pack('script.zip', {'script.sh'}) => ok
Mixed compression settings:
zip.pack('mixed.zip', {{'foo', '9', level = 9}, {'foo', '1', level = 1}, {'foo', '0', level = 0}, {'foo', 'stored', method = 'stored'}}) => ok
Archive path is a directory:
zip.pack('foo', {'game.project'}) => error: Output path is a directory: foo
Source path does not exist:
zip.pack('error.zip', {'does-not-exist.txt'}) => error: Source path does not exist: " root "/does-not-exist.txt
Target path is absolute:
zip.pack('error.zip', {'" (System/getenv "HOME") "'}) => error: Target path must be relative: " (System/getenv "HOME") "
Target path is a relative path above root:
zip.pack('error.zip', {{'game.project', '../../game.project'}}) => error: Target path is above archive root: ../../game.project
zip.pack('error.zip', {{'game.project', 'foo/bar/../../../../game.project'}}) => error: Target path is above archive root: ../../game.project
Testing zip.unpack...
zip.unpack('script.zip', 'build', {'script.sh'}) => ok
Default conflict resolution is error:
zip.unpack('script.zip', 'build', {'script.sh'}) => error: Path already exists: " root "/build/script.sh
Overwrite option:
zip.unpack('script.zip', 'build', {on_conflict = 'overwrite'}, {'script.sh'}) => ok
Skip:
zip.unpack('foo.zip', 'build/skip') => ok
build/skip/foo exists: true
build/skip/bar exists: false
zip.unpack('multiple.zip', 'build/skip', {on_conflict = 'skip'}) => ok
build/skip/foo exists: true
build/skip/bar exists: true
Overwrite can't replace directory with a file:
zip.unpack('script.zip', 'build/dir_conflict', {on_conflict = 'overwrite'}) => error: Can't overwrite directory: " root "/build/dir_conflict/script.sh
Partial unpack:
zip.unpack('multiple.zip', 'build/subset', {'settings.ini'}) => ok
build/subset/settings.ini exists: true
build/subset/bar exists: false
build/subset/foo exists: false
Archive validation:
zip.unpack('does-not-exist.zip') => error: Archive path does not exist: " root "/does-not-exist.zip
zip.unpack('foo') => error: Archive path is a directory: " root "/foo
Paths validation:
zip.unpack('script.zip', 'build', {on_conflict = 'skip'}, {'foo/../../../bar'}) => error: Invalid argument:
- \"foo/../../../bar\" is above root
- {\"foo/../../../bar\"} is unexpected
zip.unpack('script.zip', 'build', {on_conflict = 'skip'}, {'/tmp'}) => error: Invalid argument:
- \"/tmp\" is absolute
- {\"/tmp\"} is unexpected
zip.unpack('script.zip', 'build', {on_conflict = 'skip'}, {'tmp/..'}) => error: Invalid argument:
- \"tmp/..\" is empty
- {\"tmp/..\"} is unexpected
Extract to parent:
zip.pack('build/nested/archive.zip', {'game.project'}) => ok
zip.unpack('build/nested/archive.zip') => ok
build/nested/game.project exists: true
"))

(deftest zip-test
  (test-util/with-scratch-project "test/resources/editor_extensions/zip_project"
    (let [root (workspace/project-directory workspace)
          list-entries (fn list-entries [path-str]
                         (with-open [zis (ZipArchiveInputStream. (io/input-stream (fs/path root path-str)))]
                           (loop [acc (transient #{})]
                             (if-let [e (.getNextZipEntry zis)]
                               (recur (conj! acc (.getName e)))
                               (persistent! acc)))))
          size (fn size [path-str]
                 (fs/path-size (fs/path root path-str)))
          list-methods (fn list-methods [path-str]
                         (with-open [zis (ZipArchiveInputStream. (io/input-stream (fs/path root path-str)))]
                           (loop [acc (transient {})]
                             (if-let [e (.getNextZipEntry zis)]
                               (recur (assoc! acc (.getName e) (condp = (.getMethod e)
                                                                 ZipEntry/STORED :stored
                                                                 ZipEntry/DEFLATED :deflated)))
                               (persistent! acc)))))
          out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      ;; See test.editor_script: the script creates a bunch of archives and logs pack
      ;; arguments with the result of execution
      (expect-script-output (expected-zip-test-output root) out)
      (is (= #{".gitignore"} (list-entries "gitignore.zip")))
      (is (= #{"foo/bar.txt" "foo/long.txt" "foo/subdir/baz.txt"} (list-entries "foo.zip")))
      (is (= #{"foo/bar.txt" "foo/long.txt" "foo/subdir/baz.txt"
               "bar/bar.txt" "bar/long.txt" "bar/subdir/baz.txt"
               "settings.ini"
               ".gitignore"}
             (list-entries "multiple.zip")))
      (is (= #{"project.clj"} (list-entries "outside.zip")))
      ;; foo.zip and stored.zip are same archives with different compression methods
      (is (and (= (list-entries "foo.zip") (list-entries "stored.zip"))
               (< (size "foo.zip") (size "stored.zip"))))
      (is (<= (size "level_9.zip")
              (size "level_8.zip")
              (size "level_7.zip")
              (size "level_6.zip")
              (size "level_5.zip")
              (size "level_4.zip")
              (size "level_3.zip")
              (size "level_2.zip")
              (size "level_1.zip")
              (size "level_0.zip")))
      (is (= {"0/bar.txt" :deflated
              "0/long.txt" :deflated
              "0/subdir/baz.txt" :deflated
              "1/bar.txt" :deflated
              "1/long.txt" :deflated
              "1/subdir/baz.txt" :deflated
              "9/bar.txt" :deflated
              "9/long.txt" :deflated
              "9/subdir/baz.txt" :deflated
              "stored/bar.txt" :stored
              "stored/long.txt" :stored
              "stored/subdir/baz.txt" :stored}
             (list-methods "mixed.zip")))
      (when-not (os/is-win32?)
        (is (contains?
              (Files/getPosixFilePermissions (fs/path root "build" "script.sh") fs/empty-link-option-array)
              PosixFilePermission/OWNER_EXECUTE))))))

(def expected-zlib-test-output
  "inflate zlib: hello
inflate gzip: hello
deflate hello: \\120\\94\\203\\72\\205\\201\\201\\7\\0\\6\\44\\2\\21
same as expected zlib buf: true
roundtrip: true
zlib.inflate(false) => bad argument: string expected, got boolean
zlib.deflate(false) => bad argument: string expected, got boolean
zlib.inflate('') => Failed to inflate buffer (Unexpected end of ZLIB input stream)
zlib.inflate('not-a-buf') => Failed to inflate buffer (incorrect header check)
")

(deftest zlib-test
  (test-util/with-scratch-project "test/resources/editor_extensions/zlib_project"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      (expect-script-output expected-zlib-test-output out))))

(def expected-http-server-test-output
  "Omitting conflicting routes for 'GET /test/conflict/same-path-and-method' defined in /test.editor_script
Omitting conflicting routes for '/command/{param}' defined in /test.editor_script (conflict with the editor's built-in routes)
Omitting conflicting routes for '/test/conflict/{param}' and '/test/conflict/no-param' defined in /test.editor_script
POST /test/echo/string 'hello' as string => 200
< content-length: 5
< content-type: text/plain; charset=utf-8
\"hello\"
POST /test/echo/json '{\"a\": 1}' as json => 200
< content-length: 7
< content-type: application/json
{ --[[0x0]]
  a = 1
}
GET /test/route-merging as string => 200
< content-length: 3
< content-type: text/plain; charset=utf-8
\"get\"
POST /test/route-merging as string => 200
< content-length: 4
< content-type: text/plain; charset=utf-8
\"post\"
PUT /test/route-merging as string => 200
< content-length: 3
< content-type: text/plain; charset=utf-8
\"put\"
GET /test/path-pattern/foo/bar as string => 200
< content-length: 22
< content-type: text/plain; charset=utf-8
\"key = foo, value = bar\"
GET /test/rest-pattern/a/b/c as string => 200
< content-length: 5
< content-type: text/plain; charset=utf-8
\"a/b/c\"
GET /test/files/not-found.txt as string => 404
< content-length: 0
\"\"
GET /test/files/test.txt as string => 200
< content-length: 9
< content-type: text/plain
\"test file\"
GET /test/files/test.json as json => 200
< content-length: 14
< content-type: application/json
{ --[[0x1]]
  test = true
}
GET /test/resources/not-found.txt as string => 404
< content-length: 0
\"\"
GET /test/resources/test.txt as string => 200
< content-length: 9
< content-type: text/plain
\"test file\"
GET /test/resources/test.json as json => 200
< content-length: 14
< content-type: application/json
{ --[[0x2]]
  test = true
}
")

(deftest http-server-test
  (test-util/with-loaded-project "test/resources/editor_extensions/http_server_project"
    (with-open [server (http-server/start!
                         (web-server/make-dynamic-handler
                           ;; for testing conflicts with the built-in handlers
                           {"/command" {"GET" (constantly http-server/not-found)}
                            "/command/{command}" {"POST" (constantly http-server/not-found)}}))]
      (let [out (StringBuilder.)]
        (reload-editor-scripts! project
                                :display-output! #(doto out (.append %2) (.append \newline))
                                :web-server server)
        (run-edit-menu-test-command!)
        (expect-script-output expected-http-server-test-output out)))))

(deftest property-availability-test
  (test-util/with-loaded-project "test/resources/editor_extensions/property_availability_project"
    (reload-editor-scripts! project)
    (g/with-auto-evaluation-context ec
      (let [{:keys [rt]} (extensions/ext-state project ec)]
        (->> (g/node-value project :nodes ec)
             (map #(g/node-value % :node-outline ec))
             (mapcat #(tree-seq :children :children %))
             (mapcat (fn [outline]
                       (->> [(g/node-value (:node-id outline) :_properties ec)]
                            properties/coalesce
                            :properties
                            vals
                            (map #(assoc % :outline outline)))))
             (run! (fn [{:keys [outline key] :as p}]
                     (let [{:keys [node-id]} outline
                           ext-key (string/replace (name key) \- \_)]
                       (is (some? (graph/ext-value-getter node-id ext-key project ec)))
                       (when-not (properties/read-only? p)
                         (is (some? (graph/ext-lua-value-setter node-id ext-key rt project ec))))))))))))

(def ^:private expected-tilemap-test-output
  "new => {
}
fill 3x3 => {
  [1, 1] = 2
  [2, 1] = 2
  [3, 1] = 2
  [1, 2] = 2
  [2, 2] = 2
  [3, 2] = 2
  [1, 3] = 2
  [2, 3] = 2
  [3, 3] = 2
}
remove center => {
  [1, 1] = 2
  [2, 1] = 2
  [3, 1] = 2
  [1, 2] = 2
  [3, 2] = 2
  [1, 3] = 2
  [2, 3] = 2
  [3, 3] = 2
}
clear => {
}
set using table => {
  [8, 8] = 1
}
get tile: 1
get info:
  h_flip: true
  index: 1
  v_flip: true
  rotate_90: true
non-existent tile: nil
non-existent info: nil
negative keys => {
  [-100, -100] = 1
  [8, 8] = 1
}
create a layer with tiles...
tiles from the graph => {
  [-100, -100] = 1
  [8, 8] = 1
}
set layer tiles to new value...
new tiles from the graph => {
  [8, 8] = 2
}
")

(deftest tile-map-editing-test
  (test-util/with-loaded-project "test/resources/editor_extensions/tilemap_project"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      (expect-script-output expected-tilemap-test-output out))))

(def ^:private expected-attachment-test-output
  "Atlas initial state:
  images: 0
  animations: 0
  can add images: true
  can add animations: true
Transaction: add image and animation
After transaction (add):
  images: 1
    image: /builtins/assets/images/logo/logo_256.png
  animations: 1
    animation id: logos
    animation images: 2
      animation image: /builtins/assets/images/logo/logo_blue_256.png
      animation image: /builtins/assets/images/logo/logo_256.png
Transaction: remove image
After transaction (remove image):
  images: 0
  animations: 1
    animation id: logos
    animation images: 2
      animation image: /builtins/assets/images/logo/logo_blue_256.png
      animation image: /builtins/assets/images/logo/logo_256.png
Transaction: clear animation images
After transaction (clear):
  images: 0
  animations: 1
    animation id: logos
    animation images: 0
Transaction: remove animation
After transaction (remove animation):
  images: 0
  animations: 0
Expected errors:
  Wrong list name to add => \"layers\" is undefined
  Wrong list name to remove => \"layers\" is undefined
  Wrong list item to remove => /test.atlas is not in the \"images\" list of /test.atlas
  Wrong list name to clear => \"layers\" is undefined
  Wrong child property name => Can't set property \"no_such_prop\" of AtlasAnimation
  Added value is not a table => \"/foo.png\" is not a table
  Added nested value is not a table => \"/foo.png\" is not a table
  Added node has invalid property value => \"invalid-pivot\" is not a tuple
  Added resource has wrong type => resource extension should be jpeg, jpg or png
Tilesource initial state:
  animations: 0
  collision groups: 0
  tile collision groups: 0
Transaction: add animations and collision groups
After transaction (add animations and collision groups):
  animations: 2
    animation: idle
    animation: walk
  collision groups: 4
    collision group: obstacle
    collision group: character
    collision group: collision_group
    collision group: collision_group1
  tile collision groups: 2
    tile collision group: 1 obstacle
    tile collision group: 3 character
Transaction: set tile_collision_groups to its current value
After transaction (tile_collision_groups roundtrip):
  animations: 2
    animation: idle
    animation: walk
  collision groups: 4
    collision group: obstacle
    collision group: character
    collision group: collision_group
    collision group: collision_group1
  tile collision groups: 2
    tile collision group: 1 obstacle
    tile collision group: 3 character
Expected errors
  Using non-existent collision group => Collision group \"does-not-exist\" is not defined in the tilesource
Transaction: clear tilesource
After transaction (clear):
  animations: 0
  collision groups: 0
  tile collision groups: 0
Tilemap initial state:
  layers: 0
Transaction: add 2 layers
After transaction (add 2 layers):
  layers: 2
    layer: background
    tiles: {
      [1, 1] = 1
      [2, 1] = 2
      [3, 1] = 3
      [1, 2] = 2
      [2, 2] = 2
      [3, 2] = 3
      [1, 3] = 3
      [2, 3] = 3
      [3, 3] = 3
    }
    layer: items
    tiles: {
      [2, 2] = 3
    }
Transaction: clear tilemap
After transaction (clear):
  layers: 0
Particlefx initial state:
  emitters: 0
  modifiers: 0
Transaction: add emitter and modifier
After transaction (add emitter and modifier):
  emitters: 1
    emitter: emitter
    modifiers: 2
      modifier: modifier-type-vortex
      modifier: modifier-type-drag
  modifiers: 1
    modifier: modifier-type-acceleration
Transaction: clear particlefx
After transaction (clear):
  emitters: 0
  modifiers: 0
Collision object initial state:
  collision_type: collision-object-type-dynamic
  shapes: 0
Transaction: add 3 shapes
After transaction (add 3 shapes):
  collision_type: collision-object-type-static
  shapes: 3
  - id: box
    type: shape-type-box
    dimensions: 20 20 20
  - id: sphere
    type: shape-type-sphere
    diameter: 20
  - id: capsule
    type: shape-type-capsule
    diameter: 20
    height: 40
Transaction: clear
After transaction (clear):
  collision_type: collision-object-type-dynamic
  shapes: 0
Expected errors:
  missing type => type is required
  wrong type => box is not shape-type-box, shape-type-capsule or shape-type-sphere
GUI initial state:
  layers: 0
  materials: 0
  particlefxs: 0
  textures: 0
  layouts: 0
  spine scenes: 0
  fonts: 0
  nodes: 0
Transaction: edit GUI
After transaction (edit):
  layers: 2
    layer: bg
    layer: fg
  materials: 4
    material: material
    material: test /test.material
    material: test1 /test.material
    material: material1
  particlefxs: 3
    particlefx: particlefx
    particlefx: test /test.particlefx
    particlefx: particlefx1
  textures: 2
    texture: test /test.tilesource
    texture: test1 /test.atlas
  layouts: 2
    layout: Landscape
    layout: Portrait
  spine scenes: 4
    spine scene: spine_scene
    spine scene: explicit name
    spine scene: template /defold-spine/assets/template/template.spinescene
    spine scene: spine_scene1
  fonts: 3
    font: test /test.font
    font: font
    font: font1
  nodes: 2
  - type: gui-node-type-box
    id: box
    nodes: 5
    - type: gui-node-type-pie
      id: pie
      nodes: 0
    - type: gui-node-type-text
      id: text
      nodes: 0
    - type: gui-node-type-template
      id: button
      nodes: 2
      - type: gui-node-type-box
        id: button/box
        nodes: 0
      - type: gui-node-type-text
        id: button/text
        nodes: 0
    - type: gui-node-type-particlefx
      id: particlefx
      nodes: 0
    - type: gui-node-type-spine
      id: spine
      nodes: 1
      - type: gui-node-type-box
        id: box1
        nodes: 0
  - type: gui-node-type-text
    id: text1
    nodes: 0
Transaction: set Landscape position
  position = {10, 10, 10}, can reset = false
  Landscape:position = {20, 20, 20}, can reset = true
  Portrait:position = {10, 10, 10}, can reset = false
Transaction: reset Landscape position
  position = {10, 10, 10}, can reset = false
  Landscape:position = {10, 10, 10}, can reset = false
  Portrait:position = {10, 10, 10}, can reset = false
Template node: button
  can add: false
  can reorder: false
Override text node: button/text
  can add: false
  can reorder: false
Transaction: set override node property
  text: custom text
  can reset: true
Transaction: reset override node property
  text: <text>
  can reset: false
Transaction: set override position and layout position properties
  position = {10, 10, 10}, can reset = true
  Landscape:position = {20, 20, 20}, can reset = true
  Portrait:position = {10, 10, 10}, can reset = false
can reorder layers: true
can reorder nodes: true
Transaction: reorder
After transaction (reorder):
  layers: 2
    layer: fg
    layer: bg
  materials: 4
    material: material
    material: test /test.material
    material: test1 /test.material
    material: material1
  particlefxs: 3
    particlefx: particlefx
    particlefx: test /test.particlefx
    particlefx: particlefx1
  textures: 2
    texture: test /test.tilesource
    texture: test1 /test.atlas
  layouts: 2
    layout: Landscape
    layout: Portrait
  spine scenes: 4
    spine scene: spine_scene
    spine scene: explicit name
    spine scene: template /defold-spine/assets/template/template.spinescene
    spine scene: spine_scene1
  fonts: 3
    font: test /test.font
    font: font
    font: font1
  nodes: 2
  - type: gui-node-type-text
    id: text1
    nodes: 0
  - type: gui-node-type-box
    id: box
    nodes: 5
    - type: gui-node-type-pie
      id: pie
      nodes: 0
    - type: gui-node-type-text
      id: text
      nodes: 0
    - type: gui-node-type-template
      id: button
      nodes: 2
      - type: gui-node-type-box
        id: button/box
        nodes: 0
      - type: gui-node-type-text
        id: button/text
        nodes: 0
    - type: gui-node-type-particlefx
      id: particlefx
      nodes: 0
    - type: gui-node-type-spine
      id: spine
      nodes: 1
      - type: gui-node-type-box
        id: box1
        nodes: 0
Expected reorder errors:
  undefined property => \"not-a-property\" is undefined
  reorder not defined => \"shapes\" is not reorderable
  duplicates => Reordered child nodes are not the same as current child nodes
  missing children => Reordered child nodes are not the same as current child nodes
  wrong child nodes => Reordered child nodes are not the same as current child nodes
  add to template node => \"nodes\" is read-only
  reorder template node => \"nodes\" is read-only
  add to overridden text node => \"nodes\" is read-only
  reorder overridden text node => \"nodes\" is read-only
  reset unresettable => Can't reset property \"text\" of TextNode
Transaction: clear GUI
Expected layout errors:
  no name => layout name is required
  unknown profile => \"Not a profile\" is not \"Landscape\" or \"Portrait\"
  duplicates => \"Landscape\" is not \"Portrait\"
After transaction (clear):
  layers: 0
  materials: 0
  particlefxs: 0
  textures: 0
  layouts: 0
  spine scenes: 0
  fonts: 0
  nodes: 0
Go initial state:
  components: 0
Transaction: add go components
After transaction (add go components):
  components: 15
  - type: camera
    id: camera
  - type: collectionfactory
    id: collectionfactory
  - type: collectionproxy
    id: collectionproxy
  - type: collectionproxy
    id: collectionproxy1
  - type: collisionobject
    id: collisionobject-embedded
    collision_type: collision-object-type-static
    shapes: 1
    - id: box
      type: shape-type-box
      dimensions: 2.5 2.5 2.5
  - type: factory
    id: factory
  - type: label
    id: label
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {0.05, 0.05, 0.05}
  - type: mesh
    id: mesh
    position: {0, 0, 0}
    rotation: {0, 0, 0}
  - type: model
    id: model
    position: {0, 0, 0}
    rotation: {0, 0, 0}
  - type: sound
    id: boom
  - type: spinemodel
    id: spinemodel
    position: {3.14, 3.14, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
  - type: sprite
    id: blob
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
  - type: component-reference
    id: test
  - type: component-reference
    id: collisionobject-referenced
  - type: component-reference
    id: referenced-tilemap
    position: {0, 0, 0}
    rotation: {0, 0, 0}
Collision object components found:
  referenced: true
  embedded: true
Collision object components have shapes property:
  referenced: false
  embedded: true
Transaction: clear go components
After transaction (clear go components):
  components: 0
Collection initial state
  children: 0 (editable)
Transaction: add gos and collections
After transaction (add gos and collections):
  children: 5 (editable)
  - id: go
    url: /go
    type: go
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {0.5, 0.5, 0.5}
    components: 0
    children: 4 (editable)
    - id: go1
      url: /go1
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 0 (editable)
    - id: ref
      url: /ref
      type: go-reference
      path: /ref.go
      position: {3.14, 3.14, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 0 (editable)
    - id: go2
      url: /go2
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 1 (editable)
      - id: char
        url: /char
        type: go
        position: {0, 0, 0}
        rotation: {0, 0, 0}
        scale: {1, 1, 1}
        components: 3
        - type: sprite
          id: sprite
          position: {0.5, 0.5, 0.5}
          rotation: {0, 0, 0}
          scale: {1, 1, 1}
        - type: collisionobject
          id: collisionobject
          collision_type: collision-object-type-dynamic
          shapes: 1
          - id: box
            type: shape-type-box
            dimensions: 2.5 2.5 2.5
        - type: component-reference
          id: test
        children: 0 (editable)
    - id: empty-ref
      url: /empty-ref
      type: go-reference
      path: -
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      children: 0 (editable)
  - id: empty-collection
    url: /empty-collection
    type: collection-reference
    path: -
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
  - id: ref1
    url: /ref1
    type: collection-reference
    path: /ref.collection
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
    children: 1 (readonly)
    - id: go
      url: /ref1/go
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 1
      - type: sprite
        id: sprite
        position: {0, 0, 0}
        rotation: {0, 0, 0}
        scale: {1, 1, 1}
      children: 1 (readonly)
      - id: ref
        url: /ref1/ref
        type: go-reference
        path: /ref.go
        position: {0, 0, 0}
        rotation: {0, 0, 0}
        scale: {1, 1, 1}
        components: 0
        children: 0 (readonly)
  - id: readonly
    url: /readonly
    type: collection-reference
    path: /readonly/readonly.collection
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
    children: 0 (readonly)
  - id: readonly1
    url: /readonly1
    type: go-reference
    path: /readonly/readonly.go
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
    components: 0
    children: 1 (editable)
    - id: allowed-child-of-readonly-go
      url: /allowed-child-of-readonly-go
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 0 (editable)
Transaction: edit already existing collection elements
After transaction (edit already existing collection elements):
  children: 5 (editable)
  - id: go
    url: /go
    type: go
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {0.5, 0.5, 0.5}
    components: 0
    children: 5 (editable)
    - id: go1
      url: /go1
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 0 (editable)
    - id: ref
      url: /ref
      type: go-reference
      path: /ref.go
      position: {3.14, 3.14, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 1 (editable)
      - id: new-referenced-go-child
        url: /new-referenced-go-child
        type: go
        position: {0, 0, 0}
        rotation: {0, 0, 0}
        scale: {1, 1, 1}
        components: 0
        children: 0 (editable)
    - id: go2
      url: /go2
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 1 (editable)
      - id: char
        url: /char
        type: go
        position: {0, 0, 0}
        rotation: {0, 0, 0}
        scale: {1, 1, 1}
        components: 3
        - type: sprite
          id: sprite
          position: {0.5, 0.5, 0.5}
          rotation: {0, 0, 0}
          scale: {1, 1, 1}
        - type: collisionobject
          id: collisionobject
          collision_type: collision-object-type-dynamic
          shapes: 1
          - id: box
            type: shape-type-box
            dimensions: 2.5 2.5 2.5
        - type: component-reference
          id: test
        children: 0 (editable)
    - id: empty-ref
      url: /empty-ref
      type: go-reference
      path: -
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      children: 0 (editable)
    - id: new-embedded-go-child
      url: /new-embedded-go-child
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 0 (editable)
  - id: empty-collection
    url: /empty-collection
    type: collection-reference
    path: -
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
  - id: ref1
    url: /ref1
    type: collection-reference
    path: /ref.collection
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
    children: 1 (readonly)
    - id: go
      url: /ref1/go
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 1
      - type: sprite
        id: sprite
        position: {0, 0, 0}
        rotation: {0, 0, 0}
        scale: {1, 1, 1}
      children: 1 (readonly)
      - id: ref
        url: /ref1/ref
        type: go-reference
        path: /ref.go
        position: {0, 0, 0}
        rotation: {0, 0, 0}
        scale: {1, 1, 1}
        components: 0
        children: 0 (readonly)
  - id: readonly
    url: /readonly
    type: collection-reference
    path: /readonly/readonly.collection
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
    children: 0 (readonly)
  - id: readonly1
    url: /readonly1
    type: go-reference
    path: /readonly/readonly.go
    position: {0, 0, 0}
    rotation: {0, 0, 0}
    scale: {1, 1, 1}
    components: 0
    children: 2 (editable)
    - id: allowed-child-of-readonly-go
      url: /allowed-child-of-readonly-go
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 0 (editable)
    - id: new-readonly-referenced-go-child
      url: /new-readonly-referenced-go-child
      type: go
      position: {0, 0, 0}
      rotation: {0, 0, 0}
      scale: {1, 1, 1}
      components: 0
      children: 0 (editable)
Expected collection errors:
  add child to referenced collection => \"children\" is read-only
  remove child of referenced collection => \"children\" is read-only
  add child to go in referenced collection => \"children\" is read-only
  clear children of a go in referenced collection => \"children\" is read-only
  add child to readonly referenced collection => \"children\" is read-only
Transaction: clear collection
After transaction (clear collection)
  children: 0 (editable)
")

(deftest attachment-properties-test
  (test-util/with-loaded-project "test/resources/editor_extensions/transact_attachment_project"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      (expect-script-output expected-attachment-test-output out))))

(def ^:private expected-resources-as-nodes-test-output
  "Directory read:
  can get path: true
  can set path: false
  can get children: true
  can set children: false
  can add children: false
Assets path:
  /assets
Assets images:
  /assets/a.png
  /assets/b.png
Expected errors:
  Setting a property => /assets is not a file resource
")

(deftest resources-as-nodes-test
  (test-util/with-loaded-project "test/resources/editor_extensions/resources_as_nodes_project"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      (expect-script-output expected-resources-as-nodes-test-output out))))

(def ^:private expected-particlefx-test-output
  "Initial state:
emitters: 0
After setup:
emitters: 1
  type: emitter-type-circle
  blend mode: blend-mode-add
  spawn rate: {0, 100, 1, 0}
  initial red: {0, 0.5, 1, 0}
  initial green: {0, 0.8, 1, 0}
  initial blue: {0, 1, 1, 0}
  initial alpha: {0, 0.2, 1, 0}
  life alpha: {0, 0, 0.1, 0.995} {0.2, 1, 1, 0} {1, 0, 1, 0}
  modifiers: 1
    type: modifier-type-acceleration
    magnitude: {0, 1, 1, 0}
    rotation: {0, 0, -180}
Expected errors:
  empty points => {points = {}} does not satisfy any of its requirements:
    - {points = {}} is not a number
    - {} needs at least 1 element
  x outside of range => {points = {{y = 1, tx = 1, x = -1, ty = 0}}} does not satisfy any of its requirements:
    - {points = {{y = 1, tx = 1, x = -1, ty = 0}}} is not a number
    - -1 is not between 0 and 1
  tx outside of range => {points = {{y = 1, tx = -0.5, x = 0, ty = 0}}} does not satisfy any of its requirements:
    - {points = {{y = 1, tx = -0.5, x = 0, ty = 0}}} is not a number
    - -0.5 is not between 0 and 1
  ty outside of range => {points = {{y = 1, tx = 1, x = 0, ty = 2}}} does not satisfy any of its requirements:
    - {points = {{y = 1, tx = 1, x = 0, ty = 2}}} is not a number
    - 2 is not between -1 and 1
  xs not from 0 to 1 (upper) => {points = {{y = 1, tx = 1, x = 0, ty = 0}, {y = 1, tx = 1, x = 0.5, ty = 0}}} does not satisfy any of its requirements:
    - {points = {{y = 1, tx = 1, x = 0, ty = 0}, {y = 1, tx = 1, x = 0.5, ty = 0}}} is not a number
    - {{y = 1, tx = 1, x = 0, ty = 0}, {y = 1, tx = 1, x = 0.5, ty = 0}} does not increase xs monotonically from 0 to 1
  xs not from 0 to 1 (lower) => {points = {{y = 1, tx = 1, x = 0.5, ty = 0}, {y = 1, tx = 1, x = 1, ty = 0}}} does not satisfy any of its requirements:
    - {points = {{y = 1, tx = 1, x = 0.5, ty = 0}, {y = 1, tx = 1, x = 1, ty = 0}}} is not a number
    - {{y = 1, tx = 1, x = 0.5, ty = 0}, {y = 1, tx = 1, x = 1, ty = 0}} does not increase xs monotonically from 0 to 1
  xs not from 0 to 1 (duplicates) => {points = {{y = 1, tx = 1, x = 0, ty = 0}, {y = 1, tx = 1, x = 0, ty = 0}, {y = 1, tx = 1, x = 1, ty = 0}}} does not satisfy any of its requirements:
    - {points = {{y = 1, tx = 1, x = 0, ty = 0}, {y = 1, tx = 1, x = 0, ty = 0}, {y = 1, tx = 1, x = 1, ty = 0}}} is not a number
    - {{y = 1, tx = 1, x = 0, ty = 0}, {y = 1, tx = 1, x = 0, ty = 0}, {y = 1, tx = 1, x = 1, ty = 0}} does not increase xs monotonically from 0 to 1
After clear:
emitters: 0
")

(deftest particlefx-test
  (test-util/with-loaded-project "test/resources/editor_extensions/particlefx_project"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      (expect-script-output expected-particlefx-test-output out))))

(deftest inheritance-chain-test
  (testing "hierarchy adherence"
    (let [h-ref (atom (-> (make-hierarchy)
                          (derive :mammal :animal)
                          (derive :bird :animal)
                          (derive :dog :mammal)
                          (derive :cat :mammal)
                          (derive :sparrow :bird)
                          (derive :eagle :bird)))
          sound-chain (graph/make-inheritance-chain h-ref)]
      (sound-chain :animal (constantly :grunt))
      (sound-chain :mammal (constantly :roar))
      (sound-chain :bird (constantly :chirp))
      (sound-chain :cat #(when (:happy %) :meow))
      (sound-chain :eagle (constantly :screech))
      (is (= :chirp ((sound-chain :sparrow) {})))
      (is (= :screech ((sound-chain :eagle) {})))
      (is (= :roar ((sound-chain :dog) {})))
      (is (= :roar ((sound-chain :cat) {})))
      (is (= :meow ((sound-chain :cat) {:happy true})))))
  (testing "hierarchy modification"
    (let [h-ref (atom (derive (make-hierarchy) :mammal :animal))
          sound-chain (graph/make-inheritance-chain h-ref)]
      (sound-chain :animal (constantly :grunt))
      (sound-chain :mammal (constantly :roar))
      (is (= :roar ((sound-chain :mammal) {})))
      (is (nil? ((sound-chain :cat) {})))
      ;; hierarchy is modified
      (swap! h-ref derive :cat :mammal)
      (is (= :roar ((sound-chain :cat) {})))
      ;; chain is modified
      (sound-chain :cat (constantly :meow))
      (is (= :meow ((sound-chain :cat) {}))))))

(def ^:private expected-game-project-properties-test-output
  "Initial state:
  project.title: unnamed
  sound.gain: 1
  display.width: 960
  display.fullscreen: false
  input.game_binding: /input/game.input_binding
  physics.type: 2D
  project.dependencies: -
Set settings...
After transaction:
  project.title: Set from Editor Script!
  sound.gain: 0.5
  display.width: 1000
  display.fullscreen: true
  input.game_binding: /builtins/input/all.input_binding
  physics.type: 3D
  project.dependencies: https://github.com/defold/extension-spine/archive/refs/tags/3.10.0.zip
Expected errors:
  set undefined property => Can't set property \"gooboo.gaabaa\" of GameProjectNode
  not a string type => 1 is not a string
  not a number type => \"max\" is not a number
  not an integer type => 0.5 is not an integer
  not a boolean type => 1 is not a boolean
  not a valid resource type => resource extension should be input_binding
  not a valid enum value => \"2D+3D\" is not \"2D\" or \"3D\"
  not an array => \"https://defold.com/\" is not an array
  not a valid url => \"latest spine\" is not a valid URL
")

(deftest game-project-properties-test
  (test-util/with-scratch-project "test/resources/editor_extensions/game_project_properties_test"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      (expect-script-output expected-game-project-properties-test-output out))))

(def ^:private expected-localization-output
  "message => Build
localization.and_list({1, 2, message}) => 1, 2, and Build
localization.or_list({1, 2, message}) => 1, 2, or Build
localization.concat({1, 2, message}) => 12Build
localization.concat({1, 2, message}, '/') => 1/2/Build
localization.concat({1, 2, message}, message) => 1Build2BuildBuild
localization.message('progress.loading-resource') => Loading {resource}
localization.message('progress.loading-resource', {resource = 1}) => Loading 1
localization.message('progress.loading-resource', {resource = message}) => Loading Build
")

(deftest localization-test
  (test-util/with-loaded-project "test/resources/editor_extensions/localization_project"
    (let [out (StringBuilder.)]
      (reload-editor-scripts! project :display-output! #(doto out (.append %2) (.append \newline)))
      (run-edit-menu-test-command!)
      (expect-script-output expected-localization-output out))))
