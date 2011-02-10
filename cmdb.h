/* mbed Command Interpreter Library
 * Copyright (c) 2011 wvd_vegt
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef MBED_CMDB_H
#define MBED_CMDB_H

#include "mbed.h"

#include <vector>

//Max size of an Ansi escape code.
#define MAX_ESC_LEN   5

//Max (strlen) of a Param.
#define MAX_PARM_LEN 32

//Max eight parms.
#define MAX_ARGS 8

//Max 132 characters commandline.
#define MAX_CMD_LEN 132

//'Show' hidden subsystems and commands.
#define SHOWHIDDEN

#define MIN_BYTE        -128
#define MAX_BYTE        +127

#define MIN_SHORT     -32768
#define MAX_SHORT     +32767

#define MIN_INT       -32768
#define MAX_INT       +32767

//TODO Make sure we use id and array index properly!!

struct cmdb_cmd {
    char *cmdstr;
    int  subs;
    int  id;                                            //Changed to int as signed char won't compile
    char *parms;
    char *cmddescr;
    char *parmdescr;
};

//----Escape Codes and Strings

const char cr           = '\r';
const char lf           = '\n';
const char bell         = '\7';
const char esc          = '\033';
const char sp           = ' ';
const char crlf[]       = "\r\n\0";

const char bs[]         = "\b \b\0";

const char boldon[]     = "\033[1m\0";
const char boldoff[]    = "\033[0m\0";
const char cls[]        = "\033[2J\0";
const char home[]       = "\033[H\0";

const char prompt[]     = "CMD>";

//Before including this file, define CID_LAST as the last value from the enum with commands.

//#define CMD_TBL_LEN  CID_LAST

#define SUBSYSTEM         -1
#define GLOBALCMD         -2
#define HIDDENSUB         -3

#define CID_BOOT    9991
#define CID_MACRO   9992
#define CID_RUN     9993
#define CID_MACROS  9994

#define CID_ECHO    9995
#define CID_BOLD    9996
#define CID_CLS     9997
#define CID_IDLE    9998
#define CID_HELP    9999

//You need to add the following commands to your command table.

//Optional
cmdb_cmd BOOT =     { "Boot",           GLOBALCMD    ,CID_BOOT      ,""                 ,"Boot"                         ,""};

//Optional
cmdb_cmd MACRO =    { "Macro",          GLOBALCMD    ,CID_MACRO     ,"%s"               ,"Define macro (sp->_, cr->|)"  ,"command(s)"};
cmdb_cmd RUN =      { "Run",            GLOBALCMD    ,CID_RUN       ,""                 ,"Run a macro"                  ,""};
cmdb_cmd MACROS =   { "Macros",         GLOBALCMD    ,CID_MACROS    ,""                 ,"List macro(s)"                ,""};

//Optional
cmdb_cmd ECHO =     { "Echo",           GLOBALCMD    ,CID_ECHO      ,"%bu"              ,"Echo On|Off (1|0)"            ,"state"};
cmdb_cmd BOLD =     { "Bold",           GLOBALCMD    ,CID_BOLD      ,"%bu"              ,"Bold On|Off (1|0)"            ,"state"};
cmdb_cmd CLS  =     { "Cls",            GLOBALCMD    ,CID_CLS       ,""                 ,"Clears the terminal screen"   ,""};

//Mandatory!
cmdb_cmd IDLE =     { "Idle",           GLOBALCMD    ,CID_IDLE      ,""                 ,"Deselect Subsystems"          ,""};

//Mandatory!
cmdb_cmd HELP =     { "Help",           GLOBALCMD    ,CID_HELP      ,"%s"               ,"Help"                         ,""};

#define ESC_TBL_LEN  4

struct esc_st {
    char     *escstr;
    int     id;
};

enum {
    EID_CURSOR_UP,
    EID_CURSOR_DOWN,
    EID_CURSOR_RIGHT,
    EID_CURSOR_LEFT,
    EID_LAST
};

const struct esc_st esc_tbl [ESC_TBL_LEN] = {
    { "\033[A",    EID_CURSOR_UP    },
    { "\033[B",    EID_CURSOR_DOWN  },
    { "\033[C",    EID_CURSOR_RIGHT },
    { "\033[D",    EID_CURSOR_LEFT  },
};

//Define a const struct cmbd_cmd cmdb_tbl [CMD_TBL_LEN] {}; that is passed into the constructor.

/** Command Interpreter class.
 *
 * Steps to take:
 *
 * 1) Create a std::vector<cmdb_cmd> and fill it with at least
 *    the mandatory commands IDLE and HELP.
 * 2) Create an Cmdb class instance and pass it both the vector and
 *    a Serial port object like Serial serial(USBTX, USBRX);
 * 3) Feed the interpreter with characters received from your serial port.
 *    Note Cmdb self does not retrieve input it must be handed to it
 * 4) Handle commands added by the application by the Id and parameters passed.
 *
 */
class Cmdb {
public:
    /** Create a Command Interpreter.
     *
     * @param serial a Serial port used for communication.
     * @param cmds a vector with the command table.
     */
    Cmdb(const Serial serial, const std::vector<cmdb_cmd>& cmds);

    /** Checks if the macro buffer has any characters left.
     *
     * @returns true if any characters left.
     */
    bool cmdb_macro_hasnext();

    /** Gets the next character from the macro buffer and 
     *  advances the macro buffer pointer.
     *
     *  Do not call if no more characters are left!
     *
     * @returns the next character.
     */
    char cmdb_macro_next();

    /** Gets the next character from the macro buffer
     *  but does not advance the macro buffer pointer.
     *
     *  Do not call if no more characters are left!
     *
     * @returns the next character.
     */
    char cmdb_macro_peek();
    
    /** Resets the macro buffer and macro buffer pointer.
     * 
     */
    void cmdb_macro_reset();

    /** Checks if the macro buffer has any characters.
     *
     * @returns true if any characters available.
     */
    bool cmdb_macro_hasnext();

    /** Checks if the serial port has any characters 
     * left to read by calling serial.readable().
     *
     * @returns true if any characters available.
     */
    bool cmdb_hasnext();

    /** Gets the next character from the serial port by 
     *  calling serial.getc().
     *
     *  Do not call if no characters are left!
     *
     * @returns the next character.
     */
    char cmdb_next();

private:
    //Utilities.
    /** Checks if the macro buffer has any characters left.
     *
     * @param char* escstr the escape code to lookup.
     *
     * @returns the index of the escape code or -1.
     */
    int cmdb_escid_search(char *escstr);
    int cmdb_cmdid_search(char *cmdstr);
    int  cmdb_cmdid_index(int cmdid);

    //Parser.
    void cmdb_init(const char full);
    void cmdb_prompt(void);
    bool cmdb_scan(const char c);
    int cmdb_parse(char *cmd);

    //Command Processor.
    //TODO Must call Callback function.
    void cmdb_cmd_proc(char *cmd);

    //Help
    void cmdb_cmdhelp(char *pre, int ndx, char *post);

    //Output.
    int cmdb_printf(const char *format, ...);
    int cmdb_print(const char *msg);
    char cmdb_printch(const char ch);

    //Utilities.
    void zeromemory(char *p,unsigned int siz);
    int stricmp (char *s1, char *s2);


    //Storage, see http://www.cplusplus.com/reference/stl/vector/
    std::vector<cmdb_cmd> _cmds;
    Serial _serial;
    bool echo;
    bool bold;

    int CID_LAST;
    int CMD_TBL_LEN;

    //Macro's.
    int macro_ptr;
    char macro_buf[1 + MAX_CMD_LEN];

    enum parmtype {
        PARM_UNUSED,            //0

        PARM_FLOAT,             //1     (f)

        PARM_LONG,              //2     (l/ul)
        PARM_INT,               //3     (i/ui)
        PARM_SHORT,             //4     (w/uw)

        PARM_CHAR,              //5     (c/uc)
        PARM_STRING             //6     (s)
    };

    union value {
        float               f;

        unsigned long       ul;
        long                 l;

        int                  i;
        unsigned int        ui;

        short                w;
        unsigned short      uw;

        char                 c;
        unsigned char       uc;

        char                 s[MAX_PARM_LEN];
    };

    struct parm {
        enum parmtype    type;
        union value     val;
    };

//------------------------------------------------------------------------------
//----These helper functions retieve parameters in the correct format.
//------------------------------------------------------------------------------

//TODO Add tests for correct type of parameter.

    bool BOOLPARM(int ndx) {
        return parms[ndx].val.uc!=0;
    }

    unsigned char BYTEPARM(int ndx) {
        return parms[ndx].val.uc;
    }

    char CHARPARM(int ndx) {
        return parms[ndx].val.c;
    }

    unsigned int WORDPARM(int ndx) {
        return parms[ndx].val.ui;
    }

    unsigned int UINTPARM(int ndx) {
        return parms[ndx].val.ui;
    }

    int INTPARM(int ndx) {
        return parms[ndx].val.i;
    }

    unsigned long DWORDPARM(int ndx) {
        return parms[ndx].val.ul;
    }

    long LONGPARM(int ndx) {
        return parms[ndx].val.l;
    }

    float FLOATPARM(int ndx) {
        return parms[ndx].val.f;
    }

    char* STRINGPARM(int ndx) {
        return parms[ndx].val.s;
    }

//------------------------------------------------------------------------------
//----Buffers
//------------------------------------------------------------------------------

    char            cmdbuf [1 + MAX_CMD_LEN];           // command buffer
    char            cmdndx;                             // command index

    char            lstbuf [1 + MAX_CMD_LEN];           // last command buffer

    char            escbuf [1 + MAX_ESC_LEN];           // command buffer
    unsigned char   escndx;                             // command index

    struct          parm parms[MAX_ARGS];
    int             noparms;

    int             subsystem;

    int     argcnt;                                     //No of arguments found in command
    int     argfnd;                                     //No of arguments to find in parameter definition.
    int     error;                                      //strtoXX() Error detection

};

#endif