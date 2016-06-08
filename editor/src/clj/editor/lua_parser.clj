(ns editor.lua-parser
  (:require [clj-antlr.core :as antlr]
            [clojure.java.io :as io]
            [clojure.walk :as walk]
            [clojure.zip :as zip]
            [clojure.edn :as edn]))

(def lua-parser (antlr/parser (slurp (io/resource "Lua.g4")) {:throw? false}))

(defn- parse-namelist [namelist]
  (vec (remove #(= "," %) (rest namelist))))

(defn- require-name [loc]
  (if (zip/end? loc)
    nil
    (let [node (zip/node loc)]
      (if (and (seq? node) (= :string (first node)))
        (edn/read-string (second node))
        (recur (zip/next loc))))))

(defn- has-require? [loc]
  (if (zip/end? loc)
    nil
    (let [node (zip/node loc)]
      (if (and (seq? node) (= :var (first node)) (= "require" (second node)))
        node
        (recur (zip/next loc))))))

(defn collect-require [parsed-names exprlist]
  (if exprlist
    (let [zexpr (zip/seq-zip exprlist)]
      (when (has-require? zexpr)
        {(first parsed-names) (require-name zexpr)}))
    {}))

(defmulti xform-node (fn [node] (keyword (second node))))

(defmethod xform-node :default [node]
  nil)

(defmethod xform-node :local [node]
  (let [[_ _ namelist _ exprlist] node
        parsed-names (parse-namelist namelist)
        require-info (collect-require parsed-names exprlist)]
    {:locals parsed-names :requires require-info}))

(defmethod xform-node :function [node]
  (let [[_ _ [_ funcname] funcbody] node
        [_ _  [_  namelist]] funcbody]
    {:functions {funcname {:params (parse-namelist namelist)}}}))

(defn collect-info [loc]
  (loop [loc loc
        result []]
   (if (zip/end? loc)
     result
     (let [node (zip/node loc)]
       (if (and (seq? node) (= :stat (first node)))
         (recur (zip/next loc) (conj result (xform-node node)))
         (recur (zip/next loc) result))))))


(defn lua-info [code]
  (let [info (-> code lua-parser zip/seq-zip collect-info)
        locals-info (vec (apply concat (map :locals info)))
        functions-info (or (apply merge (map :functions info)) {})
        requires-info (or (apply merge (map :requires info)) {})]
    {:locals locals-info
     :functions functions-info
     :requires requires-info}))


(comment

  (lua-info "local d \n local e")

  (collect-info  (zip/seq-zip (lua-parser "local d \n local e")))
  (apply merge-with concat  [{:locals ["d"]} {:locals ["e"]}])


  [(:stat "local" (:namelist "d")) (:stat "local" (:namelist "e"))]
  (collect-info  (zip/seq-zip (lua-parser "local d,e ")))
  (collect-info  (zip/seq-zip (lua-parser "local mymathmodule = require(\"mymath\")")))


  (collect-info  (zip/seq-zip (lua-parser "function oadd(num1, num2) return num1 end")))

  (lua-parser "function oadd(num1, num2) return num1 end")
  (lua-info "function oadd(num1, num2) return num1 end")
  (:chunk (:block (:stat "function" (:funcname "oadd") (:funcbody "(" (:parlist (:namelist "num1" "," "num2")) ")" (:block (:retstat "return" (:explist (:exp (:prefixexp (:varOrExp (:var "num1"))))))) "end"))) "<EOF>")

  (lua-parser "local mymathmodule = require(\"mymath\")")
  (lua-info "local mymathmodule = require(\"mymath\")")
  (:chunk (:block (:stat "local" (:namelist "mymathmodule") "=" (:explist (:exp (:prefixexp (:varOrExp (:var "require")) (:nameAndArgs (:args "(" (:explist (:exp (:string "\"mymath\""))) ")"))))))) "<EOF>")
  (:chunk (:block (:stat (:varlist (:var "mymathmodule")) "=" (:explist (:exp (:prefixexp (:varOrExp (:var "require")) (:nameAndArgs (:args "(" (:explist (:exp (:string "\"mymath\""))) ")"))))))) "<EOF>")





  )
