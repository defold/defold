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

(ns util.ensure)

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defmacro argument
  ([argument-sym pred]
   `(argument ~argument-sym ~pred "Invalid %s argument: %s" ~argument-sym))
  ([argument-sym pred message-fmt & extra-message-fmt-args]
   {:pre [(symbol? argument-sym)
          (string? message-fmt)]}
   `(if (~pred ~argument-sym)
      nil
      (let [^String message#
            ~(list* `format
                    message-fmt
                    (name argument-sym)
                    extra-message-fmt-args)]
        (throw (IllegalArgumentException. message#))))))

(defmacro argument-satisfies
  [argument-sym expected-protocol-sym]
  {:pre [(symbol? argument-sym)
         (symbol? expected-protocol-sym)
         (some-> expected-protocol-sym resolve var-get :on-interface)]}
  `(argument
     ~argument-sym
     #(satisfies? ~expected-protocol-sym %)
     "%s argument must satisfy the protocol %s - got %s"
     (.getName ^Class (:on-interface ~expected-protocol-sym))
     (some-> ~argument-sym class .getName)))

(defmacro argument-type
  [argument-sym expected-class-sym]
  {:pre [(symbol? argument-sym)
         (symbol? expected-class-sym)
         (class? (resolve expected-class-sym))]}
  `(argument
     ~argument-sym
     #(instance? ~expected-class-sym %)
     "%s argument must be %s - got %s"
     (.getName ~expected-class-sym)
     (some-> ~argument-sym class .getName)))
