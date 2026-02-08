#include <ctype.h>
void str_to_lower(char *s) {
  for (; *s; s++) {
    *s = tolower((unsigned char)*s);
  }
}
