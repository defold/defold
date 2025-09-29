# Defold API documentation

The Defold API documentation is generated from source file comments, similar to Doxygen but uses a proprietary python tool to generate json and sdoc files that the Editor and website uses for rendering documentation and provide completion.

## Markdown support

The documentation formatter accepts Markdown with a few extensions:

Fenced codeblocks
: Denote with three backticks. The first one can contain a language specifier for syntax coloring:

    ```
    function init(self)
        local pos = go.get("player", "position")
    end
    ```

    ```lua
    function init(self)
        local pos = go.get("player", "position")
    end
    ```

Table support
: Tables can be written simply:

```
First Header   | Second Header
-------------- | -------------
Cell 1 content | Content from cell 2
Another row    | Cell 2 second row
```

Definition lists
: This is such a list with definition terms and description pairs. These are used to list function parameters.

```
term 1
: Description for term 1

term 2:
: Description for term 2
```

Type information
: Type denotations for parameters and return values are written with special syntax to allow them to be rendered clearly. The type text is arbitrary, but for consistency, use the following forms:

```
[type:object]
[type:vector3]
[type:vector4]
[type:vector]
[type:quaternion]
[type:url]
[type:hash]
[type:boolean]
[type:table]
[type:number]
[type:string]
[type:constant]

// Multiple types possible
[type:string|hash|url]

// Function signature
[type:function(self, transaction, error)]
```

Icons
: Icons can be included in the documents to call attention to some text, indicate a platform specific note etc.

```
[icon:attention]
[icon:ios]
[icon:android]
[icon:macos]
[icon:linux]
[icon:windows]
[icon:google]
[icon:amazon]
```

References
: Cross references can be created by the following forms:

```
// Expands to a link to "get_id" in the current document
[ref:get_id]

// Expands to a link to "get_id" in the "go" document
[ref:go.get_id]
```

Custom span classes
: Generic forms `[name:text]` that does not match "icon", "type" or "ref" are expanded to span elements with class `name`:

```
// Expands to <span class="mark">READ ONLY</span>
[mark:READ ONLY]
```

## Documentation formatting

Documentation comments are denoted by comments in the following format, a standard multiline C/C++ comment with an appended hash character:

```
/*#
 *
 */
```

An API document usually consists of a series of doc comments, each one containing documentation for a function, a @message, a @property, a @constant or the @document itself.

The first line of the comment contains a one line BRIEF of what the comment is about and subsequent lines up to the first type tag (denoted by '@' + the type name) should contain a DESCRIPTION. These blocks of text are processed by Markdown and can thus contain either markdown syntax or plain HTML.

```
/*# this is the one line concise brief of this doc comment
 * Here follows a description of the doc comment. This could span any number of
 * lines and should in general describe the functionality or use of the function,
 * message, variable or property in question.
 *
 * - Here is a bulleted list
 * - With a set of things that make this doc comment nice
 * - And easy to understand
 *
 * @some_valid_type
 * @name some name
 */
```

All doc comment types require a @name tag which is used to set a name for the doc comment. This is typically the name of the function, variable or property.

## The @document tag

Since the documentation for a specific namespace (component or feature) might span several files, a documentation information doc comment should be added to each file.

The @document comment should contain a brief, a description, what @namespace the documentation belongs to (so it can be grouped with documentation from other files) as well as a @name:

```
/*# MyFeature API documentation
 *
 * Functions and constants for interacting with MyFeature APIs.
 *
 * @document
 * @name MyFeature
 * @namespace myfeature
 * @path path/to/this/file.cpp
 */
```

## Functions

There is no function tag. If the doc comment does not contain any of the other valid type tags, the type is implicitly set to function.

```
/*# decode JSON from a string to a lua-table
 * Decode a string of JSON data into a Lua table.
 * A Lua error is raised for syntax errors.
 *
 * @name json.decode
 * @param json [type:string] json data
 * @return data [type:table] decoded json
 *
 */
```

## The @message, @constant and @property tags

Use these tags to denote doc comments for messages:

```
/*# applies a force on a collision object
 * Post this message to a collision-object-component to apply the specified force on the collision object.
 * The collision object must be dynamic.
 *
 * @message
 * @name apply_force
 * @param force [type:vector3] the force to be applied on the collision object, measured in Newton
 * @param position [type:vector3] the position where the force should be applied
 * @examples
 *
 * Assuming the instance of the script has a collision-object-component with id "co":
 *
 * ```lua
 * -- apply a force of 1 Newton towards world-x at the center of the game object instance
 * msg.post("#co", "apply_force", {force = vmath.vector3(1, 0, 0), position = go.get_world_position()})
 * ```
 */
```

and for constants:

```
/*# RGB image type
 *
 * @name image.TYPE_RGB
 * @constant
 */
```

For properties, the type of the property is written first in the brief.

```
/*# [type:vector3] game object position
 *
 * The position of the game object.
 * The type of the property is vector3.
 *
 * @name position
 * @property
 *
 * @examples
 *
 * How to query a game object's position, either as a vector3 or selecting a specific dimension:
 *
 * ```lua
 * function init(self)
 *   -- get position from "player"
 *   local pos = go.get("player", "position")
 *   local posx = go.get("player", "position.x")
 *   -- do something useful
 *   assert(pos.x == posx)
 * end
 * ```
 */
```

## The @param tag

For functions and messages, the @param tag is used to denote the parameters the function or message takes. Each parameter starts with the @param tag, followed by a string of characters that make out the name of the parameter, a whitespace and then the documentation for the parameter which runs until the next @tag or end of the doc comment.

```
 * @param name_of_parameter documentation of the parameter. This documentation can
 * run for several lines up until the next @tag.
 * @param another_parameter documentation of the second parameter.
```

Optional parameters are written with the name surrounded by square brackets:

```
 * @param [optional_parameter] documentation of the parameter
```

Type information for parameters are by convention written first in the documentation:

```
 * @param parameter_name [type:some_type] documentation of the parameter
```

Also by convention definition lists are used to lay out the details of function argument signatures, like callback functions. The type of each function argument is written first in the definition description:

```
/*# set transaction listener
 *
 * Set the callback function to receive transaction events.
 *
 * @name iap.set_listener
 * @param listener [type:function(self, transaction, error)] listener callback function
 *
 * `self`
 * : [type:object] The current object.
 *
 * `transaction`
 * : [type:table] a table describing the transaction. The table contains the following fields:
 *
 * - `ident`: product identifier
 * - `state`: transaction state
 * - `date`: transaction date
 * - `original_trans`: original transaction (only set when state == TRANS_STATE_RESTORED)
 * - `trans_ident` : transaction identifier (only set when state == TRANS_STATE_RESTORED, TRANS_STATE_UNVERIFIED or TRANS_STATE_PURCHASED)
 * - `request_id`: transaction request id. (only if receipt is set and for transactions when used in the iap.buy call parameters)
 * - `receipt`: receipt (only set when state == TRANS_STATE_PURCHASED or TRANS_STATE_UNVERIFIED)
 *
 * `error`
 * : [type:table] a table containing any error information. The error parameter is `nil` on success.
 *
 */
```

## The @return tag

The @return tag is used identically as the @param tag. Each return value starts with the @return tag, followed by a string of characters that make out the name of the parameter, a whitespace and then the documentation for the value which runs until the next @tag or end of the doc comment.
The return value is named to allow for clearer description of multiple return values.

```
 * @return rms_left [type:number] RMS value for left channel
 * @return rms_right [type:number] RMS value for right channel
```

## The @examples tag

This tag gathers documentation for example usages. The documentation runs up to the next valid @tag or to the end of the doc comment.

```
 * @examples
 *
 * How to start a simple color animation, where the node fades in to white during 0.5 seconds:
 *
 * ```lua
 * gui.set_color(node, vmath.vector4(0, 0, 0, 0)) -- node is fully transparent
 * gui.animate(node, gui.PROP_COLOR, vmath.vector4(1, 1, 1, 1), gui.EASING_INOUTQUAD, 0.5) -- start animation
 * ```
 *
 * How to start a sequenced animation where the node fades in to white during 0.5 seconds, stays visible for 2 seconds and then fades out:
 *
 * ```lua
 * local function on_animation_done(self, node)
 *     -- fade out node, but wait 2 seconds before the animation starts
 *     gui.animate(node, gui.PROP_COLOR, vmath.vector4(0, 0, 0, 0), gui.EASING_OUTQUAD, 0.5, 2.0)
 * end
 *
 * function init(self)
 *     -- fetch the node we want to animate
 *     local my_node = gui.get_node("my_node")
 *     -- node is initially set to fully transparent
 *     gui.set_color(my_node, vmath.vector4(0, 0, 0, 0))
 *     -- animate the node immediately and call on_animation_done when the animation has completed
 *     gui.animate(my_node, gui.PROP_COLOR, vmath.vector4(1, 1, 1, 1), gui.EASING_INOUTQUAD, 0.5, 0.0, on_animation_done)
 * end
 * ```
```
