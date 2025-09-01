## Using Portal While Working on the Defold Editor

Portal is a web-based UI for inspecting Clojure values that can run standalone in a web browser or be integrated into various popular IDEs. Once running, you can call `(tap> some-value)` to inspect it in Portal.

This PR introduces a new `portal` Leiningen profile that will include the relevant dependencies to run Portal when working on the Defold Editor, as well as some editor-specific functionality, such as inspecting graph nodes and resources.

Portal is only a simple value inspector. If you desire full REPL-replacement with more advanced capabilities, look into using [Reveal](https://vlaaad.github.io/reveal/) instead. It has similar Defold Editor-specific inspection capabilities and more.

### Setup Instructions

Install the relevant Portal plugin for your IDE of choice. This enables you to run the Portal inspector in a docked panel inside your editor.
  * Cursive: https://plugins.jetbrains.com/plugin/18467-portal-inspector/
  * Visual Studio Code: https://marketplace.visualstudio.com/items?itemName=djblue.portal

Once installed,
  * Use `lein with-profile +portal ...` when starting up to include Portal
    on the class path.
  * Evaluate the following expression through the REPL connection:
    ```clj
    ((requiring-resolve 'editor.portal/open!))
    ```
    You might want to set up a REPL Command for this in your IDE.

### General Functionality
* Call `(tap> some-value)` to inspect any Clojure value or Java object in Portal.
* In Portal, you can double-click a `node-id` or a `Resource` value to inspect the corresponding node.

### Node View Functionality
This view shows details related to a node in the graph.

* Nodes that belong to a `ResourceNode` will display their owner `Resource`.
* Override nodes will show the chain of original `node-ids`.
* The **properties** map shows raw field values, and includes defaults when viewing a base node. When viewing an override node, only overridden values are shown.
* For the **inputs** and **outputs**, we show incoming and outgoing connections instead of values.
* Double-click the `:keyword` for a **property**, **input**, or **output** to evaluate it.
