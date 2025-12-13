#pragma once

#include <fstream>
#include <iostream>
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

  // Keywords
  T_MODULE,
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

  // Types
  T_INT32,
  T_INT64,
  T_FLOAT32,
  T_FLOAT64,
  T_SHORT,
  T_STRING_TYPE,
  T_BOOL,
  T_VOID
};

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
  ~Tokenizer();

  void Tokenize();
  const std::vector<Token> &GetTokens() const;
  void PrintTokens() const;

private:
  std::string m_Filename;
  std::string m_Source;
  std::vector<Token> m_Tokens;
  int m_Cursor = 0;
  int m_Line = 1;
  int m_Column = 1;

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
