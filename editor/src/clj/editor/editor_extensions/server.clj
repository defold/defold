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

(ns editor.editor-extensions.server
  (:require [dynamo.graph :as g]
            [editor.editor-extensions :as extensions]
            [editor.editor-extensions.runtime :as rt]
            [editor.future :as future]
            [editor.web-server :as web-server]
            [util.http-server :as http-server])
  (:import [java.io StringWriter]
           [org.luaj.vm2 LuaError Varargs]))

(set! *warn-on-reflection* true)

(defn- error-text [e]
  (or (ex-message e) (.getSimpleName (class e))))

(defn- eval-request [request project token]
  (web-server/require-authorized! request token)
  (let [prototype (try
                    (-> request :body slurp (rt/read "eval"))
                    (catch LuaError e
                      (throw (http-server/error (http-server/response 422 (error-text e))))))
        output (StringWriter.)
        rt (:rt (g/with-auto-evaluation-context evaluation-context
                  (extensions/ext-state project evaluation-context)))]
    (when-not rt
      (throw (http-server/error (http-server/response 503 "Editor extension runtime is not ready\n"))))
    (-> rt
        (rt/invoke-suspending {:override-out output :override-err output} (rt/bind rt prototype))
        (future/then
          (fn [^Varargs varargs]
            (dotimes [i (.narg varargs)]
              (doto output
                (.append "=> ")
                (.append (str (.arg varargs (inc i))))
                (.append \newline)))
            (http-server/response 200 (str output))))
        (future/catch
          (fn [e]
            (if (instance? LuaError e)
              (http-server/response 422 (str output (error-text e)))
              (throw e)))))))

(defn routes [project token]
  {"/eval" {"POST" (with-meta
                     (bound-fn [request] (eval-request request project token))
                     {:openapi {:summary "Evaluate Lua code in the editor extension runtime"
                                :security [{"token" []}]
                                :requestBody {:required true
                                              :content {"text/plain" {:example "print('hi') return 1, 2"}}}
                                :responses {"200" {:description "Printed output + returned values"
                                                   :content {"text/plain" {:example "hi\n=> 1\n=> 2"}}}
                                            "401" {:description "Missing or invalid bearer token"}
                                            "422" {:description "Printed output + error"}}}})}})
