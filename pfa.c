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
 *               TOK_COMMOLON inserts a right space unless followed by TOK_CBRACE
 *               TOK_LABEL has no right gap relative to TOK_OBRACE 
 *               TOK_EXP binds close on the left and on the right
 *               TOK_PM acts like TOK_OPERATOR, except when it binds right
 */
enum {
  TOK_LABEL,
  TOK_NUMBER,
  TOK_STRING,
  TOK_OBRACE,
  TOK_CBRACE,
  TOK_COMMENT,
  TOK_OPERATOR,
  TOK_COMMOLON
};

int main(int argc, char** argv) {
    char linebuf[BUFSIZE];
    
    FILE* file = fopen("test.py", "r");
    int is_continuation = 0;
    int inside_string = 0;
    while (fgets(linebuf, sizeof(linebuf), file)) {
        int llen = strlen(linebuf);
        linebuf[llen-1] = '\0';
        linebuf[llen] = '\0';
        /* token-split the line with NULL characters; double NULL is eof */
        
        /* STATE MACHINE TOKENIZE */
        char* cur = linebuf;
        int leading_spaces = 0;
        for (;cur[0] == ' ' || cur[0] == '\t';cur++) {
           leading_spaces++;
        }
        
        char tokbuf[BUFSIZE];
        char* tokd = tokbuf;
        
        fprintf(stderr, "LSP: %d\n", leading_spaces);
        for (;cur[0];cur++) {
            /* STATE MACHINE GOES HERE */
            if (cur[0] == '\t') {
                cur[0] = ' ';
            }
            if (!inside_string && cur[0] == ' ' && cur[1] == ' ') {
                continue;
            }
            /* SO: single space is a token boundary ... */
            
            
            *tokd = *cur;
            tokd++;
            
            
            /* TODO: count leading space number */
            
          
//             fprintf(stderr, "%d |%c|\n", cur, *cur); 
        }
        *tokd = '\0';
        
        if (!is_continuation) {
          /* Introduce spaces to list */
          char lsp[BUFSIZE];
          memset(lsp, ' ', leading_spaces);
          lsp[leading_spaces] = '\0';
          printf("%s%s\n", lsp, tokbuf);
          
          
          /* Line wrapping & printing, oh joy */

        }
        
        /* test for line continuation; if so, put up a marker and join the lines */
        
        
        
//         printf("%s | %d\n", linebuf, llen); 
        // use strncpy
    }
    
}
