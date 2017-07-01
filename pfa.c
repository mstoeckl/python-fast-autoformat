#include <stdio.h>
#include <string.h>

#define BUFSIZE 4096

/* Cast characters to the simplest equivalent character. Thus A -> a, 4 -> 0 */
char classification_table[256];

/* Tokens: things which can't be split.
 * For instance,
 *   TOK_LABEL includes everything from 'import' to 'quit' to 'try'
 *   TOK_NUMBER is: 3j, 1.05e-55
 *   TOK_STRING is: a'BDEFDSF' r'GSGFDG' b"\"\"\'''" """ afsfa """
 *   TOK_OBRACE is: any of ( { [
 *   TOK_CBRACE is: any of ] } )
 *   TOK_COMMENT is: a comment!
 *   TOK_OPERATOR is: * ^ | |= @=
 *   TOK_EXP is: **
 *   TOK_PM is: + -
 *   TOK_COMMOLON is: : ,
 * Note that all our token definitions are by left/right spacing action
 * For instance, TOK_OPERATOR unconditionally spaces left/right
 *               TOK_COMMOLON inserts a right space unless followed by
 * TOK_CBRACE
 *               TOK_LABEL has no right gap relative to TOK_OBRACE
 *               TOK_EXP binds close on the left and on the right
 *               TOK_PM acts like TOK_OPERATOR, except when it binds right
 */
enum {
  TOK_LABEL,
  TOK_NUMBER,
  TOK_STRING,
  TOK_TRISTR,
  TOK_OBRACE,
  TOK_CBRACE,
  TOK_COMMENT,
  TOK_OPERATOR,
  TOK_COMMOLON,
  TOK_EXP,
  TOK_INBETWEEN,
  TOK_LCONT,
  TOK_DOT,
};

const char *tok_to_string(int tok) {
  switch (tok) {
  case TOK_LABEL:
    return "LAB";
  case TOK_NUMBER:
    return "NUM";
  case TOK_STRING:
    return "STR";
  case TOK_TRISTR:
    return "TST";
  case TOK_OBRACE:
    return "OBR";
  case TOK_CBRACE:
    return "CBR";
  case TOK_COMMENT:
    return "CMT";
  case TOK_OPERATOR:
    return "OPR";
  case TOK_EXP:
    return "EXP";
  case TOK_COMMOLON:
    return "CLM";
  case TOK_INBETWEEN:
    return "INB";
  case TOK_LCONT:
    return "LCO";
  case TOK_DOT:
    return "DOT";
  default:
    return "???";
  }
}

int isalpha_lead(char c) {
  if (c > 127)
    return 1;
  if ('a' <= c && c <= 'z')
    return 1;
  if ('A' <= c && c <= 'Z')
    return 1;
  if ('_' == c)
    return 1;
  return 0;
}
int isnumeric_lead(char c) {
  if ('0' <= c && c <= '9')
    return 1;
  if ('.' == c)
    return 1;
  return 0;
}

int isoptype(char c) {
  if (c == '=' || c == '+' || c == '-' || c == '@' || c == '|' || c == '^' ||
      c == '&' || c == '*' || c == '/' || c == '<' || c == '>')
    return 1;
  return 0;
}

void pyformat(FILE *file, FILE *out) {
  char linebuf[BUFSIZE];
  const char *lbufend = &linebuf[BUFSIZE - 1];
  char *lbufstart = linebuf;

  char tokbuf[BUFSIZE];
  int toks[BUFSIZE];
  char *tokd = NULL;
  int ntoks = 0;

  char string_starter = '\0';
  int is_continuation = 0;
  int inside_string = 0;
  int leading_spaces = 0;
  int ltok = TOK_INBETWEEN;
  int neof = 0;
  int was_last_empty = 0;
  while (1) {
    char *readct = fgets(lbufstart, lbufend - lbufstart, file);
    if (!readct) {
      if (!is_continuation || neof) {
        break;
      } else {
        neof = 1;
      }
    }

    if (!is_continuation) {
      ntoks = 0;
      tokd = tokbuf;
    }

    /* token-split the line with NULL characters; double NULL is eof */

    /* STATE MACHINE TOKENIZE */
    char *cur = lbufstart;
    int llen = readct ? strlen(cur) : 0;
    /* space char gives room for termination checks */
    if (is_continuation && ltok == TOK_TRISTR) {
      cur[llen - 1] = '\n';
    } else {
      cur[llen - 1] = ' ';
    }
    cur[llen] = '\0';
    cur[llen + 1] = '\0';

    if (!is_continuation) {
      leading_spaces = 0;
      for (; cur[0] == ' ' || cur[0] == '\t'; cur++) {
        leading_spaces++;
      }
    } else {
      for (; cur[0] == ' ' || cur[0] == '\t'; cur++) {
      }
    }
    int rle = strlen(cur);
    if (rle == 0) {
      if (was_last_empty) {
        is_continuation = 0;
        continue;
      }
      was_last_empty = 1;
    } else {
      was_last_empty = 0;
    }

    int proctok = TOK_INBETWEEN;
    if (is_continuation && ltok == TOK_TRISTR) {
      proctok = TOK_TRISTR;
    }

    char *eolpos = &cur[strlen(cur) - 1];
    char lopchar = '\0';
    int numlen = 0;
    int nstrescps = 0;
    int nstrleads = 0;
    for (; cur[0]; cur++) {
      /* STATE MACHINE GOES HERE */
      if (cur[0] == '\t') {
        cur[0] = ' ';
      }
      if (!inside_string && cur[0] == ' ' && cur[1] == ' ') {
        continue;
      }
      /* SO: single space is a token boundary ... */
      char nxt = *cur;
      int ignore = 0;
      int tokfin = 0;
      int otok = proctok;
      switch (proctok) {
      case TOK_LABEL: {
        if (isalpha_lead(nxt)) {

        } else {
          tokfin = 1;
          proctok = TOK_INBETWEEN;
          ignore = 1;
          cur--;
        }
      } break;
      case TOK_DOT:
      case TOK_NUMBER: {
        /* We don't care about the number itself, just that things stay
         * numberish */
        int isdot = (numlen == 1) && !isnumeric_lead(nxt);
        if (!isdot && isnumeric_lead(nxt)) {
        } else if (!isdot && (nxt == 'e' || nxt == 'x')) {

        } else {
          if (cur[-1] == '.' && numlen == 1)
            otok = TOK_DOT;

          tokfin = 1;
          proctok = TOK_INBETWEEN;
          ignore = 1;
          cur--;
        }
        numlen++;
      } break;
      case TOK_STRING: {
        /* The fun one */
        int ffin = 0;
        if (nxt == string_starter) {
          nstrleads++;
        } else {
          if (nstrleads == 2) {
            /* implicitly to the end */
            ffin = 1;
            cur--;
            ignore = 1;
          } else {
            nstrleads = 0;
          }
        }
        if (nstrleads == 3) {
          proctok = TOK_TRISTR;
          nstrleads = 0;
          nstrescps = 0;
        } else if (!ffin && nstrleads == 2) {
          /* doubled */
        } else if ((nxt != string_starter ||
                    (nstrescps % 2 == 1 && nxt == string_starter)) &&
                   !ffin) {
          if (nxt == '\\') {
            nstrescps++;
          } else {
            nstrescps = 0;
          }
        } else {
          tokfin = 1;
          proctok = TOK_INBETWEEN;
        }
      } break;
      case TOK_TRISTR: {
        /* Only entry this once we've been in TOK_STRING */
        if (nxt == string_starter && nstrescps % 2 == 0) {
          nstrleads++;
        } else {
          nstrleads = 0;
        }
        if ((nxt != string_starter ||
             (nstrescps % 2 == 1 && nxt == string_starter))) {
          if (nxt == '\\') {
            nstrescps++;
          } else {
            nstrescps = 0;
          }
        }
        if (nstrleads == 3) {
          tokfin = 1;
          proctok = TOK_INBETWEEN;
        }
      } break;
      case TOK_OBRACE: {
        /* Single character */
        tokfin = 1;
        proctok = TOK_INBETWEEN;
      } break;
      case TOK_CBRACE: {
        /* Single character */
        tokfin = 1;
        proctok = TOK_INBETWEEN;
      } break;
      case TOK_COMMENT: {
        /* do nothing because comment goes to EOL */
      } break;
      case TOK_EXP:
      case TOK_OPERATOR: {
        /* Operator handles subtypes */
        if (lopchar == '\0' && isoptype(nxt)) {
        } else if (lopchar == '*' && nxt == '*') {
          proctok = TOK_EXP;
        } else if (lopchar == '/' && nxt == '/') {
        } else if (nxt == '=') {
        } else {
          tokfin = 1;
          proctok = TOK_INBETWEEN;
          ignore = 1;
          cur--;
        }
        lopchar = nxt;
      } break;
      case TOK_COMMOLON: {
        /* Single character */
        tokfin = 1;
        proctok = TOK_INBETWEEN;
      } break;
      case TOK_LCONT: {
        /* Single character */
        tokfin = 1;
        proctok = TOK_INBETWEEN;
      } break;
      case TOK_INBETWEEN: {
        ignore = 1;
        if (nxt == '#') {
          proctok = TOK_COMMENT;
        } else if (nxt == '"' || nxt == '\'') {
          string_starter = nxt;
          proctok = TOK_STRING;
          ignore = 0;
          nstrescps = 0;
          nstrleads = 1;
        } else if (isoptype(nxt)) {
          lopchar = '\0';
          proctok = TOK_OPERATOR;
          cur--;
        } else if (nxt == ',' || nxt == ':') {
          proctok = TOK_COMMOLON;
          cur--;
        } else if (nxt == '(' || nxt == '[' || nxt == '{') {
          proctok = TOK_OBRACE;
          cur--;
        } else if (nxt == ')' || nxt == ']' || nxt == '}') {
          proctok = TOK_CBRACE;
          cur--;
        } else if (nxt == '\\') {
          proctok = TOK_LCONT;
          cur--;
        } else if (isalpha_lead(nxt)) {
          proctok = TOK_LABEL;
          cur--;
        } else if (isnumeric_lead(nxt)) {
          numlen = 0;
          proctok = TOK_NUMBER;
          cur--;
        }
      } break;
      }

      if (!ignore) {
        *tokd = *cur;
        tokd++;
      }

      if (cur == eolpos) {
        tokfin = 1;
      }

      if (tokfin) {
        *tokd = '\0';
        ++tokd;
        toks[ntoks] = otok;
        ntoks++;
      }
    }
    *tokd = '\0';

    /* determine the next line shall continue this one */
    ltok = toks[ntoks - 1];
    int ptok = ntoks > 2 ? toks[ntoks - 2] : TOK_INBETWEEN;
    if (ltok == TOK_LCONT || (ptok == TOK_LCONT && ltok == TOK_INBETWEEN) ||
        ltok == TOK_TRISTR) {
      is_continuation = 1;
    } else {
      is_continuation = 0;
    }

    if (!is_continuation || neof) {
      /* Introduce spaces to list */
      char lsp[BUFSIZE];
      memset(lsp, ' ', leading_spaces);
      lsp[leading_spaces] = '\0';
      //      fprintf(stderr, ">>%s\n", linebuf);
      //      fprintf(stderr, "<<%s%s\n", lsp, tokbuf);
      //      fprintf(stderr, "%d", lbufstart - linebuf);
      //      for (int i = 0; i < ntoks; i++) {
      //        fprintf(stderr, " %s", tok_to_string(toks[i]));
      //      }
      //      fprintf(stderr, "\n");

      /* Line wrapping & printing, oh joy */
      char *tokpos = tokbuf;
      fprintf(out, "%s", lsp);
      for (int i = 0; i < ntoks - 1; i++) {
        int pretok = toks[i];
        int postok = toks[i + 1];
        int toklen = strlen(tokpos);

        if (pretok == TOK_LCONT || pretok == TOK_INBETWEEN) {
          /* ignore line breaks */
        } else if (pretok == TOK_COMMENT) {
          char *eos = tokpos + toklen;
          char *sos = tokpos;
          while (*sos == ' ') {
            sos++;
          }
          while (*eos == ' ') {
            *eos = '\0';
            eos--;
          }
          fprintf(out, "# %s", sos);
        } else {
          fprintf(out, "%s", tokpos);
        }
        tokpos += toklen + 1;

        int space;
        if (pretok == TOK_LCONT) {
          space = 0;
        } else if (pretok == TOK_TRISTR && postok == TOK_TRISTR) {
          space = 0;
        } else if (pretok == TOK_EXP || postok == TOK_EXP) {
          space = 0;
        } else if (pretok == TOK_DOT || postok == TOK_DOT) {
          space = 0;
        } else if (postok == TOK_COMMOLON) {
          space = 0;
        } else if (pretok == TOK_COMMOLON) {
          space = 1;
        } else if (pretok == TOK_CBRACE && postok == TOK_LABEL) {
          space = 1;
        } else if (pretok == TOK_OPERATOR || postok == TOK_OPERATOR) {
          space = 1;
        } else if (pretok == TOK_OBRACE || postok == TOK_CBRACE ||
                   pretok == TOK_CBRACE || postok == TOK_OBRACE) {
          space = 0;
        } else {
          space = 1;
        }
        if (space) {
          fprintf(out, " ");
        }
      }
      if (toks[ntoks - 1] == TOK_LCONT) {
        /* ignore line breaks */
      } else if (toks[ntoks - 1] == TOK_COMMENT) {
        char *eos = tokpos + strlen(tokpos);
        char *sos = tokpos;
        while (*sos == ' ') {
          sos++;
        }
        while (*eos == ' ') {
          *eos = '\0';
          eos--;
        }
        if (sos[0] == '!') {
          fprintf(out, "#%s\n", sos);
        } else {
          fprintf(out, "# %s\n", sos);
        }
      } else {
        fprintf(out, "%s\n", tokpos);
      }
    }
    /* Append to prior buffer or not */
    if (!is_continuation) {
      lbufstart = linebuf;
    } else {
      lbufstart += llen - 0;
    }
  }
}

int main(int argc, char **argv) {

  if (argc == 1) {
    fprintf(stderr, "Usage: ./pfa [files]\n");
  }
  for (int i = 1; i < argc; i++) {
    const char *name = argv[i];
    FILE *in = fopen(name, "r");
    //    FILE *out = fopen("out.py", "w");
    FILE *out = stdout;
    if (!in) {
      fprintf(stderr, "File %s dne\n", name);
      return 1;
    }
    pyformat(in, out);

    // Evtly, mv/swap the output file with the original on completion
  }
}
