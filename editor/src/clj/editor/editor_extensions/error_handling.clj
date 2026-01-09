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

(ns editor.editor-extensions.error-handling
  (:require [clojure.spec.alpha :as s])
  (:import [org.luaj.vm2 OrphanedThread]))

(set! *warn-on-reflection* true)

(defn display-script-error!
  "Submit labeled error message from the exception to the display output"
  [display-output! label ex]
  (display-output! :err (str label " failed: " (or (ex-message ex) (.getSimpleName (class ex))))))

(s/fdef try-with-extension-exceptions
        :args (s/cat :kv-args (s/+ (s/cat :k #{:display-output! :label :catch} :v any?)) :expr any?))
(defmacro try-with-extension-exceptions
  "Convenience macro for executing an expression and reporting extension errors
  to the console

  Leading kv-args:
    :display-output!    required, 2-arg fn to display error output
    :label              optional, string error prefix, defaults to \"Extension\"
    :catch              optional, result value in case of exceptions (otherwise
                        the Exception is re-thrown)"
  [& kv-args+expr]
  (let [opts (apply hash-map (butlast kv-args+expr))
        expr (last kv-args+expr)
        ex-sym (gensym "ex")]
    (when-not (:display-output! opts)
      (throw (Exception. ":display-output! is required")))
    `(try
       ~expr
       (catch OrphanedThread ~ex-sym (throw ~ex-sym))
       (catch Throwable ~ex-sym
         (display-script-error! ~(:display-output! opts) ~(:label opts "Extension") ~ex-sym)
         ~(:catch opts `(throw ~ex-sym))))))
