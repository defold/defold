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

(ns integration.editor-extensions-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.editor-extensions :as extensions]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.prefs-functions :as prefs-functions]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.vm :as vm]
            [editor.future :as future]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.pipeline.bob :as bob]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support])
  (:import [org.luaj.vm2 LuaError]))

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
  (selection [_] selection)
  (succeeding-selection [_] [])
  (alt-selection [_] []))

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

(defn- reload-editor-scripts! [project & {:keys [display-output! open-resource! prefs]
                                          :or {display-output! println
                                               open-resource! open-resource-noop!}}]
  (extensions/reload! project :all
                      :prefs (or prefs (test-util/make-test-prefs))
                      :reload-resources! (make-reload-resources-fn (project/workspace project))
                      :display-output! display-output!
                      :save! (make-save-fn project)
                      :open-resource! open-resource!
                      :invoke-bob! (make-invoke-bob-fn project)))

(deftest editor-scripts-commands-test
  (test-util/with-loaded-project "test/resources/editor_extensions/commands_project"
    (let [sprite-outline (:node-id (test-util/outline (test-util/resource-node project "/main/main.collection") [0 0]))]
      (reload-editor-scripts! project)
      (let [handler+context (handler/active
                              (:command (first (handler/realize-menu :editor.outline-view/context-menu-end)))
                              (handler/eval-contexts
                                [(handler/->context :outline {} (->StaticSelection [sprite-outline]))]
                                false)
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
        (is (= 2.5 (test-util/prop sprite-outline :playback-rate)))))))

(deftest refresh-context-after-write-test
  (test-util/with-loaded-project "test/resources/editor_extensions/refresh_context_project"
    (let [output (atom [])
          _ (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
          handler+context (handler/active
                            (:command (first (handler/realize-menu :editor.asset-browser/context-menu-end)))
                            (handler/eval-contexts
                              [(handler/->context :asset-browser {} (->StaticSelection [(test-util/resource-node project "/test.txt")]))]
                              false)
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
                          (handler/eval-contexts
                            [(handler/->context :global {} (->StaticSelection []))]
                            false)
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
                            (handler/eval-contexts
                              [(handler/->context :outline {} (->StaticSelection [node]))]
                              false)
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
  (test-util/with-loaded-project "test/resources/editor_extensions/save_test"
    (let [output (atom [])
          _ (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
          handler+context (handler/active
                            (:command (first (handler/realize-menu :editor.asset-browser/context-menu-end)))
                            (handler/eval-contexts
                              [(handler/->context :asset-browser {} (->StaticSelection []))]
                              false)
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
        coerce #(coerce/coerce vm %1 (vm/->lua %2))]

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
        (is (thrown? LuaError (coerce by-key 42)))))))

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

(deftest ui-test
  (test-util/with-loaded-project "test/resources/editor_extensions/ui_project"
    (let [output (atom [])]
      (reload-editor-scripts! project :display-output! #(swap! output conj [%1 %2]))
      (run-edit-menu-test-command!)
      ;; see test.editor_script: it creates a lot of ui components that should
      ;; form a valid UI tree. In case of any errors the output will get error
      ;; entries.
      (is (= [] @output)))))

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
              [:out "object: table foo"]
              [:out "object: table bar"]
              [:out "object: table baz"]
              [:out "object of: table foo = true, bar = false"]
              [:out "object of: table foo = false, bar = true"]
              [:out "set: table foo = true, bar = true"]
              [:out "set: table foo = true, bar = nil"]
              [:out "string: a string"]
              [:out "string: another_string"]
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