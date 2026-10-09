// Strict native parser/serializer roundtrip without rewriting the source.
#include "../../remaster/profile.h"
#include <iostream>
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  RemasterProfile a,b;
  std::vector<RemasterProfileDiagnostic> errors;
  if (!S9xRemasterLoadProfile(argv[1],a,errors)) return 1;
  std::string first,second;
  if (!S9xRemasterSerializeProfile(a,first,errors) ||
      !S9xRemasterParseProfile(first,b,errors) ||
      !S9xRemasterSerializeProfile(b,second,errors) || first != second) return 1;
  std::cout << "strict native canonical roundtrip PASS\n";
}
