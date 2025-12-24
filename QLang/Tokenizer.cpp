#include "Tokenizer.h"
#include "QError.h"
#include <cctype>
#include <sstream>

Tokenizer::Tokenizer(const std::string &filename) : m_Filename(filename) {
  ReadFile();
}

Tokenizer::Tokenizer(const std::string &source, bool isSource)
    : m_Source(source), m_Filename("<source>") {}

Tokenizer::Tokenizer(const std::string &filename,
                     std::shared_ptr<QErrorCollector> errorCollector)
    : m_Filename(filename), m_ErrorCollector(errorCollector) {
  ReadFile();
}

// String source constructor
Tokenizer::Tokenizer(const std::string &source, bool isSource,
                     std::shared_ptr<QErrorCollector> errorCollector)
    : m_Source(source), m_Filename("<source>"),
      m_ErrorCollector(errorCollector) {
  if (m_ErrorCollector) {
    m_ErrorCollector->SetSource(m_Source);
  }
}

Tokenizer::~Tokenizer() {}

void Tokenizer::ReportError(const std::string &message) {
  if (m_ErrorCollector) {
    m_ErrorCollector->ReportError(QErrorSeverity::Error, message, m_Line,
                                  m_Column, 0, "tokenizer");
  } else {
    std::cerr << "[TOKENIZER ERROR] " << message << " at " << m_Line << ":"
              << m_Column << std::endl;
  }
}

void Tokenizer::Tokenize() {
  while (!IsAtEnd()) {
    ScanToken();
  }
  AddToken(TokenType::T_EOF, "");
}

const std::vector<Token> &Tokenizer::GetTokens() const { return m_Tokens; }

void Tokenizer::PrintTokens() const {
  for (const auto &token : m_Tokens) {
    std::string typeStr;
    switch (token.type) {
    case TokenType::T_EOF:
      typeStr = "T_EOF";
      break;
    case TokenType::T_UNKNOWN:
      typeStr = "T_UNKNOWN";
      break;
    case TokenType::T_IDENTIFIER:
      typeStr = "T_IDENTIFIER";
      break;
    case TokenType::T_INTEGER:
      typeStr = "T_INTEGER";
      break;
    case TokenType::T_FLOAT:
      typeStr = "T_FLOAT";
      break;
    case TokenType::T_STRING:
      typeStr = "T_STRING";
      break;
    case TokenType::T_OPERATOR:
      typeStr = "T_OPERATOR";
      break;
    case TokenType::T_END_OF_LINE:
      typeStr = "T_END_OF_LINE";
      break;
    case TokenType::T_COMMA:
      typeStr = "T_COMMA";
      break;
    case TokenType::T_DOT:
      typeStr = "T_DOT";
      break;
    case TokenType::T_COLON:
      typeStr = "T_COLON";
      break;
    case TokenType::T_LPAREN:
      typeStr = "T_LPAREN";
      break;
    case TokenType::T_RPAREN:
      typeStr = "T_RPAREN";
      break;
    case TokenType::T_LBRACE:
      typeStr = "T_LBRACE";
      break;
    case TokenType::T_RBRACE:
      typeStr = "T_RBRACE";
      break;
    case TokenType::T_LBRACKET:
      typeStr = "T_LBRACKET";
      break;
    case TokenType::T_RBRACKET:
      typeStr = "T_RBRACKET";
      break;
    case TokenType::T_MODULE:
      typeStr = "T_MODULE";
      break;
    case TokenType::T_END:
      typeStr = "T_END";
      break;
    case TokenType::T_IF:
      typeStr = "T_IF";
      break;
    case TokenType::T_ELSE:
      typeStr = "T_ELSE";
      break;
    case TokenType::T_ELSEIF:
      typeStr = "T_ELSEIF";
      break;
    case TokenType::T_FOR:
      typeStr = "T_FOR";
      break;
    case TokenType::T_TO:
      typeStr = "T_TO";
      break;
    case TokenType::T_NEXT:
      typeStr = "T_NEXT";
      break;
    case TokenType::T_WHILE:
      typeStr = "T_WHILE";
      break;
    case TokenType::T_WEND:
      typeStr = "T_WEND";
      break;
    case TokenType::T_CLASS:
      typeStr = "T_CLASS";
      break;
    case TokenType::T_METHOD:
      typeStr = "T_METHOD";
      break;
    case TokenType::T_NEW:
      typeStr = "T_NEW";
      break;
    case TokenType::T_RETURN:
      typeStr = "T_RETURN";
      break;
    case TokenType::T_INT32:
      typeStr = "T_INT32";
      break;
    case TokenType::T_INT64:
      typeStr = "T_INT64";
      break;
    case TokenType::T_FLOAT32:
      typeStr = "T_FLOAT32";
      break;
    case TokenType::T_FLOAT64:
      typeStr = "T_FLOAT64";
      break;
    case TokenType::T_SHORT:
      typeStr = "T_SHORT";
      break;
    case TokenType::T_STRING_TYPE:
      typeStr = "T_STRING_TYPE";
      break;
    case TokenType::T_BOOL:
      typeStr = "T_BOOL";
      break;
    case TokenType::T_VOID:
      typeStr = "T_VOID";
      break;
    }
#if QLANG_DEBUG
    std::cout << "Token(" << typeStr << ", '" << token.value
              << "', Line: " << token.line << ", Col: " << token.column << ")"
              << std::endl;
#endif
  }
}

void Tokenizer::ReadFile() {
  std::ifstream file(m_Filename);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << m_Filename << std::endl;
    return;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  m_Source = buffer.str();
  m_Cursor = 0;
  m_Line = 1;
  m_Column = 1;

  if (m_ErrorCollector) {
    m_ErrorCollector->SetSource(m_Source);
  }
}

char Tokenizer::Peek(int offset) const {
  if (m_Cursor + offset >= m_Source.length())
    return '\0';
  return m_Source[m_Cursor + offset];
}

char Tokenizer::Advance() {
  char c = m_Source[m_Cursor++];
  if (c == '\n') {
    m_Line++;
    m_Column = 1;
  } else {
    m_Column++;
  }
  return c;
}

bool Tokenizer::IsAtEnd() const { return m_Cursor >= m_Source.length(); }

void Tokenizer::ScanToken() {
  char c = Peek();

  // Handle newline explicitly to generate token
  if (c == '\n') {
    Advance();
    AddToken(TokenType::T_END_OF_LINE, "\n");
    return;
  }

  if (isspace(c)) {
    Advance();
    return;
  }

  // Check for comments
  if (c == '/') {
    char next = Peek(1);
    if (next == '/') {
      // Single-line comment - skip until end of line
      while (!IsAtEnd() && Peek() != '\n') {
        Advance();
      }
      return;
    } else if (next == '*') {
      // Multi-line comment - skip until */
      Advance(); // consume /
      Advance(); // consume *
      while (!IsAtEnd()) {
        if (Peek() == '*' && Peek(1) == '/') {
          Advance(); // consume *
          Advance(); // consume /
          break;
        }
        Advance();
      }
      return;
    }
  }

  if (isalpha(c) || c == '_') {
    ScanIdentifierOrKeyword();
  } else if (isdigit(c)) {
    ScanNumber();
  } else if (c == '"') {
    ScanString();
  } else {
    ScanOperatorOrPunctuation();
  }
}

void Tokenizer::ScanIdentifierOrKeyword() {
  std::string value;
  int startCol = m_Column;
  while (isalnum(Peek()) || Peek() == '_') {
    value += Advance();
  }

  // Check for specific keywords
  TokenType type = TokenType::T_IDENTIFIER;
  if (value == "module") {
    type = TokenType::T_MODULE;
  } else if (value == "import") {
    type = TokenType::T_IMPORT;
  } else if (value == "end") {
    type = TokenType::T_END;
  } else if (value == "if") {
    type = TokenType::T_IF;
  } else if (value == "else") {
    type = TokenType::T_ELSE;
  } else if (value == "elseif") {
    type = TokenType::T_ELSEIF;
  } else if (value == "for") {
    type = TokenType::T_FOR;
  } else if (value == "class") {
    type = TokenType::T_CLASS;
  } else if (value == "static") {
    type = TokenType::T_STATIC;
  } else if (value == "method") {
    type = TokenType::T_METHOD;
  } else if (value == "new") {
    type = TokenType::T_NEW;
  } else if (value == "return") {
    type = TokenType::T_RETURN;
  } else if (value == "int32") {
    type = TokenType::T_INT32;
  } else if (value == "int64") {
    type = TokenType::T_INT64;
  } else if (value == "float32") {
    type = TokenType::T_FLOAT32;
  } else if (value == "float64") {
    type = TokenType::T_FLOAT64;
  } else if (value == "short") {
    type = TokenType::T_SHORT;
  } else if (value == "string") {
    type = TokenType::T_STRING_TYPE;
  } else if (value == "bool") {
    type = TokenType::T_BOOL;
  } else if (value == "void") {
    type = TokenType::T_VOID;
  } else if (value == "true") {
    type = TokenType::T_TRUE;
  } else if (value == "false") {
    type = TokenType::T_FALSE;
  } else if (value == "this") {
    type = TokenType::T_THIS;
  } else if (value == "to") {
    type = TokenType::T_TO;
  } else if (value == "next") {
    type = TokenType::T_NEXT;
  } else if (value == "while") {
    type = TokenType::T_WHILE;
  } else if (value == "wend") {
    type = TokenType::T_WEND;
  } else if (value == "cptr") {
    type = TokenType::T_CPTR;
  } else if (value == "null") {
    type = TokenType::T_NULL;
  }

  // Construct manually to keep start column
  Token token;
  token.type = type;
  token.value = value;
  token.line = m_Line;
  token.column = startCol;
  m_Tokens.push_back(token);
}

void Tokenizer::ScanNumber() {
  std::string value;
  int startCol = m_Column;
  bool isFloat = false;

  while (isdigit(Peek())) {
    value += Advance();
  }

  if (Peek() == '.' && isdigit(Peek(1))) {
    isFloat = true;
    value += Advance(); // Consume '.'
    while (isdigit(Peek())) {
      value += Advance();
    }
  }

  Token token;
  token.type = isFloat ? TokenType::T_FLOAT : TokenType::T_INTEGER;
  token.value = value;
  token.line = m_Line;
  token.column = startCol;
  m_Tokens.push_back(token);
}

void Tokenizer::ScanString() {
  Advance(); // Consume opening "
  std::string value;
  int startCol = m_Column; // Correct logic would be start of string

  while (Peek() != '"' && !IsAtEnd()) {
    if (Peek() == '\n')
      m_Line++;
    value += Advance();
  }

  if (IsAtEnd()) {
    std::cerr << "Unterminated string at line " << m_Line << std::endl;
    return;
  }

  Advance(); // Consume closing "

  Token token;
  token.type = TokenType::T_STRING;
  token.value = value;
  token.line = m_Line;
  token.column = startCol; // Approximate
  m_Tokens.push_back(token);
}

void Tokenizer::ScanOperatorOrPunctuation() {
  char c = Advance();
  std::string value(1, c);
  TokenType type = TokenType::T_UNKNOWN;

  switch (c) {
  case '+':
    type = TokenType::T_OPERATOR;
    // Check for ++ or +=
    if (Peek() == '+' || Peek() == '=') {
      value += Advance();
    }
    break;
  case '-':
    type = TokenType::T_OPERATOR;
    // Check for -- or -=
    if (Peek() == '-' || Peek() == '=') {
      value += Advance();
    }
    break;
  case '*':
  case '/':
  case '=':
  case '!':
    type = TokenType::T_OPERATOR;
    // Check for 2-char operators like ==, !=
    if (Peek() == '=') {
      value += Advance();
    }
    break;
  case '<':
    // Check for <= (comparison operator)
    if (Peek() == '=') {
      value += Advance();
      type = TokenType::T_OPERATOR;
    } else {
      type = TokenType::T_LESS; // For generics
    }
    break;
  case '>':
    // Check for >= (comparison operator)
    if (Peek() == '=') {
      value += Advance();
      type = TokenType::T_OPERATOR;
    } else {
      type = TokenType::T_GREATER; // For generics
    }
    break;
  case '&':
    type = TokenType::T_OPERATOR;
    // Check for &&
    if (Peek() == '&') {
      value += Advance();
    }
    break;
  case '|':
    type = TokenType::T_OPERATOR;
    // Check for ||
    if (Peek() == '|') {
      value += Advance();
    }
    break;
  case ';':
    type = TokenType::T_END_OF_LINE;
    break;
  case ',':
    type = TokenType::T_COMMA;
    break;
  case '.':
    type = TokenType::T_DOT;
    break;
  case ':':
    type = TokenType::T_COLON;
    break;
  case '(':
    type = TokenType::T_LPAREN;
    break;
  case ')':
    type = TokenType::T_RPAREN;
    break;
  case '{':
    type = TokenType::T_LBRACE;
    break;
  case '}':
    type = TokenType::T_RBRACE;
    break;
  case '[':
    type = TokenType::T_LBRACKET;
    break;
  case ']':
    type = TokenType::T_RBRACKET;
    break;
  }

  AddToken(type, value);
}

void Tokenizer::AddToken(TokenType type, const std::string &value) {
  Token token;
  token.type = type;
  token.value = value;
  token.line = m_Line;
  token.column = m_Column - value.length(); // Simplified column tracking
  m_Tokens.push_back(token);
}
