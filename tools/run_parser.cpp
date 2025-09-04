#include <iostream>
#include <fstream>
#include <sstream>
#include "../src/parser.h"

int main(int argc, char **argv) {
  std::string path = "tests/test_smoke.oo";
  if (argc > 1) path = argv[1];
  std::ifstream in(path);
  if (!in) { std::cerr << "failed to open " << path << "\n"; return 2; }
  std::ostringstream ss; ss << in.rdbuf();
  std::string src = ss.str();
  // Debug: print source size and a short visible snippet to inspect offsets
  std::cout << "src.size()=" << src.size() << "\n";
  std::cout << "src[0..256]:\n";
  for (size_t i = 0; i < src.size() && i < 256; ++i) {
    char c = src[i];
    if (c == '\n') std::cout << "\\n";
    else std::cout << c;
  }
  std::cout << "\n--- end snippet ---\n";
  // Diagnostic: print first tokens from the start
  {
    Lexer Lstart(src);
    std::cout << "Initial tokens:\n";
    for (int i = 0; i < 40; ++i) {
      Token t = Lstart.nextToken();
    //   std::cout << "  kind=" << (int)t.kind << " text='" << t.text << "' pos=" << t.pos << "\n";
      if (t.kind == TokenKind::Tok_EOF) break;
    }
  }

  Parser p(src);
  auto res = p.parse();
  if (res.ok) {
    std::cout << "Parse OK\n";
    return 0;
  } else {
    std::cout << "Parse error: " << res.error << "\n";
    // Diagnostic: dump a few remaining tokens from a fresh lexer to help debug what's left
    {
      Lexer L(src);
      Token t;
      std::cout << "Remaining tokens:\n";
      for (int i = 0; i < 20; ++i) {
        t = L.nextToken();
        // std::cout << "  kind=" << (int)t.kind << " text='" << t.text << "' pos=" << t.pos << "\n";
        if (t.kind == TokenKind::Tok_EOF) break;
      }
    }
    return 1;
  }
}
