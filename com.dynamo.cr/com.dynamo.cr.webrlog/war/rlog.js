function showError(message) {
    var templ = $("#templ-error-alert").html();
    var em = $("#error-messages").append(templ);
    $(em).find("div.errorMessage").html(message);
}
