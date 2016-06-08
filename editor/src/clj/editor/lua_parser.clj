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
  (when exprlist
    (let [zexpr (zip/seq-zip exprlist)]
     {(first parsed-names) (if (has-require? zexpr)
                             (require-name zexpr)
                             :not-found)})))

(defmulti xform-node (fn [node] (keyword (second node))))

(defmethod xform-node :default [node]
  nil)

(defmethod xform-node :local [node]
  (let [[_ _ namelist _ exprlist] node
        parsed-names (parse-namelist namelist)]
    {:locals parsed-names
     :requires (collect-require parsed-names exprlist)}))

(defmethod xform-node :function [node]
  (let [[_ _ [_ funcname] funcbody] node
        [_ _  [_  namelist]] funcbody]
    {:functions {funcname (parse-namelist namelist)}}))

(defn collect-info [loc]
  (loop [loc loc
        result []]
   (if (zip/end? loc)
     result
     (let [node (zip/node loc)]
       (if (and (seq? node) (= :stat (first node)))
         (recur (zip/next loc) (conj result (xform-node node)))
         (recur (zip/next loc) result))))))


(collect-info  (zip/seq-zip (lua-parser "local d \n local e")))
(apply merge-with concat  [{:locals ["d"]} {:locals ["e"]}])


[(:stat "local" (:namelist "d")) (:stat "local" (:namelist "e"))]
(collect-info  (zip/seq-zip (lua-parser "local d,e ")))
(collect-info  (zip/seq-zip (lua-parser "local mymathmodule = require(\"mymath\")")))


(collect-info  (zip/seq-zip (lua-parser "function oadd(num1, num2) return num1 end")))

(lua-parser "function oadd(num1, num2) return num1 end")
(:chunk (:block (:stat "function" (:funcname "oadd") (:funcbody "(" (:parlist (:namelist "num1" "," "num2")) ")" (:block (:retstat "return" (:explist (:exp (:prefixexp (:varOrExp (:var "num1"))))))) "end"))) "<EOF>")

(lua-parser "local mymathmodule = require(\"mymath\")")
(:chunk (:block (:stat "local" (:namelist "mymathmodule") "=" (:explist (:exp (:prefixexp (:varOrExp (:var "require")) (:nameAndArgs (:args "(" (:explist (:exp (:string "\"mymath\""))) ")"))))))) "<EOF>")
(:chunk (:block (:stat (:varlist (:var "mymathmodule")) "=" (:explist (:exp (:prefixexp (:varOrExp (:var "require")) (:nameAndArgs (:args "(" (:explist (:exp (:string "\"mymath\""))) ")"))))))) "<EOF>")





