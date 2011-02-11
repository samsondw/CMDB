#include <vector>

#include "mbed.h"
#include "cmdb.h"

DigitalOut myled(LED1);

//We'll be using the Usb Serial port
Serial serial(USBTX, USBRX); //tx, rx

#define CID_TEST 1
#define CID_INT  2

/** Sample User Command Dispatcher.
 *
 * @parm cmdb the command interpreter object.
 * @parm cid the command id.
 */
void my_dispatcher(Cmdb& cmdb, int cid) {
    cmdb.printf("my_dispatcher: cid=%d\r\n", cid);

    switch (cid) {
        case CID_INT :
            cmdb.printf("my_dispatcher: parm 0=%d\r\n",cmdb.INTPARM(0));
            break;
    }
}

const cmd c1("Test",SUBSYSTEM,CID_TEST,""  ,"* Test Subsystem");
const cmd c2("Int" ,CID_TEST ,CID_INT ,"%i","* Int as parameter" ,"dummy");

int main() {
    // Set the Baudrate.
    serial.baud(115200);

    // Test the serial connection by
    serial.printf("\r\n\r\nCmdb Command Interpreter Demo Version %0.2f.\r\n\r\n", Cmdb::version());

    //Create a Command Table Vector.
    std::vector<cmd> cmds;

    //Add some of our own first...
    cmds.push_back(c1); //Test Subsystem is handled by Cmdb internally.
    cmds.push_back(c2); //The Int Command is handled by our 'my_dispatcher' method.

    //Add some predefined...
    cmds.push_back(BOOT); //Handled by Cmdb internally.

    cmds.push_back(ECHO); //Handled by Cmdb internally.
    cmds.push_back(BOLD); //Handled by Cmdb internally.
    cmds.push_back(CLS);  //Handled by Cmdb internally.

    cmds.push_back(MACRO);  //Handled by Cmdb internally.
    cmds.push_back(RUN);    //Handled by Cmdb internally.
    cmds.push_back(MACROS); //Handled by Cmdb internally.

    //Add some predefined and mandatory...
    cmds.push_back(IDLE); //Handled by Cmdb internally.
    cmds.push_back(HELP); //Handled by Cmdb internally.

    //Create and initialize the Command Interpreter.
    Cmdb cmdb(serial, cmds, &my_dispatcher);

    while (1) {
        //Check for input...
        if (cmdb.hasnext()==true) {

            //Supply input to Command Interpreter
            if (cmdb.scan(cmdb.next())) {

                //Flash led when a command has been parsed and dispatched.
                myled = 1;
                wait(0.2);

                //cmdb.print("Command Parsed and Dispatched\r\n");

                myled = 0;
                wait(0.2);
            }
        }

        //For Macro Support we basically do the same but take characters from the macro buffer.
        //Example Macro: Test|Int_42|Idle
        while (cmdb.macro_hasnext()) {
            //Get and process next character.
            cmdb.scan(cmdb.macro_next());
            
            //After the last character we need to add a cr to force execution.    
            if (!cmdb.macro_peek()) {
                cmdb.scan(cr);
            }
        }
    }
}
