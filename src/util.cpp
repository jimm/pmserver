#include <string.h>
#include "util.h"

void split_line_into_words(char *line, char *words[]) {
  char *string, **ap;
  int line_len = strlen(line);

  if (line[line_len - 1] == '\n')
    line[line_len - 1] = 0;
  string = line;
  for (ap = words; (*ap = strsep(&string, " ")) != 0; )
    if (**ap != 0)
      if (++ap >= &words[MAX_WORDS])
        break;
  *ap = 0;
}
