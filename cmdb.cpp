#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include "cmdb.h"
#include "mbed.h"

//DONE Pass Serial into constructor for printf, putc and getc.
//DONE CID_<subsystem> must be handled internally.
//
//TODO ADD Documentation.
//TODO CID_HELP must be handled internally (like all system commands (IDLE/MACRO etc).
//TODO CID_HELP should be function (so we can call it easier in the switche's else branch).
//TODO Link CID_LAST to Vector Size.

//Constructor (see http://www.daniweb.com/forums/thread293338.html)
Cmdb::Cmdb(const Serial serial, const std::vector<cmdb_cmd>& cmds) :  _serial(serial), _cmds(cmds) {
    echo = true;
    bold = true;
    subsystem = -1;

    CID_LAST = _cmds.back().id;
    CMD_TBL_LEN = cmds.size();

    cmdb_init(true);
}

//Public
bool  Cmdb::cmdb_macro_hasnext() {
    return macro_ptr!=-1 && macro_ptr<MAX_CMD_LEN && macro_buf[macro_ptr];
}

char Cmdb::cmdb_macro_next() {
    char ch = macro_buf[macro_ptr++];
    if (macro_ptr==MAX_CMD_LEN) {
        cmdb_macro_reset();
    }
    return ch;
}

char  Cmdb::cmdb_macro_peek() {
    return macro_buf[macro_ptr];
}

void  Cmdb::cmdb_macro_reset() {
    macro_ptr         = -1;
    macro_buf[0]     = '\0';
}

bool  Cmdb::cmdb_hasnext() {
    return _serial.readable()==1;
}

char  Cmdb::cmdb_next() {
    return _serial.getc();
}

//Private Utilities #1

int  Cmdb::cmdb_escid_search(char *escstr) {
    for (int i=0; i<ESC_TBL_LEN; i++) {
        if (strcmp (esc_tbl[i].escstr, escstr) == 0)
            return (esc_tbl[i].id);
    }

    return (EID_LAST);
}

int  Cmdb::cmdb_cmdid_search(char *cmdstr) {
    //Warning, we return the ID but somewhere assume it's equal to the array index!
    for (int i=0; i<CMD_TBL_LEN; i++) {
        if ((stricmp (_cmds[i].cmdstr, cmdstr) == 0) && ((_cmds[i].subs == subsystem) || (_cmds[i].subs<0)))
            return (_cmds[i].id);
    }

    return (CID_LAST);
}

int  Cmdb::cmdb_cmdid_index(int cmdid) {
    for (int i=0; i<CMD_TBL_LEN; i++) {
        if (_cmds[i].id==cmdid)
            return i;
    }

    return -1;
}

int Cmdb::cmdb_parse(char *cmd) {
    //Command
    char cmdstr_buf [1 + MAX_CMD_LEN];

    //Parameters
    char argstr_buf [1 + MAX_CMD_LEN];
    char *argsep;

    char prmstr_buf [1 + MAX_CMD_LEN];                          //copy of sscanf pattern
    char *tok;                                                  //current token
    void *toks[MAX_ARGS];                                       //pointers to string tokens IN commandline (argstr_buf)
    char *prms[MAX_ARGS];                                       //patterns IN copy of sscanf string (*parms)

    char typ = '\0';                                            //Var type
    char mod = '\0';                                            //Var modifier      (for cardinal types)
    unsigned int base;                                          //Var number base (8,10,16)
    //unsigned int bytes;                                       //Var size in bytes (used for malloc)

    float f;                                                    //Temp var for conversion, 4 bytes
    //unsigned char b;                                          //Temp var for conversion, 1 byte
    //char c;                                                    //Temp var for conversion, 1 byte
    //short h;                                                  //Temp var for conversion, 2 bytes
    //int k;                                                    //Temp var for conversion, 2 bytes
    long l;                                                     //Temp var for conversion, 4 bytes

    char* endptr;                                               //strtoXX() Error detection

    signed char id;
    unsigned int  i;

    //Init (global) variables.
    argfnd=0;
    argcnt=0;
    error =0;

    //Signals empty string...
    id=-1;

    //Zero the two string buffers for splitting cmd string into.
    zeromemory((char*)cmdstr_buf,sizeof(cmdstr_buf));
    zeromemory(argstr_buf,sizeof(argstr_buf));

    //Make it worse in Lint
    for (i=0;i<MAX_ARGS;i++) {
        parms[i].type=PARM_UNUSED;
        zeromemory((char*)&(parms[i].val),sizeof(parms[i].val));
    }

    /*------------------------------------------------
    First, copy the command and convert it to all
    uppercase.
    ------------------------------------------------*/

    strncpy(cmdstr_buf, cmd, sizeof (cmdstr_buf) - 1);
    cmdstr_buf [sizeof (cmdstr_buf) - 1] = '\0';

    /*------------------------------------------------
    Next, find the end of the first thing in the
    buffer.  Since the command ends with a space,
    we'll look for that.  NULL-Terminate the command
    and keep a pointer to the arguments.
    ------------------------------------------------*/

    argsep = strchr(cmdstr_buf, ' ');

    if (argsep == NULL) {
        argstr_buf [0] = '\0';
    } else {
        strcpy (argstr_buf, argsep + 1);
        *argsep = '\0';
    }

    /*------------------------------------------------
    Search for a command ID, then switch on it.  Note
    that I removed my action items for each command,
    but you would invoke each function here.
    VEG:Watch out ID  not neccesarily equal to Array Index!
    ------------------------------------------------*/

    //1) Find the Command Id
    id = cmdb_cmdid_search(cmdstr_buf);

    if (id!=CID_LAST) {
        //2) Tokenize a copy of the parms from the cmd_tbl.

        //Get Format patterns from cmd_tbl[id].parms.
        //Note: strtok inserts \0 into the original string. Hence the copy.
        zeromemory((char *)(&prmstr_buf),sizeof(prmstr_buf));

        strncpy (prmstr_buf, _cmds[id].parms, sizeof (prmstr_buf) - 1);

        argcnt=0;
        tok = strtok(prmstr_buf, " ");
        while (tok != NULL) {
            //Store Pointers
            prms[argcnt++] = tok;

            //cmdb_printf("prm_%2.2d='%s'\r\n",argcnt, tok);

            tok = strtok(NULL, " ");
        }

        //3) Tokenize the commandline.

        //Get Tokens from arguments.
        //Note: strtok inserts \0 into the original string. Won't harm here as we do not re-use it.

        argfnd=0;

        if (strlen(argstr_buf)!=0) {
            tok = strtok(argstr_buf, " ");
        } else {
            tok=NULL;
        }

        while (tok != NULL) {
            //Store Pointers
            toks[argfnd++]=tok;

            //cmdb_printf("tok_%2.2d='%s'\r\n",argfnd, tok);

            tok = strtok(NULL, " ");
        }

        if (argfnd==argcnt || (id==CID_HELP && argfnd==0)) {

            error = 0;

            for (i=0;i<argcnt;i++) {
                //cmdb_printf("prm_%2.2d=%s\r\n",i, prms[i]);

                switch (strlen(prms[i])) {
                    case 0:
                        break;
                    case 1:
                        break;
                    case 2: //Simple pattern, no modifier
                        mod='\0';
                        typ=prms[i][1];

                        break;
                    case 3: //pattern with Modifier.
                        mod=prms[i][1];
                        typ=prms[i][2];

                        break;
                    default:
                        break;
                }

                switch (typ) {
                    case 'o' :
                        base=8;
                        break;
                    case 'x' :
                        base=16;
                        break;
                    default:
                        base=10;
                        break;
                }

                endptr = (char*)toks[i];

                //Cardinal Types
                switch (typ) {
                    case 'd' :  //Check mod
                    case 'i' :  //Check mod
                    case 'u' :  //Check mod
                    case 'o' :  //Check mod
                    case 'x' :  //Check mod
                        switch (mod) {
                            case 'b' : //char
                                //test range
                                l=strtol((char*)toks[i], &endptr, base);
                                if (l>=MIN_BYTE && l<=MAX_BYTE) {
                                    parms[i].type=PARM_CHAR;
                                    parms[i].val.uc =(unsigned char)l;
                                } else {
                                    error = i+1;
                                }

                                break;
                            case 'h' : //short
                                l=strtol((char*)toks[i], &endptr, base);
                                if (l>=MIN_SHORT && l<=MAX_SHORT) {
                                    parms[i].type=PARM_SHORT;
                                    parms[i].val.w=(short)l;
                                } else {
                                    error = i+1;
                                }

                                break;
                            case 'l' : //long
                                l=strtol((char*)toks[i], &endptr, base);
                                parms[i].type=PARM_LONG;
                                parms[i].val.l=l;

                                break;
                            default:
                                l=strtol((char*)toks[i], &endptr, base);
                                if (l>=MIN_INT && l<=MAX_INT) {
                                    parms[i].type=PARM_INT;
                                    parms[i].val.l=(int)l;
                                } else {
                                    error = i+1;
                                }
                                break;
                        }

                        if (error==0 &&
                                (endptr==toks[i]    //No Conversion at all.
                                 || *endptr)) {       //Incomplete conversion.
                            error = i+1;
                        }

                        break;
                }

                //Floating Point Types
                switch (typ) {
                    case 'e' :
                    case 'f' :
                    case 'g' :
                        f = strtod((char*)toks[i], &endptr);

                        parms[i].type=PARM_FLOAT;
                        parms[i].val.f=f;

                        if (error==0 &&
                                (endptr==toks[i]    //No Conversion at all.
                                 || *endptr)) {       //Incomplete conversion.
                            error = i;
                        }

                        break;
                }

                //String types
                switch (typ) {
                    case 'c' :
                        parms[i].type=PARM_CHAR;
                        parms[i].val.c=((char*)toks[i])[0];

                        if (error==0 && strlen((char*)toks[i])!=1) {  //Incomplete conversion.
                            error = i;
                        }

                        break;

                    case 's' :
                        parms[i].type=PARM_STRING;
                        strncpy(parms[i].val.s,(char*)toks[i], strlen((char*)toks[i]));

                        break;
                }
            }
        } else {
            //id=CID_LAST;
        }
    }

    return id;
}

void  Cmdb::cmdb_cmd_proc(char *cmd) {
    int  cid;
    int  ndx;

    cid = cmdb_parse(cmd);
    ndx = cmdb_cmdid_index(cid);

    if (cid!=-1) {

        /*------------------------------------------------
        Process the command and it's arguments that are
        found. id contains the command id and argcnt &
        argfnd the number of found and expected paramaters
        parms contains the parsed argument values and their
        types.
        ------------------------------------------------*/

        if (cid==CID_LAST) {
            cmdb_print("Unknown command, type 'Help' for a list of available commands.\r\n");
        } else {

            //Test for more commandline than allowed too.
            //i.e. run 1 is wrong.

            //TODO Fix Index/Id problem.

            if (argcnt==0 && argfnd==0 && error==0 && ndx!=-1 && _cmds[ndx].subs==SUBSYSTEM) {
                subsystem=cid;
            } else if ( ((cid==CID_HELP) || (argcnt==argfnd)) && error==0 ) {
                switch (cid) {

                        /////// GLOBAL MACRO COMMANDS ///////

                        //Define Macro from commandline
                    case CID_MACRO:
                        macro_ptr=-1;
                        strncpy(macro_buf, STRINGPARM(0), sizeof(macro_buf) - 1);
                        break;

                        //Run Macro
                    case CID_RUN:
                        macro_ptr=0;
                        break;

                        //List Macro's
                    case CID_MACROS:
                        cmdb_print("[Macro]\r\n");
                        if (macro_buf[0]) {
                            cmdb_printf("Value=%s\r\n",macro_buf);
                        } else {
                            cmdb_printf(";No Macro Defined\r\n");
                        }
                        break;

                        /////// GLOBAL STATEMACHINE COMMANDS ///////

#ifdef STATEMACHINE

                        //Start State Machine
                    case CID_STATE:
                        statemachine(BYTEPARM(0));

                        break;
#endif

                        /////// GLOBAL COMMANDS ///////

                        //Echo
                    case CID_ECHO:
                        echo = BOOLPARM(0);
                        break;

                        //Bold
                    case CID_BOLD:
                        bold = BOOLPARM(0);
                        break;

                        //Warm Boot
                    case CID_BOOT:
                        //reset();
                        break;

                        //Sends an ANSI escape code to clear the screen.
                    case CID_CLS:
                        cmdb_print(cls);
                        break;

                        //Returns to CMD> prompt where most commands are disabled.
                    case CID_IDLE:
                        subsystem=-1;
                        break;

                        //Help
                    case CID_HELP: {

//TODO Handle Subsystem

//TODO Call command processor callback and if it returns false we supply help.

                        cmdb_print("\r\n");

                        if (argfnd>0) {
                            cid = cmdb_cmdid_search(STRINGPARM(0));
                        } else {
                            cid=CID_LAST;
                        }

                        if (argfnd>0 && cid!=CID_LAST) {

                            //Help with a valid command as first parameter
                            ndx = cmdb_cmdid_index(cid);

                            switch (_cmds[ndx].subs) {
                                case SUBSYSTEM: //Dump whole subsystem
                                    cmdb_printf("%s subsystem commands:\r\n\r\n",_cmds[ndx].cmdstr);

                                    for (int i=0;i<CMD_TBL_LEN-1;i++) {
                                        if (_cmds[i].subs==ndx) {
                                            cmdb_cmdhelp("",i,",\r\n");
                                        }
                                    }

                                    break;

                                case GLOBALCMD: //Dump command only
                                    //cmdb_print("Global command:\r\n\r\n",cmd_tbl[cmd_tbl[ndx].subs].cmdstr);
                                    cmdb_cmdhelp("Syntax: ",ndx,".\r\n");
                                    break;

                                default:        //Dump one subsystem command
                                    cmdb_printf("%s subsystem command:\r\n\r\n",_cmds[_cmds[ndx].subs].cmdstr);
                                    cmdb_cmdhelp("Syntax: ",ndx,".\r\n");
                                    break;
                            }
                        } else {
                            if (argfnd>0) {
                                //Help with invalid command as first parameter
                                cmdb_print("Unknown command, type 'Help' for a list of available commands.\r\n");
                            } else {
                                //Help

                                //Dump Active Subsystem, Global & Other (dormant) Subsystems
                                for (int i=0;i<CMD_TBL_LEN-1;i++) {
                                    if ((_cmds[i].subs<0) || (_cmds[i].subs==subsystem)) {
                                        cmdb_cmdhelp("",i,",\r\n");
                                    }
                                }
                                cmdb_cmdhelp("",CMD_TBL_LEN-1,".\r\n");
                            }
                        }
                        cmdb_print("\r\n");
                        break;
                    } //CID_HELP
                }
            } else {
                cmdb_cmdhelp("Syntax: ",ndx,".\r\n");
            }
        }
    }
}

//Private Utilities #2

void  Cmdb::cmdb_init(const char full) {
    if (full) {
        echo = true;
        bold = true;

        subsystem = -1;

        lstbuf [cmdndx] = '\0';

        cmdb_macro_reset();

        cmdb_prompt();
    }

    cmdndx = 0;
    cmdbuf [cmdndx] = '\0';

    escndx = 0;
    escbuf [escndx] = '\0';
}

void  Cmdb::cmdb_prompt(void) {
    if (subsystem!=-1) {
        cmdb_printf("%s>",_cmds[subsystem].cmdstr);
    } else {
        cmdb_print(prompt);
    }
}

bool  Cmdb::cmdb_scan(const char c) {
    int i;

    //See http://www.interfacebus.com/ASCII_Table.html

    if (c == '\r') {                                // cr?
        cmdb_print(crlf);                           // Output it and ...
        if (cmdndx) {
            strncpy(lstbuf,cmdbuf,cmdndx);
            lstbuf[cmdndx]='\0';
            cmdb_cmd_proc(cmdbuf);
        }
        cmdb_init(false);
        cmdb_prompt();

        return true;
    }

    //TODO BACKSPACE NOT CORRECT FOR TELNET!

    if (c == '\b') {                                // Backspace
        if (cmdndx != 0) {
            cmdb_print(bs);
            cmdbuf [--cmdndx] = '\0';
        } else {
            cmdb_printch(bell);                     // Output Error
        }
        return false;
    }

    if (c == '\177') {                              // Delete
        while (cmdndx>0) {
            cmdb_print(bs);
            cmdbuf [--cmdndx] = '\0';
        }
        return false;
    }

    //Reset Escape Buffer.
    if (c == '\033') {
        if (escndx!=0) {
            //_putchar(bell);                       // Output Error
            //printf("%s\r\n",escbuf);
        }
        escndx = 0;
        escbuf [escndx] = '\0';                     // NULL-Terminate buffer
    }

    //Extract Escape Sequence.
    if (c == '\033' || escndx ) {                   // Wait for escape
        escbuf [escndx++] = (unsigned char) c;      // Add to the buffer
        escbuf [escndx]   = '\0';                   // NULL-Terminate buffer
        if (isalpha(c)) {
            switch (cmdb_escid_search(escbuf)) {
                case EID_CURSOR_LEFT    : {
                    if (cmdndx != 0) {   // Backspace?
                        cmdb_print(bs);
                        cmdbuf [--cmdndx] = '\0';
                    } else {
                        cmdb_printch(bell);             // Output char
                    }
                    break;
                }
                case EID_CURSOR_UP    : {
                    for (i=0;i<cmdndx;i++) {
                        cmdb_print(bs);
                    }
                    cmdndx=strlen(lstbuf);
                    strncpy(cmdbuf,lstbuf,cmdndx);
                    cmdbuf[cmdndx]='\0';
                    cmdb_printf("%s",cmdbuf);
                    break;
                }
                case EID_CURSOR_RIGHT:
                    break;
                case EID_CURSOR_DOWN    :
                    break;
                case EID_LAST            :
                    break;
                default                     :
                    cmdb_printch(bell);
                    break;
            }
            escndx=0;
            escbuf [escndx]   = '\0';               // NULL-Terminate buffer
        }
        return false;
    }

    if (c=='\n') {                                  // LF
        return false;                               // Dump it
    }

    if (!isprint (c)) {                             // Printable character?
        cmdb_printch(bell);
        return false;
    }

    if (cmdndx >= MAX_CMD_LEN) {                    // Past buffer length?
        cmdb_printch(bell);
        return false;
    }

    cmdbuf [cmdndx++] = (unsigned char) c;          // Add to the buffer
    cmdbuf [cmdndx]   = '\0';                       // NULL-Terminate buffer

    if (echo) {
        cmdb_printch(c);                            // Output char
    }

    return false;
}

//Private Utilities #3

int   Cmdb::cmdb_printf(const char *format, ...) {
    int cnt;

    va_list args;
    char buf[1024];

    memset(buf,'\0',sizeof(buf));

    va_start(args, format);
    cnt = vsnprintf(buf, sizeof(buf), format, args);
    if (cnt==-1) {
        //Error
    }
    va_end(args);

    return cmdb_print(buf);
}

//Link to outside world.
int   Cmdb::cmdb_print(const char *msg) {
    return _serial.printf(msg);
}

//Link to outside world.
char  Cmdb::cmdb_printch(const char ch) {
    return _serial.putc(ch);
}

void  Cmdb::cmdb_cmdhelp(char *pre, int ndx, char *post) {
    int  j;
    int  k;
    int  lastmod;

    k=0;
    lastmod=0;

    switch (_cmds[ndx].subs) {
        case SUBSYSTEM :
            break;
        case GLOBALCMD :
            break;
        case HIDDENSUB :
            return;
        default        :
            if (strlen(pre)==0 && bold) {
                cmdb_print(boldon);
            }
            break;
    }

    cmdb_print(pre);
    k+=strlen(pre);

    if (k==0) {
        cmdb_printf("%12s",_cmds[ndx].cmdstr);
        k+=12;
    } else {
        if (strlen(pre)>0 && bold) {
            cmdb_print(boldon);
        }

        cmdb_printf("%s",_cmds[ndx].cmdstr);
        k+=strlen(_cmds[ndx].cmdstr);

        if (strlen(pre)>0 && bold) {
            cmdb_print(boldoff);
        }
    }

    if (strlen(_cmds[ndx].parms)) {
        cmdb_printch(sp);
        k++;
    }

    for (j=0;j<strlen(_cmds[ndx].parms);j++) {
        switch (_cmds[ndx].parms[j]) {
            case '%' :
                lastmod=0;
                break;

            case 'b' :
                lastmod=8;
                break;
            case 'h' :
                lastmod=16;
                break;
            case 'l' :
                lastmod=32;
                break;

            case 'd' :
            case 'i' :     {
                switch (lastmod) {
                    case  0 :
                    case 16 :
                        cmdb_print("int");
                        k+=3;
                        break;
                    case  8 :
                        cmdb_print("shortint");
                        k+=8;
                        break;
                    case 32:
                        cmdb_print("longint");
                        k+=7;
                        break;
                }
                break;
            }

            case 'u' :
            case 'o' :
            case 'x' :     {
                switch (lastmod) {
                    case  0 :
                    case 16 :
                        cmdb_print("word");
                        k+=4;
                        break;
                    case  8 :
                        cmdb_print("byte");
                        k+=4;
                        break;
                    case 32 :
                        cmdb_print("dword");
                        k+=5;
                        break;
                }

                switch (_cmds[ndx].parms[j]) {
                    case 'o' :
                        cmdb_print("[o]");
                        k+=3;
                        break;
                    case 'x' :
                        cmdb_print("[h]");
                        k+=3;
                        break;
                }

                break;
            }

            case 'e' :
            case 'f' :
            case 'g' :
                cmdb_print("float");
                k+=5;
                break;

            case 'c' :
                cmdb_print("char");
                k+=4;
                break;

            case 's' :
                cmdb_print("string");
                k+=6;
                break;

            case ' ' :
                cmdb_printch(sp);
                k++;
                break;
        }
    }

    for (j=k;j<40;j++) cmdb_printch(sp);

    switch (_cmds[ndx].subs) {
        case SUBSYSTEM :
            if (ndx==subsystem) {
                cmdb_printf("- %s (active subsystem)%s",_cmds[ndx].cmddescr,post);
            } else {
                cmdb_printf("- %s (dormant subsystem)%s",_cmds[ndx].cmddescr,post);
            }
            break;
        case HIDDENSUB :
            break;
        case GLOBALCMD :
            cmdb_printf("- %s (global command)%s",_cmds[ndx].cmddescr,post);
            break;
        default        :
            cmdb_printf("- %s%s",_cmds[ndx].cmddescr,post);
            if (strlen(pre)==0 && bold) {
                cmdb_print(boldoff);
            }
            break;
    }

    if (strlen(pre)>0 && strlen(_cmds[ndx].parmdescr)) {
        cmdb_printf("Params: %s",_cmds[ndx].parmdescr);
        cmdb_print("\r\n");
    }
}

//------------------------------------------------------------------------------
//----Wrappers
//------------------------------------------------------------------------------

void  Cmdb::zeromemory(char *p,unsigned int siz) {
    memset(p,'\0',siz);
}

int  Cmdb::stricmp (char *s1, char *s2) {
    int  i;
    int  len1,len2;

    len1=strlen(s1);
    len2=strlen(s2);

    for (i = 0; (i<len1) && (i<len2);i++) {
        if ( toupper (s1[i])<toupper(s2[i]) ) return (-1);
        if ( toupper (s1[i])>toupper(s2[i]) ) return (+1);
    }

    if (len1<len2) return (-1);
    if (len1>len2) return (+1);

    return (0);
}
