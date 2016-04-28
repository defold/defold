{{#symbols}}
extern "C" void {{.}}();
{{/symbols}}

void dmExportedSymbols() {
    {{#symbols}}
    {{.}}();
    {{/symbols}}
}
