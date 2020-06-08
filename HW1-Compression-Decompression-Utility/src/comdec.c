#include "const.h"
#include "sequitur.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 *
 * IMPORTANT: You MAY NOT use floating point arithmetic or declare
 * any "float" or "double" variables.  IF YOU VIOLATE THIS RESTRICTION,
 * YOU WILL GET A ZERO!
 */

int write_utf(FILE* out, int c) {
    if(c == 0x81 || c == 0x82 || c == 0x83 || c == 0x84 || c == 0x85) {
        fputc(c, out);
        return 1;
    }
    else if(c & 0x1F0000) {
        fputc((0xF0 | ((c & 0x1C0000) >> 18)), out);
        fputc((0x80 | ((c & 0x3F000) >> 12)), out);
        fputc((0x80 | ((c & 0xFC0) >> 6)), out);
        fputc((0x80 | ((c & 0x3F))), out);
        return 4;
    }
    else if(c & 0xF800) {
        fputc((0xE0 | ((c & 0xF000) >> 12)), out);
        fputc((0x80 | ((c & 0xFC0) >> 6)), out);
        fputc((0x80 | ((c & 0x3F))), out);
        return 3;
    }
    else if(c & 0x780) {
        fputc((0xC0 | ((c & 0x7C0) >> 6)), out);
        fputc((0x80 | ((c & 0x3F))), out);
        return 2;
    }
    else {
        fputc(c, out);
        return 1;
    }
}

int get_utf(FILE *in) {
    int c = fgetc(in);
    if( c != EOF) {
        if(c == 0x81 || c == 0x82 || c == 0x83 || c == 0x84 || c == 0x85) {
            return c;
        }
        if((c & 0xE0) == 0xC0){
            c = (c & 0x1F) << 6 | (fgetc(in) & 0x3F);
        }
        else if((c & 0xF0) == 0xE0){
            c = c & 0xF;
            c = c << 6;
            c += (fgetc(in) & 0x3F);
            c = c << 6;
            c += (fgetc(in) & 0x3F);
        }
        else if((c & 0xF8) == 0xF0){
            c = (c & 0x7);
            c = c << 6;
            c += (fgetc(in) & 0x3F);
            c = c << 6;
            c += (fgetc(in) & 0x3F);
            c = c << 6;
            c += (fgetc(in) & 0x3F);
        }
        return c;
    }

    return EOF;
}

/**
 * Main compression function.
 * Reads a sequence of bytes from a specified input stream, segments the
 * input data into blocks of a specified maximum number of bytes,
 * uses the Sequitur algorithm to compress each block of input to a list
 * of rules, and outputs the resulting compressed data transmission to a
 * specified output stream in the format detailed in the header files and
 * assignment handout.  The output stream is flushed once the transmission
 * is complete.
 *
 * The maximum number of bytes of uncompressed data represented by each
 * block of the compressed transmission is limited to the specified value
 * "bsize".  Each compressed block except for the last one represents exactly
 * "bsize" bytes of uncompressed data and the last compressed block represents
 * at most "bsize" bytes.
 *
 * @param in  The stream from which input is to be read.
 * @param out  The stream to which the block is to be written.
 * @param bsize  The maximum number of bytes read per block.
 * @return  The number of bytes written, in case of success,
 * otherwise EOF.
 */
int compress(FILE *in, FILE *out, int bsize) {
    if(in == NULL || out == NULL) {
        return EOF;
    }
    int b = fgetc(in);
    int count = 2;
    fputc(0x81, out);               //Write SOT
    while(b != EOF) {
        init_symbols();
        init_rules();
        init_digram_hash();
        SYMBOL *mr = new_rule(next_nonterminal_value++);   //Initialize main rule
        add_rule(mr);
        for(int i = 0; (i < bsize && b != EOF); i++) {
            SYMBOL* s = new_symbol(b, NULL);
            insert_after(main_rule->prev, s);
            check_digram(main_rule->prev->prev);
            b = fgetc(in);
        }
        //This is where we start writing the blocks.
        fputc(0x83, out);        //Write SOB
        //fprintf(stderr, "%s\n", "Wrote SOB");
        count++;
        SYMBOL *mrp = main_rule; //main rule pointer = main_rule. This will get heads of rules.
        do{
            SYMBOL *rp = mrp;   //rule pointer = current rule. This will get the rule body.
            do {
                count += write_utf(out, rp->value);  //Write the value of symbol to file.
                rp = rp->next;
            } while(rp != mrp);
            mrp = mrp->nextr;
            if(mrp != main_rule) {
                //fprintf(stderr, "%s\n", "Wrote an entire rule.");
                fputc(0x85, out);
                count++;
            }

        } while(mrp != main_rule);
        fputc(0x84, out);
        //fprintf(stderr, "%s\n", "End of block.");
        count++;
        // if(b == EOF) {
        //     break;
        // }


    }
    fputc(0x82, out);
    fflush(out);
    //fprintf(stderr, "%s\n", "End of transmission");
    debug("compress return %d", count);
    return count;
}

int expand(SYMBOL* rule, FILE *out) {
    //fprintf(stderr, "%s\n", "IN EXPAND FUNCTION");
    SYMBOL* rulep = rule->next;
    //fprintf(stderr, "%s\n", "AFTER GETTING RULE");
    int x = 0;
    while(rulep != rule) {
        if(rulep->rule == NULL) {
            fputc(rulep->value, out);
            x++;
        }
        else {
            x = x + expand(*(rule_map + (rulep->value)), out);
        }
        rulep = rulep->next;
    }
    return x;
}

/**
 * Main decompression function.
 * Reads a compressed data transmission from an input stream, expands it,
 * and and writes the resulting decompressed data to an output stream.
 * The output stream is flushed once writing is complete.
 *
 * @param in  The stream from which the compressed block is to be read.
 * @param out  The stream to which the uncompressed data is to be written.
 * @return  The number of bytes written, in case of success, otherwise EOF.
 */
int decompress(FILE *in, FILE *out) {
    if(in == NULL || out == NULL) {
        return EOF;
    }
    int sob = 0x83;
    int rd = 0x85;
    int eob = 0x84;
    int sot = 0x81;
    int eot = 0x82;
    int count = 0;
    int b = get_utf(in);
    if(b != sot) {
        //fprintf(stderr, "%s\n", "error: does not start with SOT");
        return EOF;
    }
    b = get_utf(in);        //We are inside of SOT.
    while(b != eot && b != EOF) {
        if(b == sob) {     //Get the SOB
            //fprintf(stderr, "%s\n", "SOB");
            init_symbols();
            init_rules();
            //fprintf(stderr, "%s\n", "INITIALIZED RULES");
            b = get_utf(in);    //b = head of rule.
            if(b == eob) {
                b = get_utf(in);
                continue;
            }
            while(b != eob) {
                if(b == rd || b == sob || b == sot || b == eot || b == EOF) {
                    fprintf(stderr, "%s\n", "error: invalid start of head: we have eot, sot, rd, sob, eof");
                    return EOF;
                }
                //fprintf(stderr, "%s %x\n", "Have a valid new head for a new rule.", b);
                SYMBOL* nr = new_rule(b);
                if(nr == NULL) {
                     fprintf(stderr, "%s\n", "RULE IS NULL, LESS THAN 1st NONTERMINAL");
                }
                b = get_utf(in);            //b is the first symbol of body now.
                if(b == rd || b == sob || b == sot || b == eot || b == EOF || b == eob) {
                    fprintf(stderr, "%s\n", "error: invalid start to rule body.");
                    return EOF;
                }
                while(b != rd) {      //Loop to define this rule's body.
                    if(b == EOF || b == eot || b == sot || b == sob) {
                        fprintf(stderr, "%s\n", "error eot/eof/sot/sob found in a rule body.");
                        return EOF;
                    }
                    else if(b == eob) {
                        //fprintf(stderr, "%s\n", "Found the end of block.");
                        break;
                    }
                    SYMBOL* rb = new_symbol(b, rb);// Creating a new symbol representing pieces rule body.
                    rb->next = nr;    //Inserting symbol in rule body's circular link list
                    rb->prev = nr->prev;
                    nr->prev->next = rb;
                    nr->prev = rb;
                    b = get_utf(in);
                }
                add_rule(nr);           //Add the new rule to main_rule.
                //fprintf(stderr, "%s\n", "ADDED RULe");
                *(rule_map + (nr->value)) = nr;  //Store the rule in the rule_map based on head's value.
                if(b == eob) {
                    //fprintf(stderr, "%s\n", "HAVE EOB");
                    break;
                }
                b = get_utf(in);
            }
            //So now we have an EOB
            count += expand(main_rule, out);
            //fprintf(stderr, "%s\n", "EXPANDED RULE AND WRITED");
        }
        b = get_utf(in);    //Get next utf, it should be either sob or eot.
    }
    fflush(out);
    if(b != 0x82) {
        fprintf(stderr, "%s\n", "error: does not end with EOT");
        return EOF;
    }
    return count;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the selected program options will be set in the
 * global variable "global_options", where they will be accessible
 * elsewhere in the program.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * Refer to the homework document for the effects of this function on
 * global variables.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected options.
 */
int validargs(int argc, char **argv)
{
    global_options = 0;
    if (argc <= 1) {
        return -1;
    }
    char *flag1;
    flag1 = *(argv + 1);
    if(*(flag1) != '-' || *(flag1 + 2) != 0){
        return -1;
    }
    if(*(flag1 + 1) == 'h'){
        global_options = global_options | 1;
        return 0;
    }
    if(*(flag1 + 1) == 'c'){
        if(argc == 3 || argc > 4) {
            return -1;
        }
        if (argc == 2){
            global_options = global_options | 0x04000000;
            global_options = global_options | 2;
            return 0;
        }
        char *flag2 = *(argv + 2);
        if(*(flag2) != '-' || *(flag2 + 2) != 0){
            return -1;
        }
        if(*(flag2 + 1) == 'b'){
            int blocksize = 0;
            char* flag3 = *(argv + 3);
            int currentChar = *(flag3);
            while(currentChar != 0){
                int num = currentChar - 48;
                if(num > 9 || num < 0) {
                    return -1;
                }
                blocksize *= 10;
                blocksize += num;
                flag3 += 1;
                currentChar = *(flag3);
            }
            if(blocksize <= 1024 && blocksize >= 1){
                global_options = global_options | (blocksize << 16);
                global_options = global_options | 2;
                return 0;
            }
            return -1;
        }
    }
    if(*(flag1 + 1) == 'd') {
        if(argc == 2){
            global_options = global_options | 4;
            //fprintf(stderr, "%s\n", "Decompress command.");
            return 0;
        }
    }
    return -1;
}
