/*
_____________________________________________________________________________

   Project:     mBed Command Interpreter
   Filename:    cmdb.h
   Version:     0.5.0
_____________________________________________________________________________
   Date         Comment
   -------- --------------------------------------------------------------
   10022011 -Rewritten into C++ class.
            -Pass Serial into constructor for printf, putc and getc.
            -CID_<subsystem> must be handled internally.
            -Fixed a number of old Index/Id conflicts.
            -Got a working version. Much work to be done though.
            -Handle CID_HELP internally (like all system commands (IDLE/MACRO etc).
            -Defined CID_LAST as Vector Size or Last Command Id fails.
            -Removed CMD_TBL_LEN.
            -CID_LAST is now defined as CID_HELP+1.
            -Added Documentation.
            -Added code to take number limits from the C++ Runtime instead of hard defined values.
            -Renamed id to cid in cmd. 
            -Added MAX_LONG and MIN_LONG (long==int on mbed).
            -Removed cmdb_ prefix from members.
   -------- --------------------------------------------------------------
   TODO's
   10022011 -Tweak and Review Documentation.
   11022011 -Check & Test **PARM masks.
            -Remove prefix from class members?
   -------- --------------------------------------------------------------
_____________________________________________________________________________
*/

#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include "cmdb.h"
#include "mbed.h"

//------------------------------------------------------------------------------

Cmdb::Cmdb(const Serial& serial, const std::vector<cmd>& cmds, void (*callback)(Cmdb&,int)) :
        serial(serial), cmds(cmds) {
    echo = true;
    bold = true;
    
    subsystem = -1;

    callback = callback;

    init(true);
}

//------------------------------------------------------------------------------
// Public Stuff.
//------------------------------------------------------------------------------

bool  Cmdb::macro_hasnext() {
    return macro_ptr!=-1 && macro_ptr<MAX_CMD_LEN && macro_buf[macro_ptr];
}

char Cmdb::macro_next() {
    char ch = macro_buf[macro_ptr++];

    if (macro_ptr==MAX_CMD_LEN) {
        macro_reset();
    }

    return ch;
}

char  Cmdb::macro_peek() {
    return macro_buf[macro_ptr];
}

void  Cmdb::macro_reset() {
    macro_ptr         = -1;
    macro_buf[0]     = '\0';
}

//------------------------------------------------------------------------------

bool  Cmdb::hasnext() {
    return serial.readable()==1;
}

char  Cmdb::next() {
    return serial.getc();
}

//------------------------------------------------------------------------------

bool  Cmdb::scan(const char c) {
    int i;

    //See http://www.interfacebus.com/ASCII_Table.html

    if (c == '\r') {                                // cr?
        print(crlf);                           // Output it and ...
        if (cmdndx) {
            strncpy(lstbuf,cmdbuf,cmdndx);
            lstbuf[cmdndx]='\0';

            cmd_dispatcher(cmdbuf);
        }
        init(false);
        prompt();

        return true;
    }

    //TODO BACKSPACE NOT CORRECT FOR TELNET!

    if (c == '\b') {                                // Backspace
        if (cmdndx != 0) {
            print(bs);
            cmdbuf [--cmdndx] = '\0';
        } else {
            printch(bell);                     // Output Error
        }
        return false;
    }

    if (c == '\177') {                              // Delete
        while (cmdndx>0) {
            print(bs);
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
            switch (escid_search(escbuf)) {
                case EID_CURSOR_LEFT    : {
                    if (cmdndx != 0) {   // Backspace?
                        print(bs);
                        cmdbuf [--cmdndx] = '\0';
                    } else {
                        printch(bell);             // Output char
                    }
                    break;
                }
                case EID_CURSOR_UP    : {
                    for (i=0;i<cmdndx;i++) {
                        print(bs);
                    }
                    cmdndx=strlen(lstbuf);
                    strncpy(cmdbuf,lstbuf,cmdndx);
                    cmdbuf[cmdndx]='\0';
                    printf("%s",cmdbuf);
                    break;
                }
                case EID_CURSOR_RIGHT:
                    break;
                case EID_CURSOR_DOWN    :
                    break;
                case EID_LAST            :
                    break;
                default                     :
                    printch(bell);
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
        printch(bell);
        return false;
    }

    if (cmdndx >= MAX_CMD_LEN) {                    // Past buffer length?
        printch(bell);
        return false;
    }

    cmdbuf [cmdndx++] = (unsigned char) c;          // Add to the buffer
    cmdbuf [cmdndx]   = '\0';                       // NULL-Terminate buffer

    if (echo) {
        printch(c);                            // Output char
    }

    return false;
}

//------------------------------------------------------------------------------

int   Cmdb::printf(const char *format, ...) {
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

    return print(buf);
}

int   Cmdb::print(const char *msg) {
    return serial.printf(msg);
}

char  Cmdb::printch(const char ch) {
    return serial.putc(ch);
}

//------------------------------------------------------------------------------

void  Cmdb::init(const char full) {
    if (full) {
        echo = true;
        bold = true;

        subsystem = -1;

        lstbuf [cmdndx] = '\0';

        macro_reset();

        prompt();
    }

    cmdndx = 0;
    cmdbuf [cmdndx] = '\0';

    escndx = 0;
    escbuf [escndx] = '\0';
}

//------------------------------------------------------------------------------
//Private Stuff.
//------------------------------------------------------------------------------

int  Cmdb::escid_search(char *escstr) {
    for (int i=0; i<ESC_TBL_LEN; i++) {
        if (strcmp (esc_tbl[i].escstr, escstr) == 0)
            return (esc_tbl[i].id);
    }

    return (EID_LAST);
}

int  Cmdb::cmdid_search(char *cmdstr) {
    //Warning, we return the ID but somewhere assume it's equal to the array index!
    for (int i=0; i<cmds.size(); i++) {
        if ((stricmp(cmds[i].cmdstr, cmdstr) == 0) && ((cmds[i].subs == subsystem) || (cmds[i].subs<0)))
            return (cmds[i].cid);
    }

    return CID_LAST;
}

int  Cmdb::cmdid_index(int cmdid) {
    for (int i=0; i<cmds.size(); i++) {
        if (cmds[i].cid==cmdid)
            return i;
    }

    return -1;
}

//------------------------------------------------------------------------------

int Cmdb::parse(char *cmd) {
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

    int cid = -1;                                               //Signals empty string...
    int ndx = -1;

    //Init (global) variables.
    argfnd=0;
    argcnt=0;
    error =0;

    //Zero the two string buffers for splitting cmd string into.
    zeromemory((char*)cmdstr_buf,sizeof(cmdstr_buf));
    zeromemory(argstr_buf,sizeof(argstr_buf));

    //Make it worse in Lint
    for (int i=0;i<MAX_ARGS;i++) {
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
    Search for a command ID, then switch on it.
    ------------------------------------------------*/

    //1) Find the Command Id
    cid = cmdid_search(cmdstr_buf);

    if (cid!=CID_LAST) {
        //2) Tokenize a copy of the parms from the cmd_tbl.

        ndx = cmdid_index(cid);

        //Get Format patterns from cmd_tbl[id].parms.
        //note: strtok inserts \0 into the original string. Hence the copy.
        zeromemory((char *)(&prmstr_buf),sizeof(prmstr_buf));

        strncpy (prmstr_buf, cmds[ndx].parms, sizeof (prmstr_buf) - 1);

        argcnt=0;
        tok = strtok(prmstr_buf, " ");
        while (tok != NULL) {
            //Store Pointers
            prms[argcnt++] = tok;

            //printf("prm_%2.2d='%s'\r\n",argcnt, tok);

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

            //printf("tok_%2.2d='%s'\r\n",argfnd, tok);

            tok = strtok(NULL, " ");
        }

        if (argfnd==argcnt || (cid==CID_HELP && argfnd==0)) {

            error = 0;

            for (int i=0;i<argcnt;i++) {
                //printf("prm_%2.2d=%s\r\n",i, prms[i]);

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
                            default: //int
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
            cid=CID_LAST;
        }
    }

    return cid;
}

//------------------------------------------------------------------------------

void  Cmdb::cmd_dispatcher(char *cmd) {
    int  cid;
    int  ndx;

    cid = parse(cmd);
    ndx = cmdid_index(cid);

    if (cid!=-1) {
        //printf("cmds[%d]=%d\r\n",ndx, cid);

        /*------------------------------------------------
        Process the command and it's arguments that are
        found. id contains the command id and argcnt &
        argfnd the number of found and expected paramaters
        parms contains the parsed argument values and their
        types.
        ------------------------------------------------*/

        if (cid==CID_LAST) {
            print("Unknown command, type 'Help' for a list of available commands.\r\n");
        } else {
            //printf("cmds[%d]=%d [%s]\r\n",ndx, cid, _cmds[ndx].cmdstr);

            //Test for more commandline than allowed too.
            //i.e. run 1 is wrong.

            if (argcnt==0 && argfnd==0 && error==0 && ndx!=-1 && cmds[ndx].subs==SUBSYSTEM) {
                //Handle all SubSystems.
                subsystem=cid;
            } else if ( ((cid==CID_HELP) || (argcnt==argfnd)) && error==0 ) {
                switch (cid) {

#ifdef ENABLEMACROS
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
                        print("[Macro]\r\n");
                        if (macro_buf[0]) {
                            printf("Value=%s\r\n",macro_buf);
                        } else {
                            printf(";No Macro Defined\r\n");
                        }
                        break;

#endif //ENABLEMACROS

#ifdef STATEMACHINE
                        /////// GLOBAL STATEMACHINE COMMANDS ///////

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
                        mbed_reset();
                        break;

                        //Sends an ANSI escape code to clear the screen.
                    case CID_CLS:
                        print(cls);
                        break;

                        //Returns to CMD> prompt where most commands are disabled.
                    case CID_IDLE:
                        subsystem=-1;
                        break;

                        //Help
                    case CID_HELP: {
                        print("\r\n");

                        if (argfnd>0) {
                            cid = cmdid_search(STRINGPARM(0));
                        } else {
                            cid=CID_LAST;
                        }

                        if (argfnd>0 && cid!=CID_LAST) {

                            //Help with a valid command as first parameter
                            ndx = cmdid_index(cid);

                            switch (cmds[ndx].subs) {
                                case SUBSYSTEM: { //Dump whole subsystem
                                    printf("%s subsystem commands:\r\n\r\n",cmds[ndx].cmdstr);

                                    //Count SubSystem Commands.
                                    int subcmds =0;
                                    for (int i=0;i<cmds.size();i++) {
                                        if (cmds[i].subs==cid) {
                                            subcmds++;
                                        }
                                    }

                                    //Print SubSystem Commands.
                                    for (int i=0;i<cmds.size()-1;i++) {
                                        if (cmds[i].subs==cid) {
                                            subcmds--;
                                            if (subcmds!=0) {
                                                cmdhelp("",i,",\r\n");
                                            } else {
                                                cmdhelp("",i,".\r\n");
                                            }
                                        }
                                    }
                                }
                                break;

                                case GLOBALCMD: //Dump command only
                                    //print("Global command:\r\n\r\n",cmd_tbl[cmd_tbl[ndx].subs].cmdstr);
                                    cmdhelp("Syntax: ",ndx,".\r\n");
                                    break;

                                default:        //Dump one subsystem command
                                    printf("%s subsystem command:\r\n\r\n",cmds[cmds[ndx].subs].cmdstr);
                                    cmdhelp("Syntax: ",ndx,".\r\n");
                                    break;
                            }
                        } else {
                            if (argfnd>0) {
                                //Help with invalid command as first parameter
                                print("Unknown command, type 'Help' for a list of available commands.\r\n");
                            } else {
                                //Help

                                //Dump Active Subsystem, Global & Other (dormant) Subsystems
                                //-1 because we want comma's and for the last a .
                                for (int i=0;i<cmds.size()-1;i++) {
                                    if ((cmds[i].subs<0) || (cmds[i].subs==subsystem)) {
                                        cmdhelp("",i,",\r\n");
                                    }
                                }
                                cmdhelp("",cmds.size()-1,".\r\n");
                            }
                        }
                        print("\r\n");
                        break;
                    } //CID_HELP

                    default : {
                        // Do a Call to the Application's Command Dispatcher.
                        (*callback)(*this, cid);
                    }
                }
            } else {
                cmdhelp("Syntax: ",ndx,".\r\n");
            }

        }

    } else {
        //cid==-1
    }
}

void  Cmdb::prompt(void) {
#ifdef SUBSYSTEMPROMPTS
    if (subsystem!=-1) {
        int ndx = cmdid_index(subsystem);

        printf("%s>",_cmds[ndx].cmdstr);

        return;
    }
#endif //SUBSYSTEMPROMPTS

    printf(PROMPT);
}

void  Cmdb::cmdhelp(char *pre, int ndx, char *post) {
    int  j;
    int  k;
    int  lastmod;

    k=0;
    lastmod=0;

    switch (cmds[ndx].subs) {
        case SUBSYSTEM :
            break;
        case GLOBALCMD :
            break;
        case HIDDENSUB :
            return;
        default        :
            if (strlen(pre)==0 && bold) {
                print(boldon);
            }
            break;
    }

    print(pre);
    k+=strlen(pre);

    if (k==0) {
        printf("%12s",cmds[ndx].cmdstr);
        k+=12;
    } else {
        if (strlen(pre)>0 && bold) {
            print(boldon);
        }

        printf("%s",cmds[ndx].cmdstr);
        k+=strlen(cmds[ndx].cmdstr);

        if (strlen(pre)>0 && bold) {
            print(boldoff);
        }
    }

    if (strlen(cmds[ndx].parms)) {
        printch(sp);
        k++;
    }

    for (j=0;j<strlen(cmds[ndx].parms);j++) {
        switch (cmds[ndx].parms[j]) {
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
                        print("int");
                        k+=3;
                        break;
                    case  8 :
                        print("shortint");
                        k+=8;
                        break;
                    case 32:
                        print("longint");
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
                        print("word");
                        k+=4;
                        break;
                    case  8 :
                        print("byte");
                        k+=4;
                        break;
                    case 32 :
                        print("dword");
                        k+=5;
                        break;
                }

                switch (cmds[ndx].parms[j]) {
                    case 'o' :
                        print("[o]");
                        k+=3;
                        break;
                    case 'x' :
                        print("[h]");
                        k+=3;
                        break;
                }

                break;
            }

            case 'e' :
            case 'f' :
            case 'g' :
                print("float");
                k+=5;
                break;

            case 'c' :
                print("char");
                k+=4;
                break;

            case 's' :
                print("string");
                k+=6;
                break;

            case ' ' :
                printch(sp);
                k++;
                break;
        }
    }

    for (j=k;j<40;j++) printch(sp);

    switch (cmds[ndx].subs) {
        case SUBSYSTEM :
            if (ndx==subsystem) {
                printf("- %s (active subsystem)%s",cmds[ndx].cmddescr,post);
            } else {
                printf("- %s (dormant subsystem)%s",cmds[ndx].cmddescr,post);
            }
            break;
        case HIDDENSUB :
            break;
        case GLOBALCMD :
            printf("- %s (global command)%s",cmds[ndx].cmddescr,post);
            break;
        default        :
            printf("- %s%s",cmds[ndx].cmddescr,post);
            if (strlen(pre)==0 && bold) {
                print(boldoff);
            }
            break;
    }

    if (strlen(pre)>0 && strlen(cmds[ndx].parmdescr)) {
        printf("Params: %s",cmds[ndx].parmdescr);
        print("\r\n");
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

//------------------------------------------------------------------------------
