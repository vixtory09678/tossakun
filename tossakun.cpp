#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <wiringSerial.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>

#include <iostream>
#include <stdexcept>

#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

#include <pigpio.h>


#define _BV(shift) (shift << 6)
#define JOY_DEV "/dev/input/js0"

#define PIN_PWM2 13
#define PIN_PWM1 12

#define DIR1_B 26 
#define DIR2_B 19

#define DIR1_A 20
#define DIR2_A 21

#define HIGH 1
#define LOW 0

#define SECOND(x) 1000000*x
#define MILLIS(x) 1000*x

#define A_KEY 2
#define X_KEY 3
#define Y_KEY 0
#define B_KEY 1

//23 24 25
//pwm dir dir
//27 28 29
using namespace std;

unsigned fd;

void beginBaudrate(char* port,int baudrate);
unsigned char addChecksum(int angle,int time,int addr);
void sendData(int angle,int time,int addr,int isContinue);

string exec(const char* cmd) {
    char buffer[128];
    string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw runtime_error("popen() failed!");
    try {
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != NULL)
                result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}

pid_t proc_find(const char* name)
{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[512];

    if (!(dir = opendir("/proc"))) {
        perror("can't open /proc");
        return -1;
    }

    while((ent = readdir(dir)) != NULL) {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        long lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;
        }

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
        FILE* fp = fopen(buf, "r");

        if (fp) {
            if (fgets(buf, sizeof(buf), fp) != NULL) {
            	/* check the first token in the file, the program name */
                char* first = strtok(buf, " ");
                if (!strcmp(first, name)) {
                    fclose(fp);
                    closedir(dir);
                    return (pid_t)lpid;
                }
            }
            fclose(fp);
        }

    }

    closedir(dir);
    return -1;
}


long map(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void driveMotor(int pwmA , int pwmB){
	if(pwmA > 0){
		gpioWrite(DIR1_A,HIGH);
		gpioWrite(DIR2_A,LOW);
	}else{
		gpioWrite(DIR1_A,LOW);
		gpioWrite(DIR2_A,HIGH);
	}

	if(pwmB > 0){
		gpioWrite(DIR1_B,HIGH);
		gpioWrite(DIR2_B,LOW);
	}else{
		gpioWrite(DIR1_B,LOW);
		gpioWrite(DIR2_B,HIGH);
	}

	int speedA = map(abs(pwmA),0,10000,0,8000);
	int speedB = map(abs(pwmB),0,10000,0,8000);

	if(speedA > 8000) speedA = 8000;
	if(speedB > 8000) speedB = 8000;

	gpioPWM(PIN_PWM1, speedA);
	gpioPWM(PIN_PWM2, speedB);
}


int main()
{


	if (gpioInitialise() < 0)
	{
	    fprintf(stderr, "pigpio initialisation failed\n");
	    return 1;
	}
	if((fd = serialOpen("/dev/ttyACM0",115200)) < 0){
		fprintf (stderr, "Unable to open serial device: %s\n", strerror (errno)) ;
	}
	// if((fd = serOpen((char*)"/dev/ttyACM0",115200,0))>= 0){
	// 	printf("Open success\n");
	// }

	
	
	// setup joystick
	int f;
	int joy_fd, *axis=NULL, num_of_axis=0, num_of_buttons=0, x;
	char *button=NULL, name_of_joystick[80];
	struct js_event js;
	 
	if( ( joy_fd = open( JOY_DEV , O_RDONLY)) == -1 )
	{
		printf( "Couldn't open joystick\n" );
		return -1;
	}

	ioctl( joy_fd, JSIOCGAXES, &num_of_axis );
	ioctl( joy_fd, JSIOCGBUTTONS, &num_of_buttons );
	ioctl( joy_fd, JSIOCGNAME(80), &name_of_joystick );
	 
	axis = (int *) calloc( num_of_axis, sizeof( int ) );
	button = (char *) calloc( num_of_buttons, sizeof( char ) );
	 
	printf("Joystick detected: %s\n\t%d axis\n\t%d buttons\n\n"
	, name_of_joystick
	, num_of_axis
	, num_of_buttons );
	 
	fcntl( joy_fd, F_SETFL, O_NONBLOCK ); /* use non-blocking mode */

	 //end setup joystick

	int motorACommand = -1;
	int motorBCommand = -1;

	int pwmA = -1;
	int pwmB = -1;



	gpioSetMode(PIN_PWM1,PI_OUTPUT); // setup pwm
	gpioSetMode(PIN_PWM2,PI_OUTPUT);

	gpioSetPWMrange(PIN_PWM1,10000);
	gpioSetPWMrange(PIN_PWM2,10000);

	gpioSetMode(DIR1_A,PI_OUTPUT);
	gpioSetMode(DIR2_A,PI_OUTPUT);

	gpioSetMode(DIR1_B,PI_OUTPUT);
	gpioSetMode(DIR2_B,PI_OUTPUT);

	

	//end setup
	int check1 = 0,check2 = 0,check3 = 0,check4 = 0,check5 = 0,
	check6 = 0,check7 = 0,check8 = 0,check9 = 0,check10 = 0,check11 = 0,
	check12 = 0;

	for(;;){

		switch(js.type & ~JS_EVENT_INIT){
			case JS_EVENT_AXIS:
				axis[js.number] = js.value;
			break;
			case JS_EVENT_BUTTON:
				button[js.number] = js.value;
			break;
		}
		read(joy_fd, &js , sizeof(struct js_event));

		int linearXL = map(axis[0],-32767,32767,-10000,10000);
		int linearYL = map(axis[1],-32767,32767,10000,-10000);

		int linearYR = map(axis[3],-32767,32767,-10000,10000);

		// int speed_R = ((linearX-linearY)+0)*(-1);  //OLD VALUE
  // 		int speed_L = (linearX+linearY)-0;

		// speed_L = 


  		

  		// printf("SpeedR %d SpeedL %d\n\n",speed_R,speed_L);
  		driveMotor(linearYR,linearYL); // send speed
		// int readKey = 0;
		// scanf("%d",&readKey);

		pid_t pid = proc_find("./play");

		if(pid == -1){

		}else{

		}


		// eye and neck -----------------------------------------------------------

		if(button[4] == 1){
			if(button[Y_KEY] == 1){
				if(check1==0){
					sendData(0,8,10,0); //eye top
					check1 = 1;
				}			
			}else{
				check1 = 0;
			}

			if(button[B_KEY] == 1){
				if(check2 == 0){
					sendData(0,6,20,0); //eye right
					check2 = 1;
				}
				
			}else{
				check2 = 0;
			}

			if(button[A_KEY] == 1){
				if(check3 == 0){
					sendData(0,2,20,0); //eye down
					check3 = 1;
				}
				
			}else{
				check3 = 0;
			}

			if(button[X_KEY] == 1){
				if(check4 == 0){
					sendData(0,4,20,0); //eye left
					check4 = 1;
				}
			}else{
				check4 = 0;
			}

			// if(button[4] == 1){
			// 	if(check5 == 0){

			// 		check5 = 1;
			// 	}
			// }else{
			// 	check5 = 0;
			// }

			if(button[5] == 1){
				if(check6 == 0){
					sendData(0,5,20,0); //eye center
					check6 = 1;
				}
			}else{
				check6 = 0;
			}


			if(button[6] == 1){
				if(check7 == 0){
					sendData(11,0,0,0); // left
					check7 = 1;
				}
			}else{
				check7 = 0;
			}

			if(button[7] == 1){
				if(check8 == 0){
					sendData(13,0,0,0); // right
					check8 = 1;
				}
			}else{
				check8 = 0;
			}

			// if(button[8] == 1){
			// 	if(check9 == 0){
			// 		sendData();
			// 		check9 = 1;
			// 	}
			// }else{
			// 	check9 = 0;
			// }

			// if(button[9] == 1){
			// 	if(check10 == 0){

			// 		check10 = 1;
			// 	}
			// }else{
			// 	check10 = 0;
			// }

			// if(button[10] == 1){
			// 	if(check11 == 0){
			// 		system("screen -dmS 'play' ./play Hello.mp3");
			// 		check11 = 1;
			// 	}
			// }else{
			// 	check11 = 0;
			// }

			if(button[11] == 1){
				if(check12 == 0){
					sendData(12,0,0,0); //center
					check12 = 1;
				}
			}else{
				check12 = 0;
			}

		}//------------------------------------------------------------------------------------------


		// tar
		else if(button[5] == 1){  
			if(button[Y_KEY] == 1){  // sa wad dee
				if(check1==0){
					sendData(1,0,10,0); //tar 1
					check1 = 1;
				}			
			}else{
				check1 = 0;
			}

			if(button[B_KEY] == 1){  // khob khun
				if(check2 == 0){
					sendData(2,0,20,0); //tar 2

					// gpioDelay(MILLIS(500));
					// system("screen -dmS 'play' ./play 0001.mp3");
					check2 = 1;
				}
				
			}else{
				check2 = 0;
			}

			if(button[A_KEY] == 1){		//Laugh
				if(check3 == 0){
					sendData(3,0,20,0); //tar 3

					gpioDelay(MILLIS(500));
					system("screen -dmS 'play' ./play 0006.mp3");
					check3 = 1;
				}
				
			}else{
				check3 = 0;
			}

			if(button[X_KEY] == 1){ 		//introduce myself
				if(check4 == 0){
					sendData(4,0,20,0); //tar 4   

					gpioDelay(MILLIS(500));
					system("screen -dmS 'play' ./play 0003.mp3");
					check4 = 1;
				}
			}else{
				check4 = 0;
			}

			if(button[4] == 1){
				if(check5 == 0){  // nan see da chai mai
					sendData(5,0,20,0); //tar 5

					gpioDelay(MILLIS(500));
					system("screen -dmS 'play' ./play 0007.mp3");
					check5 = 1;
				}
			}else{
				check5 = 0;
			}

			if(button[6] == 1){ // mong nan
				if(check7 == 0){
					sendData(6,0,20,0); //tar 6

					gpioDelay(MILLIS(500));
					system("screen -dmS 'play' ./play 0005.mp3");
					check7 = 1;
				}
			}else{
				check7 = 0;
			}

			if(button[7] == 1){   // i have a pen
				if(check8 == 0){
					sendData(7,0,20,0); //tar 7

					gpioDelay(MILLIS(500));
					system("screen -dmS 'play' ./play PPAP.mp3");
					check8 = 1;
				}
			}else{
				check8 = 0;
			}

			if(button[8] == 1){
				if(check9 == 0){   //promote
					sendData(8,0,0,0); //tar 8

					gpioDelay(MILLIS(500));
					system("screen -dmS 'play' ./play 0008.mp3");
					check9 = 1;
				}
			}else{
				check9 = 0;
			}

			// if(button[9] == 1){
			// 	if(check10 == 0){
			// 		sendData(9,0,0,0); //tar 9
			// 		check10 = 1;
			// 	}
			// }else{
			// 	check10 = 0;
			// }

			if(button[10] == 1){      //turn left
				if(check11 == 0){
					sendData(9,0,0,0); //tar 10

					gpioDelay(MILLIS(500));
					// system("screen -dmS 'play' ./play 0005.mp3");
					check11 = 1;
				}
			}else{
				check11 = 0;
			}

			if(button[11] == 1){
				if(check12 == 0){     //turn right
					sendData(10,0,0,0); //tar 11 

					gpioDelay(MILLIS(500));
					// system("screen -dmS 'play' ./play 0005.mp3");
					check12 = 1;
				}
			}else{
				check12 = 0;
			}
		}

		if(button[Y_KEY] == 1){   //play song sawad dee
			if(check1 == 0){
				system("screen -dmS 'play' ./play 0001.mp3");
				check1 = 1;
			}
		}else{
			check1 = 0;
		}

		if(button[B_KEY] == 1){    //play song khob khun
			if(check2 == 0){
				system("screen -dmS 'play' ./play 0002.mp3");
				check2 = 1;
			}
		}else{
			check2 = 0;
		}

		if(button[9] == 1){
				if(check10 == 0){
					sendData(0,0,0,0); //tar 9 clear
					check10 = 1;
				}
		}else{
			check10 = 0;
		}

		


		

	}
	serClose(fd);
	close(joy_fd);
    serialClose(fd);
	return 0;
}




unsigned char addChecksum(int angle,int time,int addr){
	unsigned char sum = angle + time + addr;
	sum &= 0xFF;
	sum = ~sum;
	return sum;
}


/*
angle - > 0-36
time - > 0 - 128 ; 10 is 1 second, So 128 is 12.8 second
addr - > 0-32 addr to control servo motor
isContinue 0-1 check to control motor mode
*/
void sendData(int angle,int time,int addr,int isContinue){
	unsigned char buff[5];
	int i;

	if(angle > 36) angle = 36;
	else if(angle < 0) angle = 0;
	else angle = angle;

	buff[0] = angle;
	buff[1] = time & 0x7F;

	isContinue &= 0x01;
	//if(isContinue == 1)? buff[2] = ((addr & 0x1F) | 0x20):buff[2] = ((addr & 0x1F) | 0x00);
	if(isContinue == 1)
		buff[2] = (addr & 0x3F) | 0x40;
	else if(isContinue == 0)
		buff[2] = (addr & 0x3F) | 0x00;
	buff[3] = addChecksum(buff[0],buff[1],buff[2]);
	buff[4] = 0x81;// end command
	printf("%d : %d : %d\n",buff[0],buff[1],buff[2]);
	
	for(i = 0;i < 5 ; i++){
		printf("\nOut : %d\n",buff[i]);
		fflush(stdout);
		serialPutchar(fd,buff[i]);
		gpioDelay(100);
	}
	fflush(stdout);
	gpioDelay(300);
	
}
