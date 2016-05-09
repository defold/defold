
var normalize_error = function(errorObject) {
    var message = errorObject.message;
    var stack = errorObject.stack;

    // normalize message -> remove line if exists, move it to stack trace
    var callLine = /at (\S+:\d*$)/.exec(message);
    if (callLine) {
        message = message.replace(/(at \S+:\d*$)/, "");
        stack = callLine[1] + "\n" + stack;
    }

    // TODO - remove URL from network errors
    // normalize runtime stack for chrome
    message = message.replace(/(abort\(.+\)) at .+/, "$1");

    // remove GET arguments
    stack = stack.replace(/\?{1}\S+(:\d+:\d+)/g, "$1");
    // WebKit stack without entry name
    stack = stack.replace(/ *at (\S+)$/gm, "@$1");
    // normalize WebKit's stack of function calls (also named functions)
    stack = stack.replace(/ *at (\S+)(?: \[as \S+\])? +\((.+)\)/g, "$1@$2");
    // remove specific decorations
    stack = stack.replace(/^((?:Object|Array)\.)/gm, "");

    stack = stack.split("\n");

    return { stack:stack, message:message };
}

var get_error_object = function (err, url, line, column, errObj) {
    line = typeof line == "undefined" ? 0 : line;
    column = typeof column == "undefined" ? 0 : column;
    url = typeof url == "undefined" ? "" : url;
    var errorLine = url + ":" + line + ":" + column;

    var error = errObj || (typeof window.event != "undefined" ? window.event.error : "" ) || err || "Undefined Error";
    var message = "";
    var stack = "";
    var backtrace = "";

    if (typeof error == "object" && typeof error.stack != "undefined" && typeof error.message != "undefined") {
        stack = String(error.stack);
        message = String(error.message);
    } else {
        stack = String(error).split("\n");
        message = stack.shift();
        stack = stack.join("\n");
    }
    stack = stack || errorLine;

    return normalize_error({ message: message, stack: stack });
};

var crash_handler = function (err, url, line, column, errObj) {
    var errorObject = get_error_object(err, url, line, column, errObj);
    Module.ccall('JSWriteDump', 'null',
        ['string', 'string'], [errorObject.message, JSON.stringify(errorObject.stack)]);

    return true;
};

window.onerror = crash_handler;