# Defold API documentation

The Defold API documentation is generated from source file comments, similar to Doxygen but uses
a proprietary python tool to generate json and sdoc files that the Editor and website uses for
rendering documentation and provide completion.

## Documentation formatting

Documentation comments are denoted by comments in the following format, a standard multiline C/C++ comment with an appended hash character:

```
/*#
 *
 */
```

An API document usually consists of a series of doc comments, each one containing documentation for a function, a message, a property or a variable.

The first line of the comment contains a one line BRIEF of what the comment is about and subsequent lines up to the first @tag should contain a DESCRIPTION. These blocks of text are
processed by Markdown and can thus contain either markdown syntax or plain HTML.

```
/*# this is the one line concise brief of this doc comment
 *
 * Here follows a description of the doc comment. This could span any number of
 * lines and should in general describe the functionality or use of the function,
 * message, variable or property in question.
 *
 * - Here is a bulleted list
 * - With a set of things that make this doc comment nice
 * - And easy to understand
 *
 * @some_tag
 * ...
 */
```




Since the documentation for a specific namespace (component or feature) might span several files, a documentation information doc comment should be added to each file. This comment contains what namespace the documentation belongs to (so it can be grouped with documentation from other files) as well as descriptive text:

```
/*# Facebook API documentation
 *
 * Functions and constants for interacting with Facebook APIs.
 *
 * @document
 * @name Facebook
 * @namespace facebook
 */
```

## Markdown support






## Conventions

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
 * - `request_id`: transaction request id. (only if receipt is set and for Facebook IAP transactions when used in the iap.buy call parameters)
 * - `receipt`: receipt (only set when state == TRANS_STATE_PURCHASED or TRANS_STATE_UNVERIFIED)
 *
 * `error`
 * : [type:table] a table containing any error information. The error parameter is `nil` on success.
 *
 */
```