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

(ns editor.lsp.lua-language-server-plugin-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all])
  (:import [org.luaj.vm2 Globals LuaValue]
           [org.luaj.vm2.lib.jse JsePlatform]))

(set! *warn-on-reflection* true)

(def ^:private plugin-source
  (delay (slurp (io/resource "lua-language-server/plugin.lua"))))

(defn- plugin-diffs [uri source]
  (let [^Globals globals (JsePlatform/standardGlobals)
        chunk (.load globals ^String @plugin-source "@lua-language-server/plugin.lua")
        _ (.call chunk)
        on-set-text (.get globals "OnSetText")
        ^LuaValue result (.call on-set-text
                                (LuaValue/valueOf ^String uri)
                                (LuaValue/valueOf ^String source))]
    (if (.isnil result)
      []
      (loop [index 1
             diffs []]
        (if (> index (.length result))
          diffs
          (let [^LuaValue diff (.get result (int index))]
            (recur (inc index)
                   (conj diffs
                         {:start (.toint (.get diff "start"))
                          :finish (.toint (.get diff "finish"))
                          :text (.tojstring (.get diff "text"))}))))))))

(defn- apply-plugin [uri source]
  (reduce (fn [text {:keys [start finish] replacement :text}]
            (str (subs text 0 (dec start))
                 replacement
                 (subs text finish)))
          source
          (sort-by :start > (plugin-diffs uri source))))

(deftest annotates-script-lifecycle-functions-test
  (let [source (str "function init(self, params)\n"
                    "end\n\n"
                    "function on_message(self, message_id, message, sender)\n"
                    "end\n\n"
                    "function on_input(context, id, data)\n"
                    "end\n")]
    (is (= (str "---@param self script_instance\n"
                "---@param params userdata\n"
                "function init(self, params)\n"
                "end\n\n"
                "---@param self script_instance\n"
                "---@param message_id hash\n"
                "---@param message table<any, any>\n"
                "---@param sender url\n"
                "function on_message(self, message_id, message, sender)\n"
                "end\n\n"
                "---@param context script_instance\n"
                "---@param id hash|nil\n"
                "---@param data on_input.action\n"
                "---@return boolean|nil\n"
                "function on_input(context, id, data)\n"
                "end\n")
           (apply-plugin "file:///project/main.script" source)))))

(deftest lifecycle-functions-are-resource-specific-test
  (is (= "function late_update(self, dt) end"
         (apply-plugin "file:///project/main.gui_script"
                       "function late_update(self, dt) end")))
  (is (= "function final(self) end"
         (apply-plugin "file:///project/main.render_script"
                       "function final(self) end")))
  (is (= "function update(self, dt) end"
         (apply-plugin "file:///project/module.lua"
                       "function update(self, dt) end")))
  (is (= (str "---@param self script_instance\n"
              "---@param dt number\n"
              "function update(self, dt) end")
         (apply-plugin "file:///project/main.render_script"
                       "function update(self, dt) end"))))

(deftest annotates-every-supported-lifecycle-function-test
  (let [script-source (str "function init(self) end\n"
                           "function final(self) end\n"
                           "function update(self, dt) end\n"
                           "function late_update(self, dt) end\n"
                           "function fixed_update(self, dt) end\n"
                           "function on_message(self, message_id, message, sender) end\n"
                           "function on_input(self, action_id, action) end\n"
                           "function on_reload(self) end\n")
        gui-source (str "function init(self) end\n"
                        "function final(self) end\n"
                        "function update(self, dt) end\n"
                        "function on_message(self, message_id, message, sender) end\n"
                        "function on_input(self, action_id, action) end\n"
                        "function on_reload(self) end\n")
        render-source (str "function init(self) end\n"
                           "function update(self, dt) end\n"
                           "function on_message(self, message_id, message, sender) end\n"
                           "function on_reload(self) end\n")]
    (is (= 8 (count (plugin-diffs "file:///project/main.script" script-source))))
    (is (= 6 (count (plugin-diffs "file:///project/main.gui_script" gui-source))))
    (is (= 4 (count (plugin-diffs "file:///project/main.render_script" render-source))))))

(deftest preserves-source-line-endings-test
  (is (= (str "---@param self script_instance\r\n"
              "---@param dt number\r\n"
              "function update(self, dt) end\r\n")
         (apply-plugin "file:///project/main.script"
                       "function update(self, dt) end\r\n"))))

(deftest annotates-assigned-global-lifecycle-functions-test
  (let [source (str "init = function(self) end\n\n"
                    "_G.update = function(context, delta) end\n\n"
                    "function _G.on_reload(self) end\n\n"
                    "local final = function(self) end\n"
                    "callbacks.on_message = function(self, message_id, message, sender) end\n")]
    (is (= (str "---@param self script_instance\n"
                "init = function(self) end\n\n"
                "---@param context script_instance\n"
                "---@param delta number\n"
                "_G.update = function(context, delta) end\n\n"
                "---@param self script_instance\n"
                "function _G.on_reload(self) end\n\n"
                "local final = function(self) end\n"
                "callbacks.on_message = function(self, message_id, message, sender) end\n")
           (apply-plugin "file:///project/main.script" source)))))

(deftest annotates-multiline-lifecycle-definitions-test
  (is (= (str "---@param context script_instance\n"
              "---@param delta number\n"
              "function\n"
              "update(\n"
              "    context,\n"
              "    delta\n"
              ") end\n")
         (apply-plugin "file:///project/main.script"
                       (str "function\n"
                            "update(\n"
                            "    context,\n"
                            "    delta\n"
                            ") end\n")))))

(deftest preserves-user-lifecycle-annotations-test
  (let [source (str "---@param context custom_context\n"
                    "---@diagnostic disable-next-line: lowercase-global\n"
                    "function update(context, delta)\n"
                    "end\n\n"
                    "---@return true\n"
                    "function on_input(self, action_id, action)\n"
                    "end\n")]
    (is (= (str "---@param delta number\n"
                "---@param context custom_context\n"
                "---@diagnostic disable-next-line: lowercase-global\n"
                "function update(context, delta)\n"
                "end\n\n"
                "---@param self script_instance\n"
                "---@param action_id hash|nil\n"
                "---@param action on_input.action\n"
                "---@return true\n"
                "function on_input(self, action_id, action)\n"
                "end\n")
           (apply-plugin "file:///project/main.script" source)))))

(deftest ignores-lifecycle-text-outside-global-function-definitions-test
  (let [source (str "local text = [[\n"
                    "function update(self, dt)\n"
                    "]]\n"
                    "-- function update(self, dt)\n"
                    "--[[\n"
                    "function update(self, dt)\n"
                    "]]\n"
                    "local function update(self, dt) end\n\n"
                    "if true then\n"
                    "    function update(context, delta) end\n"
                    "end\n")]
    (is (= (str "local text = [[\n"
                "function update(self, dt)\n"
                "]]\n"
                "-- function update(self, dt)\n"
                "--[[\n"
                "function update(self, dt)\n"
                "]]\n"
                "local function update(self, dt) end\n\n"
                "if true then\n"
                "    ---@param context script_instance\n"
                "    ---@param delta number\n"
                "    function update(context, delta) end\n"
                "end\n")
           (apply-plugin "file:///project/main.script" source)))))
