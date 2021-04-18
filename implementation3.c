/*
 * IMPLEMENTATION OF A SIMPLE CALCULATOR.
 * Inputs Operands from the keypad
 * Output of the arithmetic operation is displayed on the Console
 * Operations available : +, -, * and factorial, selected using the keys A, B, C and D
 * Factorial operation is optional.
 * Say, you wish to calculate (978 X 4050)
 * So, you enter 9, then you enter E, press 7, press E, hit 8 and then F so that the operand will be registered. Do the same for second operand of 4050.
 * Once you have entered two operands, press any key from A, B, C or D to choose the corresponding operation.
 * So, the sequence of inputs you entered is, enter one operand, enter second operand and then enter the operation using the key.
 * The calculator is designed in a way ehere you enter the operands first and then select the operation as a third value to the Queue.
 * The corresponding output will be displayed on the console.
 * 32-bit variables are used to store the input as well as output and overflow will generate a wrong output.
 *
 * For subtraction, you may use store_operands[1]-store_operands[0] or vice versa.
 * For factorial, calculate the factorial of the lowest of the two operands.
 * There are no edge cases for addition/multiplication except you detect the overflow for 4-byte variable.
 * Detect the overflow whenever result is greater than 32 bit number.
 * 
 * We do not use SSD here in part 3.
 *
 *
 *  ECE- 315 WINTER 2021 - COMPUTER INTERFACING COURSE
 *
 * 
 *      Author: 	Shyama M. Gandhi & Hetul Patel
 */


//Include FreeRTOS Library
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"

#include "pmodkypd.h"
#include "sleep.h"
#include "xil_cache.h"
#include "math.h"

/* The Tx and Rx tasks as described at the top of this file. */
static void prvTxTask( void *pvParameters );
static void prvRxTask( void *pvParameters );

//factorial function
u32 factorial(int n1);

void DemoInitialize();
u32 SSD_decode(u8 key_value, u8 cathode);

PmodKYPD myDevice;

static TaskHandle_t xTxTask;
static TaskHandle_t xRxTask;
static QueueHandle_t xQueue = NULL;

#define DEFAULT_KEYTABLE "0FED789C456B123A"

void DemoInitialize() {
   KYPD_begin(&myDevice, XPAR_AXI_GPIO_PMOD_KEYPAD_BASEADDR);
   KYPD_loadKeyTable(&myDevice, (u8*) DEFAULT_KEYTABLE);
}

//----------------------------------------------------
// MAIN FUNCTION
//----------------------------------------------------
int main (void)
{
  //int status;

  xil_printf("System Ready!\n");

  /* Create the two tasks.  The Tx task is given a higher priority than the
  Rx task. Dynamically changing the priority of Rx Task later on so the Rx task will leave the Blocked state and pre-empt the Tx
  task as soon as the Tx task fills the queue. */

  xTaskCreate( prvTxTask,					/* The function that implements the task. */
    			( const char * ) "Tx", 		/* Text name for the task, provided to assist debugging only. */
    			configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
    			NULL, 						/* The task parameter is not used, so set to NULL. */
    			tskIDLE_PRIORITY+2,			/* The task runs at the idle priority. */
    			&xTxTask );

  xTaskCreate( prvRxTask,
    			( const char * ) "Rx",
				configMINIMAL_STACK_SIZE,
				NULL,
    			tskIDLE_PRIORITY + 1,
    			&xRxTask );

  /* Create the queue used by the tasks. */
  xQueue = xQueueCreate( 3,				/* There are three items in the queue, two operands and then operation using keypad */
		  	  sizeof( unsigned int ) );	/* Each space in the queue is large enough to hold a uint32_t. */

  /* Check the queue was created. */
  configASSERT(xQueue);

  DemoInitialize();

  vTaskStartScheduler();

  while(1);

  return 0;
}

/*-----------------------------------------------------------*/
static void prvTxTask( void *pvParameters )
{
	UBaseType_t uxPriority;

	for( ;; ) {
	   u16 keystate;
	   XStatus status, last_status = KYPD_NO_KEY;
	   u8 key, last_key = 'x';
	   u32 factor = 0, current_value = 0;
	   
	   Xil_Out32(myDevice.GPIO_addr, 0xF);
	   xil_printf("PMOD KYPD demo started. Press any key on the Keypad.\r\n");

	   uxPriority = uxTaskPriorityGet( NULL );

	   while (1) {

	      // Capture state of each key
	      keystate = KYPD_getKeyStates(&myDevice);

	      // Determine which single key is pressed, if any
	      status = KYPD_getKeyPressed(&myDevice, keystate, &key);

	      //this functions returns the number of entries in the queue, so when the queue is full, i.e., 2 entries, decreased the priority of this task and hence recieve task
	      //will immediately start to execute.
	      if(uxQueueMessagesWaiting( xQueue ) == 3){
		  	  
			  /*********************************/
	    	  // enter the function to dynamically change the priority when queue is full. This way when the queue is full here, we change the priority of this task
	    	  // and hence queue will be read in the receive task to perform the operation. If you change the priority here dynamically, make sure in the receive task to do the counter part!!!
	    	  /*********************************/
	    	  vTaskPrioritySet(xTxTask, uxPriority - 1);
	      }

	      // Print key detect if a new key is pressed or if status has changed
	      if (status == KYPD_SINGLE_KEY
	            && (status != last_status || key != last_key)) {
	         xil_printf("Key Pressed: %c\r\n", (char) key);
	         last_key = key;

	         //whenever 'F' is pressed, the aggregated number will be registered as an operand
	         if((char)key == 'F'){
	        	 current_value = current_value*10 + factor;

	        	 /*******************************/
	        	 //write the logic to enter the updated variable here to the Queue
	        	 /*******************************/
	        	 xQueueSend(xQueue, &current_value, (TickType_t) 0);
		         current_value = 0;
	         }
	         //case when we consider input key strokes from '0' to '9' (only these are the valid key inputs for arithmetic operation)
	         else if((char)key == 'E' ){
	        	 current_value = current_value*10 + factor;
	         }
	         //if user presses 'E' key, consider the last input key pressed as the operand's new digit
	         else if((int)key>=48 && (int)key<=57){
	        	 factor = (int)key - 48;
	         }
	         else if((uxQueueMessagesWaiting( xQueue ) == 2) && ((char)key == 'A' || (char)key == 'B' || (char)key == 'C' || (char)key == 'D')){
	        	 /*****************************************/
	        	 //once two operands are in the queue, enter the third value to the queue to indicate the operation to be performed using A,B,C or D key
	        	 //store the current key value to the queue as the third element
	        	 /*****************************************/
	        	 current_value = (int) key - 48;
	        	 xQueueSend(xQueue, &current_value, (TickType_t) 0);
	        	 
	        	 current_value = 0;
	         }
	      }
	      else if (status == KYPD_MULTI_KEY && status != last_status)
	         xil_printf("Error: Multiple keys pressed\r\n"); //this is valid whenever two or more keys are pressed together

	      last_status = status;
	      usleep(1000);
	   }
	}
}
/*-----------------------------------------------------------*/
static void prvRxTask( void *pvParameters )
{
	UBaseType_t uxPriority;
	uxPriority = uxTaskPriorityGet( NULL );

	for( ;; )
	{

		/***************************************/
		//Write code here to read the three elements from the queue and perform the required operation.
		//Display the output result on the console.
		//If you have dynamically changed the priority of this task in TxTask, you need to change the priority here accordingly, respectively using vTaskPrioritySet()
		//This way once the RxTask is done, TxTask will have a higher priority and hence will wait for the inputs
		/***************************************/
		u32 read_queue_value;
		u32 store_operands[2];
		int operation;
		u32 result;

		xQueueReceive(xQueue, &read_queue_value, (TickType_t) 0);
		store_operands[0] = read_queue_value;
		xQueueReceive(xQueue, &read_queue_value, (TickType_t) 0);
		store_operands[1] = read_queue_value;

		xQueueReceive(xQueue, &read_queue_value, (TickType_t) 0);
		operation = (int) read_queue_value;



		switch(operation){

			case 17:
				//A
				{result=store_operands[0]+store_operands[1]; break;}


			case 18:
				//B
				{result = store_operands[0] - store_operands[1]; break;}


			case 19:
				//C
				{result = store_operands[0] * store_operands[1]; break;}


			case 20: {
				//D
				result = factorial(store_operands[1]);
				break;
			}

			default: result = 0;
			{
				xil_printf("No operation selected by user \n");
				break;
			}
		}
		xil_printf("Arithmetic operation result = %d\n\n",result);
		vTaskPrioritySet(xTxTask, uxPriority + 1);
	}
}

u32 factorial(int n1){

	u32 factorial_answer=1;
	for(int i=1; i<=n1; i++){

		/**********************************/
		//complete the factorial logic here
		/**********************************/
		factorial_answer = i * factorial_answer;
	}

	return factorial_answer;
}

