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

(ns integration.extension-teal-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code.resource :as code.resource]
            [integration.test-util :as test-util]))

(set! *warn-on-reflection* true)

(def ^:private project-path "test/resources/transpile_teal_project")

(deftest registered-resource-types-test
  (test-util/with-loaded-project project-path
    (is (= #{} (test-util/protobuf-resource-exts-that-read-defaults workspace)))))

(deftest dirty-save-data-test
  (test-util/with-loaded-project project-path
    (test-util/clear-cached-save-data! project)
    (is (= #{} (test-util/dirty-proj-paths project)))
    (test-util/edit-proj-path! project "/modules/mathutils.tl")
    (is (= #{"/modules/mathutils.tl"} (test-util/dirty-proj-paths project)))))

(deftest outputs-test
  (test-util/with-loaded-project project-path
    (let [node-id (test-util/resource-node project "/modules/mathutils.tl")]

      (testing "build-targets"
        (is (not (g/error? (g/node-value node-id :build-targets))))
        (is (= []
               (g/node-value node-id :build-targets))))

      (testing "node-outline"
        (let [node-outline (g/valid-node-value node-id :node-outline)]
          (is (= "tl" (:label node-outline))) ; Defaults to file extension. Should we make it prettier?
          (is (empty? (:children node-outline)))))

      (testing "save-value"
        ;; TODO: No need to call ensure-loaded! soon.
        (g/with-auto-evaluation-context evaluation-context
          (code.resource/ensure-loaded! node-id evaluation-context))
        (is (= ["local Vector = require('modules.vector')"
                ""
                "local m = {}"
                ""
                "function m.distance(a: Vector, b: Vector): number"
                "    return Vector.sub(b, a):len()"
                "end"
                ""
                "return m"
                ""]
               (g/node-value node-id :save-value)))))))

(deftest build-from-saved-state-test
  (test-util/with-scratch-project project-path
    (with-open [_ (test-util/build! (test-util/resource-node project "/game.project"))]
      (let [path->info (test-util/make-build-output-infos-by-path workspace "/main/main.scriptc")]

        (testing "main.script"
          (let [script-pb-map (-> "/main/main.scriptc" path->info :pb-map)]
            (is (= "@main/main.script"
                   (-> script-pb-map :source :filename)))
            (is (= ["modules.mathutils"]
                   (-> script-pb-map :modules)))
            (is (= ["/modules/mathutils.luac"]
                   (-> script-pb-map :resources)))
            (is (= ["local mathutils = require('modules.mathutils')"
                    ""
                    "function init(self)"
                    "    local a = { x = 0, y = 0 }"
                    "    local b = { x = 0, y = 5 }"
                    "    local distance = mathutils.distance(a, b)"
                    "    print('Distance: ' .. distance)"
                    "end"
                    ""]
                   (test-util/lua-module-lines script-pb-map)))))

        (let [transpiled-mathutils-pb-map (-> "/modules/mathutils.luac" path->info :pb-map)]
          (is (= "@modules/mathutils.lua"
                 (-> transpiled-mathutils-pb-map :source :filename)))
          (is (= ["modules.vector"]
                 (-> transpiled-mathutils-pb-map :modules)))
          (is (= ["/modules/vector.luac"]
                 (-> transpiled-mathutils-pb-map :resources)))
          (is (= ["local Vector = require('modules.vector')"
                  ""
                  "local m = {}"
                  ""
                  "function m.distance(a, b)"
                  "   return Vector.sub(b, a):len()"
                  "end"
                  ""
                  "return m"
                  ""]
                 (test-util/lua-module-lines transpiled-mathutils-pb-map))))

        (testing "vector.tl"
          (let [transpiled-vector-pb-map (-> "/modules/vector.luac" path->info :pb-map)]
            (is (= "@modules/vector.lua"
                   (-> transpiled-vector-pb-map :source :filename)))
            (is (empty? (-> transpiled-vector-pb-map :modules)))
            (is (empty? (-> transpiled-vector-pb-map :resources)))
            (is (= ["local Vector = {}"
                    ""
                    ""
                    ""
                    ""
                    "local mt = { __index = Vector }"
                    ""
                    "function Vector.new(x, y)"
                    "   assert(x ~= nil)"
                    "   assert(y ~= nil)"
                    ""
                    "   return setmetatable({"
                    "      x = x,"
                    "      y = y,"
                    "   }, mt)"
                    "end"
                    ""
                    "function Vector:add(other)"
                    "   return Vector.new(self.x + other.x, self.y + other.y)"
                    "end"
                    ""
                    "function Vector:sub(other)"
                    "   return Vector.new(self.x - other.x, self.y - other.y)"
                    "end"
                    ""
                    "function Vector:dot(other)"
                    "   return self.x * other.x + self.y * other.y"
                    "end"
                    ""
                    "function Vector:len()"
                    "   return math.sqrt(Vector.dot(self, self))"
                    "end"
                    ""
                    "return Vector"
                    ""]
                   (test-util/lua-module-lines transpiled-vector-pb-map)))))))))

(deftest build-from-unsaved-state-test
  (test-util/with-scratch-project project-path
    (let [script (test-util/resource-node project "/main/main.script")
          mathutils (test-util/resource-node project "/modules/mathutils.tl")]

      ;; Add new function distance_squared to the mathutils module.
      (test-util/set-code-editor-lines! mathutils
        ["local Vector = require('modules.vector')"
         ""
         "local m = {}"
         ""
         "function m.distance(a: Vector, b: Vector): number"
         "    return Vector.sub(b, a):len()"
         "end"
         ""
         "function m.distance_squared(a: Vector, b: Vector): number"
         "    local between = Vector.sub(b, a)"
         "    return between:dot(between)"
         "end"
         ""
         "return m"
         ""])

      ;; Add a call to added mathutils function distance_squared in our script,
      ;; and add a reference to the previously unreferenced module textutils.
      (test-util/set-code-editor-lines! script
        ["local mathutils = require('modules.mathutils')"
         "local textutils = require('modules.textutils')"
         ""
         "function init(self)"
         "    local a = { x = 0, y = 0 }"
         "    local b = { x = 0, y = 5 }"
         "    local distance = mathutils.distance(a, b)"
         "    local squared_distance = mathutils.distance_squared(a, b)"
         "    local greeting = textutils.format('Hello, {{name}}!', { name = 'World' })"
         "    print('Distance: ' .. distance)"
         "    print('Squared Distance: ' .. squared_distance)"
         "    print('Greeting: ' .. greeting)"
         "end"
         ""]))

    (with-open [_ (test-util/build! (test-util/resource-node project "/game.project"))]
      (let [path->info (test-util/make-build-output-infos-by-path workspace "/main/main.scriptc")]

        (testing "main.script"
          (let [script-pb-map (-> "/main/main.scriptc" path->info :pb-map)]
            (is (= "@main/main.script"
                   (-> script-pb-map :source :filename)))
            (is (= ["modules.mathutils"
                    "modules.textutils"]
                   (-> script-pb-map :modules)))
            (is (= ["/modules/mathutils.luac"
                    "/modules/textutils.luac"]
                   (-> script-pb-map :resources)))))

        (let [transpiled-mathutils-pb-map (-> "/modules/mathutils.luac" path->info :pb-map)]
          (is (= "@modules/mathutils.lua"
                 (-> transpiled-mathutils-pb-map :source :filename)))
          (is (= ["modules.vector"]
                 (-> transpiled-mathutils-pb-map :modules)))
          (is (= ["/modules/vector.luac"]
                 (-> transpiled-mathutils-pb-map :resources)))
          (is (= ["local Vector = require('modules.vector')"
                  ""
                  "local m = {}"
                  ""
                  "function m.distance(a, b)"
                  "   return Vector.sub(b, a):len()"
                  "end"
                  ""
                  "function m.distance_squared(a, b)"
                  "   local between = Vector.sub(b, a)"
                  "   return between:dot(between)"
                  "end"
                  ""
                  "return m"
                  ""]
                 (test-util/lua-module-lines transpiled-mathutils-pb-map))))))

    ;; Undo edits.
    (let [project-graph-id (g/node-id->graph-id project)]
      (g/undo! project-graph-id)
      (g/undo! project-graph-id))

    ;; Rebuild now that we're back at the on-disk state.
    (with-open [_ (test-util/build! (test-util/resource-node project "/game.project"))]
      (let [path->info (test-util/make-build-output-infos-by-path workspace "/main/main.scriptc")]

        (testing "main.script"
          (let [script-pb-map (-> "/main/main.scriptc" path->info :pb-map)]
            (is (= "@main/main.script"
                   (-> script-pb-map :source :filename)))
            (is (= ["modules.mathutils"]
                   (-> script-pb-map :modules)))
            (is (= ["/modules/mathutils.luac"]
                   (-> script-pb-map :resources)))
            (is (= ["local mathutils = require('modules.mathutils')"
                    ""
                    "function init(self)"
                    "    local a = { x = 0, y = 0 }"
                    "    local b = { x = 0, y = 5 }"
                    "    local distance = mathutils.distance(a, b)"
                    "    print('Distance: ' .. distance)"
                    "end"
                    ""]
                   (test-util/lua-module-lines script-pb-map)))))

        (let [transpiled-mathutils-pb-map (-> "/modules/mathutils.luac" path->info :pb-map)]
          (is (= "@modules/mathutils.lua"
                 (-> transpiled-mathutils-pb-map :source :filename)))
          (is (= ["modules.vector"]
                 (-> transpiled-mathutils-pb-map :modules)))
          (is (= ["/modules/vector.luac"]
                 (-> transpiled-mathutils-pb-map :resources)))
          (is (= ["local Vector = require('modules.vector')"
                  ""
                  "local m = {}"
                  ""
                  "function m.distance(a, b)"
                  "   return Vector.sub(b, a):len()"
                  "end"
                  ""
                  "return m"
                  ""]
                 (test-util/lua-module-lines transpiled-mathutils-pb-map))))))))
