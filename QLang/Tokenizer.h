#pragma once

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

enum class TokenType {
  T_EOF,
  T_UNKNOWN,
  T_IDENTIFIER,
  T_INTEGER,
  T_FLOAT,
  T_STRING,
  T_OPERATOR,

  // Punctuation
  T_END_OF_LINE, // ;
  T_COMMA,       // ,
  T_DOT,         // .
  T_COLON,       // :
  T_LPAREN,      // (
  T_RPAREN,      // )
  T_LBRACE,      // {
  T_RBRACE,      // }
  T_LBRACKET,    // [
  T_RBRACKET,    // ]
  T_LESS,        // < (for generics)
  T_GREATER,     // > (for generics)

  // Keywords
  T_MODULE,
  T_IMPORT,
  T_END,
  T_CLASS,
  T_METHOD,
  T_NEW,
  T_RETURN,
  T_IF,
  T_ELSE,
  T_ELSEIF,
  T_FOR,
  T_TRUE,
  T_FALSE,
  T_THIS,
  T_TO,
  T_NEXT,
  T_WHILE,
  T_WEND,
  T_NULL,   // null keyword
  T_STATIC, // static class keyword

  // Types
  T_INT32,
  T_INT64,
  T_FLOAT32,
  T_FLOAT64,
  T_SHORT,
  T_STRING_TYPE,
  T_BOOL,
  T_VOID,
  T_CPTR // C pointer (void*) for C++/script interop
};

class QErrorCollector;

struct Token {
  TokenType type;
  std::string value;
  int line;
  int column;
};

class Tokenizer {
public:
  Tokenizer(const std::string &filename);
  Tokenizer(const std::string &source, bool isSource);
  // Constructor with error collector
  Tokenizer(const std::string &filename,
            std::shared_ptr<QErrorCollector> errorCollector);
  Tokenizer(const std::string &source, bool isSource,
            std::shared_ptr<QErrorCollector> errorCollector);
  ~Tokenizer();

  void Tokenize();
  const std::vector<Token> &GetTokens() const;
  void PrintTokens() const;

  // Error access
  std::shared_ptr<QErrorCollector> GetErrorCollector() const {
    return m_ErrorCollector;
  }

private:
  std::string m_Filename;
  std::string m_Source;
  std::vector<Token> m_Tokens;
  std::shared_ptr<QErrorCollector> m_ErrorCollector;
  int m_Cursor = 0;
  int m_Line = 1;
  int m_Column = 1;

  void ReportError(const std::string &message);

  void ReadFile();
  char Peek(int offset = 0) const;
  char Advance();
  bool IsAtEnd() const;

  void ScanToken();
  void ScanIdentifierOrKeyword();
  void ScanNumber();
  void ScanString();
  void ScanOperatorOrPunctuation();

  void AddToken(TokenType type, const std::string &value);
};
