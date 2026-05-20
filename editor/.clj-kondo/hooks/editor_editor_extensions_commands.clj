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
