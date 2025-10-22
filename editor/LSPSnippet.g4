grammar LSPSnippet;

snippet:
    (text
    |text_esc
    |syntax_rule
    |err
    |newline)*;

text: (TEXT|ID);
text_esc: BACK_SLASH (BACK_SLASH | DOLLAR);
err:
      DOLLAR
    | BACK_SLASH
    | OPEN
    | CLOSE
    | INT
    | COLON
    | PIPE
    | COMMA;
syntax_rule: tab_stop|placeholder|choice|variable;
newline: NL|CR;

tab_stop: naked_tab_stop | curly_tab_stop;
naked_tab_stop: DOLLAR INT;
curly_tab_stop: DOLLAR OPEN INT CLOSE;

placeholder:
    DOLLAR
    OPEN
    INT
    COLON
    placeholder_content
    CLOSE;
placeholder_content:
    (syntax_rule
    |placeholder_text
    |placeholder_esc
    |newline)*;
placeholder_text:
      TEXT
    | ID
    | INT
    | OPEN
    | COLON
    | PIPE
    | COMMA;
placeholder_esc:
    BACK_SLASH (BACK_SLASH | DOLLAR | CLOSE);

choice:
    DOLLAR
    OPEN
    INT
    PIPE
    choice_elements
    PIPE
    CLOSE;
choice_elements:
    (choice_element (COMMA choice_element)*)?;
choice_element:
    (choice_text
    |choice_esc
    |newline)+;
choice_text: TEXT | ID | INT | OPEN | COLON;
choice_esc:
    BACK_SLASH
    (BACK_SLASH | DOLLAR | CLOSE | PIPE | COMMA);

variable:
      naked_variable
    | curly_variable
    | placeholder_variable;
variable_text: ID (ID|INT)*;
naked_variable: DOLLAR variable_text;
curly_variable: DOLLAR OPEN variable_text CLOSE;
placeholder_variable:
    DOLLAR
    OPEN
    variable_text
    COLON
    placeholder_content
    CLOSE;

TEXT: ~[\\$0-9{}:|,\r\n_a-zA-Z]+;
ID: [_a-zA-Z]+;
NL: '\r'? '\n';
CR: '\r';
INT: [0-9]+;
DOLLAR: '$';
BACK_SLASH: '\\';
OPEN: '{';
CLOSE: '}';
COLON: ':';
PIPE: '|';
COMMA: ',';