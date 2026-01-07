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

(ns editor.editor-extensions.http-server
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.graph :as graph]
            [editor.editor-extensions.runtime :as rt]
            [editor.future :as future]
            [editor.workspace :as workspace]
            [reitit.impl]
            [util.coll :as coll]
            [util.http-server :as http-server])
  (:import [java.io InputStream]
           [java.nio.file Path]
           [org.luaj.vm2 LuaError]))

(set! *warn-on-reflection* true)

;; region response

;; region generic response

(def ^:private response-header-coercer
  (coerce/wrap-with-pred
    coerce/string
    (fn is-all-lower-case? [^String s]
      (let [n (.length s)]
        (if (zero? n)
          false
          (loop [i 0]
            (or (= i n)
                (and (not (Character/isUpperCase (.charAt s i)))
                     (recur (inc i))))))))
    "is not lower-case"))

(def ^:private headers-coercer
  (coerce/map-of response-header-coercer coerce/string))

(def ^:private response-args-coercer
  (coerce/regex :status :? coerce/integer
                :headers :? headers-coercer
                :body :? coerce/string))

(def ^:private ext-response-fn
  (rt/varargs-lua-fn ext-response [{:keys [rt]} varargs]
    (-> (rt/->clj rt response-args-coercer varargs)
        (with-meta {:type :response})
        (rt/wrap-userdata "http.server.response(...)"))))

;; endregion

;; region json response

(def json-value-coercer
  (coerce/recursive
    #(coerce/one-of
       coerce/null
       coerce/boolean
       coerce/number
       coerce/string
       (coerce/vector-of % :min-count 1)
       (coerce/map-of coerce/string %))))

(def ^:private json-response-args-coercer
  (coerce/regex :body json-value-coercer
                :status :? coerce/integer
                :headers :? headers-coercer))

(def ^:private ext-json-response-fn
  (rt/varargs-lua-fn ext-json-response [{:keys [rt]} varargs]
    (let [{:keys [body status headers] :or {status 200}} (rt/->clj rt json-response-args-coercer varargs)]
      (-> (http-server/json-response body status headers)
          (with-meta {:type :response})
          (rt/wrap-userdata "http.server.json_response(...)")))))

;; endregion

;; region external file response

(def ^:private external-file-response-args-coercer
  (coerce/regex :path coerce/string
                :status :? coerce/integer
                :headers :? headers-coercer))

(defn- make-ext-external-file-response-fn [^Path project-path]
  (rt/varargs-lua-fn ext-external-file-response [{:keys [rt]} varargs]
    (let [{:keys [status headers path] :or {status 200}} (rt/->clj rt external-file-response-args-coercer varargs)]
      (-> (http-server/response status headers (.normalize (.resolve project-path ^String path)))
          (with-meta {:type :response})
          (rt/wrap-userdata "http.server.external_file_response(...)")))))

;; endregion

;; region resource response

(def ^:private resource-response-args-coercer
  (coerce/regex :resource-path graph/resource-path-coercer
                :status :? coerce/integer
                :headers :? headers-coercer))

(defn- make-ext-resource-response-fn [workspace]
  (rt/varargs-lua-fn ext-resource-response [{:keys [rt evaluation-context]} varargs]
    (let [{:keys [status headers resource-path] :or {status 200}} (rt/->clj rt resource-response-args-coercer varargs)]
      (let [resource (workspace/resolve-workspace-resource workspace resource-path evaluation-context)]
        (-> (http-server/response status headers resource)
            (with-meta {:type :response})
            (rt/wrap-userdata "http.server.resource_response(...)"))))))

;; endregion

;; endregion

;; region route

(def ^:private request-method-coercer
  (coerce/wrap-with-pred
    coerce/string
    (fn is-all-upper-case? [^String s]
      (let [n (.length s)]
        (if (zero? n)
          false
          (loop [i 0]
            (or (= i n)
                (and (Character/isUpperCase (.charAt s i))
                     (recur (inc i))))))))
    "is not upper-case"))

(def ^:private route-args-coercer
  (coerce/regex :path coerce/string
                :method :? request-method-coercer
                :as :? (coerce/enum :string :json)
                :handler coerce/function))

(def ^:private reserved-path-param-names
  #{:method :path :query :headers :body})

(def ^:private response-coercer
  (coerce/one-of
    (coerce/wrap-with-pred coerce/userdata #(= :response (:type (meta %))) "is not a response")
    response-args-coercer))

(def ^:private ext-route-fn
  (rt/varargs-lua-fn ext-route [{:keys [rt]} varargs]
    (let [{:keys [path method as handler]
           :or {method "GET" as :discard}} (rt/->clj rt route-args-coercer varargs)
          {:keys [path-params]} (reitit.impl/parse path {:syntax :bracket})]
      (run!
        (fn [path-param]
          (when (contains? reserved-path-param-names path-param)
            (throw (LuaError. (str "Cannot use reserved path pattern name: " (name path-param))))))
        path-params)
      (-> {:path path
           :method method
           :handler (bound-fn ext-http-request-handler [request]
                      (let [request (-> request
                                        (dissoc :path-params)
                                        (coll/merge (:path-params request)))]
                        (try
                          (let [request (case as
                                          :discard (do (.close ^InputStream (:body request))
                                                       (dissoc request :body))
                                          :string (update request :body slurp)
                                          :json (with-open [r (io/reader (:body request))]
                                                  (assoc request :body (json/read r))))]
                            (-> (rt/invoke-suspending rt handler (rt/->lua request))
                                (future/then
                                  (fn [varargs]
                                    (let [{:keys [status headers body]
                                           :or {status 200}} (rt/->clj rt response-coercer varargs)]
                                      (http-server/response status headers body))))
                                (future/catch
                                  (fn [e]
                                    (.println (rt/stderr rt) (or (ex-message e) (.getSimpleName (class e))))
                                    http-server/internal-server-error))))
                          (catch Throwable e
                            (http-server/response 400 (or (ex-message e) (.getSimpleName (class e))))))))}
          (with-meta {:type :route})
          (rt/wrap-userdata "http.server.route(...)")))))

;; endregion

(def routes-coercer
  (coerce/vector-of
    (coerce/wrap-with-pred coerce/userdata #(= :route (:type (meta %))) "is not a route")))

(defn env [workspace project-path web-server]
  {;; constants
   "local_url" (http-server/local-url web-server)
   "port" (http-server/port web-server)
   "url" (http-server/url web-server)
   ;; functions
   "external_file_response" (make-ext-external-file-response-fn project-path)
   "json_response" ext-json-response-fn
   "resource_response" (make-ext-resource-response-fn workspace)
   "response" ext-response-fn
   "route" ext-route-fn})
