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

(ns hooks.editor-editor-extensions-commands
  (:require [clj-kondo.hooks-api :as api]))

(defn gen-query [{:keys [node]}]
  (let [[_ acc-node bindings-node & body] (:children node)
        [env-node cont-node] (:children bindings-node)
        lua-fn-sym (gensym "lua-fn__")]
    {:node
     (api/list-node
       [(api/token-node 'fn)
        (api/vector-node [(api/token-node lua-fn-sym)])
        (api/list-node
          [(api/token-node 'fn)
           (api/vector-node [env-node])
           (api/list-node
             (list*
               (api/token-node 'let)
               (api/vector-node
                 [cont-node
                  (api/list-node
                    [(api/token-node 'partial)
                     (api/token-node 'editor.editor-extensions.commands/continue)
                     acc-node
                     env-node
                     (api/token-node lua-fn-sym)])])
               body))])])}))
