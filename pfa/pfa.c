#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

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
 *   TOK_COLON is: :
 */
enum {
  TOK_LABEL,
  TOK_SPECIAL,
  TOK_NUMBER,
  TOK_STRING,
  TOK_TRISTR,
  TOK_OBRACE,
  TOK_CBRACE,
  TOK_COMMENT,
  TOK_EQUAL,
  TOK_OPERATOR,
  TOK_COMMA,
  TOK_COLON,
  TOK_EXP,
  TOK_INBETWEEN,
  TOK_LCONT,
  TOK_DOT,
  TOK_UNARYOP,
};

enum {
  LINE_IS_BLANK,
  LINE_IS_CONTINUATION,
  LINE_IS_TRISTR,
  LINE_IS_NORMAL,
};

struct vlbuf {
  union {
    void *vd;
    char *ch;
    int *in;
  } d;
  size_t len;
  size_t esize;
};

struct vlbuf vlbuf_make(size_t es) {
  struct vlbuf ib;
  ib.esize = es;
  ib.len = 16;
  ib.d.vd = malloc(ib.len * ib.esize);
  return ib;
}

static size_t vlbuf_expand(struct vlbuf *ib, size_t minsize) {
  do {
    ib->len *= 2;
  } while (ib->len <= minsize);
  ib->d.vd = realloc(ib->d.vd, ib->len * ib->esize);
  return ib->len;
}

static size_t vlbuf_append(struct vlbuf *ib, const char *str, size_t countedlen,
                           FILE *out) {
  int l = strlen(str);
  if (ib) {
    if (ib->len <= l + countedlen + 1) {
      vlbuf_expand(ib, l + countedlen + 1);
    }
    memcpy(&ib->d.ch[countedlen], str, l + 1);
  }
  if (out) {
    fputs(str, out);
  }
  return l + countedlen;
}

static void vlbuf_free(struct vlbuf *ib) {
  free(ib->d.vd);
  ib->d.vd = 0;
  ib->len = 0;
}

static size_t strapp(char *target, const char *app) {
  size_t delta = 0;
  while (*app != '\0') {
    target[delta] = *app;
    delta++;
    app++;
  }
  return delta;
}

static const char *tok_to_string(int tok) {
  switch (tok) {
  case TOK_LABEL:
    return "LAB";
  case TOK_SPECIAL:
    return "SPC";
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
  case TOK_EQUAL:
    return "EQL";
  case TOK_EXP:
    return "EXP";
  case TOK_COLON:
    return "CLN";
  case TOK_COMMA:
    return "CMA";
  case TOK_INBETWEEN:
    return "INB";
  case TOK_LCONT:
    return "LCO";
  case TOK_DOT:
    return "DOT";
  case TOK_UNARYOP:
    return "UNO";
  default:
    return "???";
  }
}

static const char *ls_to_string(int ls) {
  switch (ls) {
  case LINE_IS_BLANK:
    return "LINE_BLNK";
  case LINE_IS_CONTINUATION:
    return "LINE_CONT";
  case LINE_IS_NORMAL:
    return "LINE_NORM";
  case LINE_IS_TRISTR:
    return "LINE_TSTR";
  default:
    return "LINE_????";
  }
}

static int isalpha_lead(char c) {
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
static int isnumeric_lead(char c) {
  if ('0' <= c && c <= '9')
    return 1;
  if ('.' == c)
    return 1;
  return 0;
}

static int isoptype(char c) {
  if (c == '=' || c == '+' || c == '-' || c == '@' || c == '|' || c == '^' ||
      c == '&' || c == '*' || c == '/' || c == '<' || c == '>' || c == '!' ||
      c == '~' || c == '%')
    return 1;
  return 0;
}

/* import keyword; print(*keyword.kwlist) */
static const char *specnames[] = {
    "and",    "as",    "assert", "break",  "class",   "continue", "def",
    "del",    "elif",  "else",   "except", "finally", "for",      "from",
    "global", "if",    "import", "in",     "is",      "lambda",   "nonlocal",
    "not",    "or",    "pass",   "raise",  "return",  "try",      "while",
    "with",   "yield", NULL};

static int *spectable = NULL;
static int *terminal = NULL;
static void make_special_name_table() {
  /* string tree uses least memory; this is simpler to debug */
  int ncodes = 0;
  int nstates = 0;
  for (int i = 0; specnames[i]; i++) {
    nstates += strlen(specnames[i]) + 1;
    ncodes++;
  }
  spectable = (int *)malloc(sizeof(int) * 26 * nstates);
  terminal = (int *)malloc(sizeof(int) * nstates);
  /* by default all paths lead one to failure */
  for (int i = 0; i < 26 * nstates; i++) {
    spectable[i] = -1;
  }
  for (int i = 0; i < nstates; i++) {
    terminal[i] = 0;
  }
  /* fill in table, reusing old paths if available */
  int gstate = 1;
  for (int i = 0; i < ncodes; i++) {
    const char *s = specnames[i];
    int cstate = 0;
    for (int k = 0; s[k]; k++) {
      int cc = s[k] - 'a';
      if (spectable[26 * cstate + cc] == -1) {
        spectable[26 * cstate + cc] = gstate;
        cstate = gstate;
        gstate++;
      } else {
        cstate = spectable[26 * cstate + cc];
      }
    }
    terminal[cstate] = 1;
  }
}
static void free_special_name_table() {
  free(spectable);
  free(terminal);
}
static int is_special_name(const char *tst) {
  int fcode = 0;
  for (int k = 0; tst[k]; k++) {
    if (tst[k] < 'a' || tst[k] > 'z') {
      return 0;
    }
    fcode = spectable[26 * fcode + (tst[k] - 'a')];
    if (fcode == -1) {
      return 0;
    }
  }
  return terminal[fcode];
}

static void pyformat(FILE *file, FILE *out, struct vlbuf *origfile,
                     struct vlbuf *formfile) {
  struct vlbuf linebuf = vlbuf_make(sizeof(char));
  struct vlbuf tokbuf = vlbuf_make(sizeof(char));
  struct vlbuf toks = vlbuf_make(sizeof(int));
  struct vlbuf lsp = vlbuf_make(sizeof(char));
  struct vlbuf laccum = vlbuf_make(sizeof(char));
  struct vlbuf splitpoints = vlbuf_make(sizeof(int));
  struct vlbuf split_ratings = vlbuf_make(sizeof(int));
  struct vlbuf split_nestings = vlbuf_make(sizeof(int));
  struct vlbuf lineout = vlbuf_make(sizeof(char));

  char *tokd = NULL;
  char *stokd = NULL;
  int ntoks = 0;

  char string_starter = '\0';
  int line_state = LINE_IS_NORMAL;
  int leading_spaces = 0;
  int neof = 0;
  int nestings = 0;
  int netlen = 0;
  int origfilelen = 0;
  int formfilelen = 0;
  while (1) {
    char *readct;
    int llen = 0;
    while (1) {
      readct = fgets(&linebuf.d.ch[llen], linebuf.len - 3 - llen, file);
      if (!readct)
        break;
      int rlen = strlen(readct);
      if (origfile) {
        if (origfile->len < rlen + origfilelen)
          vlbuf_expand(origfile, rlen + origfilelen);
        memcpy(&origfile->d.ch[origfilelen], &linebuf.d.ch[llen], rlen + 1);
        origfilelen += rlen;
      }
      llen += rlen;
      if (linebuf.d.ch[llen - 1] != '\n') {
        vlbuf_expand(&linebuf, llen + 3);
      } else {
        break;
      }
    }

    if (!readct) {
      if ((line_state == LINE_IS_NORMAL || line_state == LINE_IS_BLANK) ||
          neof) {
        break;
      } else {
        neof = 1;
      }
    }

    if (line_state == LINE_IS_NORMAL || line_state == LINE_IS_BLANK) {
      netlen = llen;
      ntoks = 0;
      /* Ensure buffers can hold the worst case line */
      if (tokbuf.len < netlen * 2) {
        vlbuf_expand(&tokbuf, netlen * 2);
        vlbuf_expand(&toks, netlen * 2);
      }
      tokd = tokbuf.d.ch;
      stokd = tokbuf.d.ch;
      nestings = 0;
    } else {
      /* Adjust buffers in the case of line extension */
      netlen += llen;
      int tokdoff = tokd - tokbuf.d.ch;
      int stokdoff = stokd - tokbuf.d.ch;
      if (tokbuf.len < netlen * 2) {
        vlbuf_expand(&tokbuf, netlen * 2);
        vlbuf_expand(&toks, netlen * 2);
      }
      tokd = &tokbuf.d.ch[tokdoff];
      stokd = &tokbuf.d.ch[stokdoff];
    }

    /* token-split the line with NULL characters; double NULL is eof */

    /* STATE MACHINE TOKENIZE */
    char *cur = linebuf.d.ch;
    /* space char gives room for termination checks */
    if (llen > 0)
      cur[llen - 1] = '\n';
    cur[llen] = '\0';
    cur[llen + 1] = '\0';

    int is_whitespace = 1;
    for (char *c = cur; *c != '\n'; ++c)
      if (*c != ' ' && *c != '\t')
        is_whitespace = 0;

    int dumprest = 0;
    if (line_state == LINE_IS_TRISTR) {
      /* tristrings are unaffected by blank lines */
    } else if (is_whitespace) {
      if (line_state == LINE_IS_CONTINUATION) {
        line_state = LINE_IS_BLANK;
        dumprest = 1;
      } else if (line_state == LINE_IS_BLANK) {
        continue;
      } else {
        line_state = LINE_IS_BLANK;
      }
    } else if (line_state == LINE_IS_BLANK) {
      line_state = LINE_IS_NORMAL;
    }

    if (line_state == LINE_IS_NORMAL) {
      leading_spaces = 0;
      for (; cur[0] == '\n' || cur[0] == ' ' || cur[0] == '\t'; cur++) {
        leading_spaces++;
      }
    } else {
    }

    int proctok = TOK_INBETWEEN;
    if (line_state == LINE_IS_TRISTR) {
      proctok = TOK_TRISTR;
      --ntoks;
      --tokd;
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
      int inside_string = proctok == TOK_STRING || proctok == TOK_TRISTR;
      if (!inside_string && cur[0] == ' ' && cur[1] == ' ') {
        continue;
      }
      /* SO: single space is a token boundary ... */
      char nxt = *cur;
      int ignore = 0;
      int tokfin = 0;
      int otok = proctok;
      switch (proctok) {
      case TOK_SPECIAL:
      case TOK_LABEL: {
        if (isalpha_lead(nxt) || ('0' <= nxt && nxt <= '9')) {

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
        int isdot = (numlen == 1) && cur[-1] == '.' && !isnumeric_lead(nxt);
        if (!isdot && isnumeric_lead(nxt)) {
        } else if (cur[-1] == 'e' && (nxt == '-' || nxt == '+')) {
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
      case TOK_EQUAL:
      case TOK_EXP:
      case TOK_UNARYOP:
      case TOK_OPERATOR: {
        /* Operator handles subtypes */
        if (lopchar == '\0' && isoptype(nxt)) {
        } else if (lopchar == '*' && nxt == '*') {
          proctok = TOK_EXP;
        } else if (lopchar == '/' && nxt == '/') {
        } else if (nxt == '=') {
          if (proctok == TOK_EXP) {
            proctok = TOK_OPERATOR;
          }
        } else {
          if (proctok != TOK_EXP) {
            if (lopchar == '-' || lopchar == '+' || lopchar == '*') {
              otok = TOK_UNARYOP;
            }
            if (lopchar == '=' && !isoptype(cur[-2])) {
              otok = TOK_EQUAL;
            }
          }
          tokfin = 1;
          proctok = TOK_INBETWEEN;
          ignore = 1;
          cur--;
        }
        lopchar = nxt;
      } break;
      case TOK_COMMA: {
        /* Single character */
        tokfin = 1;
        proctok = TOK_INBETWEEN;
      } break;
      case TOK_COLON: {
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
          /* nix the terminating newline */
          linebuf.d.ch[llen - 1] = ' ';
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
        } else if (nxt == ',') {
          proctok = TOK_COMMA;
          cur--;
        } else if (nxt == ':') {
          proctok = TOK_COLON;
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

      if (cur == eolpos && otok != TOK_INBETWEEN) {
        tokfin = 1;
      }

      if (tokfin) {
        *tokd = '\0';
        ++tokd;
        /* convert label to special if it's a word in a list we have */
        if (otok == TOK_LABEL && is_special_name(stokd)) {
          otok = TOK_SPECIAL;
        }
        if (otok == TOK_OBRACE) {
          nestings++;
        } else if (otok == TOK_CBRACE) {
          nestings--;
        }
        toks.d.in[ntoks] = otok;
        stokd = tokd;
        ntoks++;
      }
    }
    *tokd = '\0';

    /* determine if the next line shall continue this one */
    if (line_state == LINE_IS_BLANK) {
      line_state = LINE_IS_BLANK;
    } else if (proctok == TOK_TRISTR) {
      line_state = LINE_IS_TRISTR;
    } else if ((ntoks > 0 && toks.d.in[ntoks - 1] == TOK_LCONT) ||
               nestings > 0) {
      line_state = LINE_IS_CONTINUATION;
    } else {
      line_state = LINE_IS_NORMAL;
    }

    if (line_state == LINE_IS_BLANK && !dumprest) {
      formfilelen = vlbuf_append(formfile, "\n", formfilelen, out);
    } else if (line_state == LINE_IS_NORMAL || neof || dumprest) {
      /* Introduce spaces to list */

      /* split ratings 0 is regular; -1 is force/cmt; 1 is weak */
      if (lsp.len < leading_spaces + 1)
        vlbuf_expand(&lsp, leading_spaces + 1);
      memset(lsp.d.vd, ' ', leading_spaces);
      lsp.d.ch[leading_spaces] = '\0';

      if (laccum.len < 2 * netlen) {
        vlbuf_expand(&laccum, 2 * netlen);
        vlbuf_expand(&splitpoints, 2 * netlen);
        vlbuf_expand(&split_ratings, 2 * netlen);
        vlbuf_expand(&split_nestings, 2 * netlen);
        vlbuf_expand(&lineout, 2 * netlen);
        vlbuf_expand(&laccum, 2 * netlen);
      }

      int nsplits = 0;
      char *buildpt = laccum.d.ch;

      /* Line wrapping & printing, oh joy */
      char *tokpos = tokbuf.d.ch;
      int nests = 0;
      for (int i = 0; i < ntoks; i++) {
        int pptok = i > 0 ? toks.d.in[i - 1] : TOK_INBETWEEN;
        int pretok = toks.d.in[i];
        int postok = toks.d.in[i + 1];
        int toklen = strlen(tokpos);

        if (pretok == TOK_LCONT) {
          /* ignore line breaks */
        } else if (pretok == TOK_COMMENT) {
          char *eos = tokpos + toklen - 1;
          char *sos = tokpos;
          while (*sos == ' ') {
            sos++;
          }
          while (*eos == ' ') {
            *eos = '\0';
            eos--;
          }
          if (sos[0] == '!') {
            buildpt += strapp(buildpt, "#");
          } else {
            buildpt += strapp(buildpt, "# ");
          }
          buildpt += strapp(buildpt, sos);
          splitpoints.d.in[nsplits] = buildpt - laccum.d.ch;
          split_ratings.d.in[nsplits] = -1;
          split_nestings.d.in[nsplits] = nests;
          nsplits++;
        } else {
          buildpt += strapp(buildpt, tokpos);
          splitpoints.d.in[nsplits] = buildpt - laccum.d.ch;
          split_ratings.d.in[nsplits] = 0;
          split_nestings.d.in[nsplits] = nests;
          nsplits++;
        }
        tokpos += toklen + 1;

        int space;
        if (pretok == TOK_COMMENT) {
          space = 0;
        } else if (pptok == TOK_INBETWEEN && pretok == TOK_OPERATOR &&
                   postok == TOK_LABEL) {
          /* annotation */
          space = 0;
        } else if (pretok == TOK_LCONT) {
          space = 0;
        } else if (pretok == TOK_EQUAL || postok == TOK_EQUAL) {
          space = nests == 0;
        } else if (pretok == TOK_SPECIAL) {
          if (postok == TOK_COLON) {
            space = 0;
          } else {
            space = 1;
          }
        } else if (postok == TOK_SPECIAL) {
          space = 1;
        } else if (pretok == TOK_TRISTR && postok == TOK_TRISTR) {
          space = 0;
        } else if (pretok == TOK_EXP || postok == TOK_EXP) {
          space = 0;
        } else if (pretok == TOK_DOT || postok == TOK_DOT) {
          space = 0;
        } else if (pretok == TOK_OPERATOR && postok == TOK_UNARYOP) {
          space = 1;
        } else if (pretok == TOK_LABEL && postok == TOK_UNARYOP) {
          space = 1;
        } else if (pretok == TOK_CBRACE && postok == TOK_UNARYOP) {
          space = 1;
        } else if (pretok == TOK_OBRACE && postok == TOK_UNARYOP) {
          space = 0;
        } else if (pretok == TOK_UNARYOP) {
          if (pptok == TOK_OPERATOR || pptok == TOK_EXP || pptok == TOK_COMMA ||
              pptok == TOK_OBRACE || pptok == TOK_EQUAL || pptok == TOK_COLON) {
            space = 0;
          } else {
            space = 1;
          }
        } else if (postok == TOK_COMMA || postok == TOK_COLON) {
          space = 0;
        } else if (pretok == TOK_COMMA) {
          if (postok == TOK_CBRACE) {
            space = 0;
          } else {
            space = 1;
          }
        } else if (pretok == TOK_COLON) {
          if (pptok == TOK_LABEL || pptok == TOK_SPECIAL) {
            space = 1;
          } else {
            space = 0;
          }
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
        if (space && i < ntoks - 1) {
          buildpt += strapp(buildpt, " ");
        }

        if (pretok == TOK_OBRACE) {
          nests++;
        } else if (postok == TOK_CBRACE) {
          nests--;
        }
      }
      int eoff = buildpt - laccum.d.ch;

      /* the art of line breaking */
      int length_left = 80 - leading_spaces;
      formfilelen = vlbuf_append(formfile, lsp.d.ch, formfilelen, out);

      if (nsplits > 0) {
        for (int i = 0; i < nsplits; i++) {
          int fr = i > 0 ? splitpoints.d.in[i - 1] : 0;
          int to = i >= nsplits - 1 ? eoff : splitpoints.d.in[i];
          memcpy(lineout.d.ch, &laccum.d.ch[fr], to - fr);
          lineout.d.ch[to - fr] = '\0';
          int nlen = to - fr;
          int force_split = i > 0 ? split_ratings.d.in[i - 1] < 0 : 0;

          /* The previous location provides the break-off score */
          int bscore = 0, bk = -1;
          for (int rleft = length_left, k = i; k < nsplits && rleft >= 0; k++) {
            /* Estimate segment length */
            int fr = k > 0 ? splitpoints.d.in[k - 1] : 0;
            int ofr = k > 1 ? splitpoints.d.in[k - 2] : 0;
            int to = k >= nsplits - 1 ? eoff : splitpoints.d.in[k];
            int len = to - fr;
            int isc = 100;
            /* Just by previous. Q: split tok? or 'saved length' field, w/
             * comments counting as -inf ls. Issue: what if there's less than N
             * fields left ?*/
            if (laccum.d.ch[ofr] == ',') {
              isc = rleft - len - 15;
            } else if (laccum.d.ch[ofr] == ':') {
              isc = rleft - len - 15;
            } else {
              isc = rleft - len;
            }
            rleft -= len;
            if (isc < bscore) {
              bscore = isc;
              bk = k;
            }
            /* Never hold up a terminator */
            if (rleft >= 0 && k == nsplits - 1) {
              bk = -1;
            }
          }
          int want_split = bk == i;
          int length_split = nlen >= length_left;

          int continuing = 1;
          if (i == 0) {
            continuing = 1;
          } else if (force_split || length_split || want_split) {
            continuing = 0;
          } else {
            continuing = 1;
          }

          if (continuing) {
            length_left -= nlen;
            formfilelen =
                vlbuf_append(formfile, lineout.d.ch, formfilelen, out);
          } else {
            char *prn = &lineout.d.ch[0];
            if (lineout.d.ch[0] == ' ') {
              prn = &lineout.d.ch[1];
              nlen -= 1;
            }
            if (force_split || split_nestings.d.in[i - 1] > 0) {
              formfilelen = vlbuf_append(formfile, "\n    ", formfilelen, out);
            } else {
              formfilelen =
                  vlbuf_append(formfile, " \\\n    ", formfilelen, out);
            }
            length_left = 80 - leading_spaces - 4 - nlen;
            formfilelen = vlbuf_append(formfile, lsp.d.ch, formfilelen, out);
            formfilelen = vlbuf_append(formfile, prn, formfilelen, out);
          }
        }
        formfilelen = vlbuf_append(formfile, "\n", formfilelen, out);
      } else {
        formfilelen = vlbuf_append(formfile, laccum.d.ch, formfilelen, out);
        formfilelen = vlbuf_append(formfile, "\n", formfilelen, out);
      }
      if (line_state == LINE_IS_BLANK) {
        formfilelen = vlbuf_append(formfile, "\n", formfilelen, out);
      } else {
        line_state = LINE_IS_NORMAL;
      }
    }
  }

  vlbuf_free(&linebuf);
  vlbuf_free(&tokbuf);
  vlbuf_free(&toks);
  vlbuf_free(&lsp);
  vlbuf_free(&laccum);
  vlbuf_free(&splitpoints);
  vlbuf_free(&split_ratings);
  vlbuf_free(&split_nestings);
  vlbuf_free(&lineout);
}

int main(int argc, char **argv) {
  (void)ls_to_string;
  (void)tok_to_string;

  int inplace = 0;
  if (argv[0][strlen(argv[0]) - 1] == 'i') {
    inplace = 1;
  }

  if (argc == 1) {
    if (inplace) {
      fprintf(stderr, "Usage: pfai [files]\n");
      fprintf(stderr, "       (to stdout) pfa [files]\n");
    } else {
      fprintf(stderr, "Usage: pfa [files]\n");
      fprintf(stderr, "       (in place)  pfai [files]\n");
    }
    return 1;
  }

  make_special_name_table();
  struct vlbuf origfile = vlbuf_make(sizeof(char));
  struct vlbuf formfile = vlbuf_make(sizeof(char));
  int maxnlen = 0;
  for (int i = 1; i < argc; i++) {
    int l = strlen(argv[i]);
    if (l > maxnlen)
      maxnlen = l;
  }
  char *nbuf = (char *)malloc(sizeof(char) * (maxnlen + 12));

  for (int i = 1; i < argc; i++) {
    const char *name = argv[i];
    FILE *in = fopen(name, "r");
    if (!in) {
      fprintf(stderr, "File %s dne\n", name);
      return 1;
    }
    /* Format file contents, saving to stdout or to buffers */
    pyformat(in, inplace ? 0 : stdout, inplace ? &origfile : 0,
             inplace ? &formfile : 0);
    fclose(in);

    if (inplace) {
      int unchanged = strcmp(origfile.d.ch, formfile.d.ch) == 0;
      if (unchanged) {
        /* Do nothing */
      } else {
        /* Construct the temporary name */
        int l = strlen(name);
        strncpy(nbuf, name, l + 1);
        int co = 0;
        for (int j = l - 1; j >= 0; j--)
          if (name[j] == '/') {
            co = j + 1;
            break;
          }
        strncpy(&nbuf[co], ".pfa_XXXXXX", 12);

        /* Write to temporary */
        int fo = mkstemp(nbuf);
        FILE *out = fdopen(fo, "w");
        fwrite(formfile.d.ch, 1, strlen(formfile.d.ch), out);
        fclose(out);

        /* Ensure properties match */
        struct stat st;
        if (stat(name, &st) < 0) {
          fprintf(stderr, "Could not get original permissions for %s\n", name);
        } else {
          chmod(nbuf, st.st_mode);
          chown(nbuf, st.st_uid, st.st_gid);
        }

        int s = rename(nbuf, name);
        if (s) {
          fprintf(stderr, "Failed to overwrite %s with %s\n", name, nbuf);
          remove(nbuf);
        }
      }
    }
  }
  vlbuf_free(&origfile);
  vlbuf_free(&formfile);
  free(nbuf);
  free_special_name_table();
}
