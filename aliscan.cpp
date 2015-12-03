// aliscan.cpp : Defines the entry point for the console application.

#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <math.h>
#include <limits>
#include <stdio.h>
#include <stdlib.h>
#include <bitset>
#include <vector>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

#include <gpib/ib.h>
#include <alibavaClient.h>

struct coord //simple struct used in a couple places
{
	double x;
	double y;
};

template<typename tocon> std::string ToString(tocon x);  //function used to convert some type tocon (int, float etc) to a standard string
void AlibavaRun(Alibava & client, std::string outputFile, int nevents, int evtBuffer, std::string cnfg = "");//this runs Alibava (may need to be replaced with acquire setup)
int Play(int Num); //provides audio feedback to the user
void GPIBCleanup(int Dev, const char * ErrorMsg); //
double XmmToSteps(double Xmm); //converts Xaxis mm to XY stage steps (using most recent calibration)
double YmmToSteps(double Ymm); //converts Yaxis mm to XY stage steps (using most recent calibration)
std::vector <bool> LimitCheck(double Xpos, double Ypos, double Xdisp, double Ydisp); //before any displacement check if the displacement will keep it in bounds
void Displaceum(int Dev, double& Xpos, double& Ypos, double dispX, double dispY, float XSpeed, float YSpeed); //displace the stage from Xpos,Ypos an amount of microns = dispX,dispY
void DisplaceSteps(int Dev, double& Xpos, double& Ypos, double dispX, double dispY, float XSpeed, float YSpeed); //displace the stage from Xpos,Ypos an amount of steps = dispX,dispY
void resetGPIBDevice(int dev);  //resets the GPIBDevice needed to allow for proper operation in consecutive runs
void XYHome(int Dev); //send the stage to its XY home 
void GPIBcommand(int Dev, std::string command); //give the command to the stage (consult manual for valid commands) no error checking of buffer cleaning!!
bool isMoving(int Dev, bool stddisp); //returns whether or not the stage is moving (actually can only be run if it isn't moving; with a long timeout time this running means it isn't moving and returns 0 accordingly)
std::vector<double> ReadUnidexPosition(int Dev); //read <X,Y> position in steps from the unidex +/- .5 step (rounding issue between internals and display)
std::vector<double> getAbsPosition(int Dev); //given a predefined origin this returns the Absolute position of the stage as <x,y>
template<typename Uin> void Userin(Uin& x); //basic user input sanitation this replaces std::cin and will prompt user for a proper value (slightly flaky but good enough for now)
void PrintDispVec(std::vector<coord> Vec, std::ofstream& dout); //provide diagnostic dump of the instructed displacements in microns for each iteration
void WriteAbsPos(int Dev); //Saves getAbsPosition(...) to the file Absolute.loc potentially ridding the need to return home every time
void WriteUniPos(int Dev);
void CleanXY(int Dev); //syncs unidex to TotalDisplacement

#define BDINDEX               0     // Board Index
#define PRIMARY_ADDR_OF_DMM   05     // Primary address of device
#define NO_SECONDARY_ADDR     0     // Secondary address of device
#define TIMEOUT               T1000s  // Timeout value = 1000 seconds
#define EOTMODE               1     // Enable the END message
#define EOSMODE               0     // Disable the EOS mode


int main()
{
  std::cout<<"STARTING ALISCAN"<<std::endl;
  char ErrorMnemonic[29][5] = { "EDVR", "ECIC", "ENOL", "EADR", "EARG",
				"ESAC", "EABO", "ENEB", "EDMA", "",
				"EOIP", "ECAP", "EFSO", "", "EBUS",
				"ESTB", "ESRQ", "", "", "",
				"ETAB", "ELCK", "EARM", "EHDL", "",
				"", "EWIP", "ERST", "EPWR" }; //GPIB errors

  int Dev = ibdev(BDINDEX, PRIMARY_ADDR_OF_DMM, NO_SECONDARY_ADDR,TIMEOUT, EOTMODE, EOSMODE);//initialization of the device	
  std::cout<<"Device " << Dev << " initialized" <<std::endl;

  std::vector<coord> DispVec;

  double absPosX = 0, absPosY = 0;
  double histX = -1, histY = -1; //determining the origin

  std::ifstream dhist;
  std::ifstream totDisp;
  double totDispX = -1, totDispY = -1;
  totDisp.open("NetDisp.loc");
  if (totDisp.good())
    {
      totDisp >> totDispX >> totDispY;
    }
  else
    {
      std::ofstream creation;
      creation.open("./NetDisp.loc");
      creation << "-1  -1" << std::endl;
    }

  //GET THE ORIGIN AND DECIDE WHAT HIST X, HIST Y SHOULD BE ///////////////////////////////////////////////////////
  std::ofstream dout;
  dhist.open("./Hist.loc");
	
  std::string runDirectory = "/home/alibava-user/AlibavaRun/";
  dout.open((runDirectory + "console.out").c_str());


  if (dhist.good() )
    {
      
      dhist >> histX >> histY;
      dhist.close();
		
      std::cout << "Position from Hist.loc: " << histX << ", " << histY << std::endl;
      //historic "checksum"
      std::cout<<"COMMUNICATING WITH DEVICE"<<std::endl;
      std::vector<double> initPos = ReadUnidexPosition(Dev);

      //TODO protect missing absolute.loc when have Hist.loc
      std::ifstream prevAbs;
      prevAbs.open("Absolute.loc");
      prevAbs >> absPosX >> absPosY;
      prevAbs.close();

      // totDispX from NetDisp.loc which intializes to -1,-1 and is overwritten by WriteUniPos()
      
      // so if Hist.loc position X + displacement from NetDisp.loc is within 1 step of Absolute.loc
      // same for Y 
      // Then rewrite Hist.loc to match Absolute.loc
      if (histX + totDispX - 1 <= absPosX && histX + totDispX + 1 >= absPosX && histY + totDispY - 1 <= absPosY && histY + totDispY + 1 >= absPosY)
	{
	  std::ofstream overwrt;
	  overwrt.open("Hist.loc");
	  overwrt << absPosX << "  " << absPosY << std::endl;
	  overwrt.close();
	}

      if (initPos[0] - 1 <= totDispX && initPos[0] + 1 >= totDispX && initPos[1] - 1 <= totDispY && initPos[1] + 1 >= totDispY) //the unidex report the same as historical within rounding error
	{
	  CleanXY(Dev); //sync up unidex and write out NetDisp.loc
	}
      else if (initPos[0] == 0 && initPos[1] == 0) //UNIDEX in STARTUP STATE OR HOME POSITION....
	{
	  int Home = 0;
	  std::cout << "Has the home Position been hit on the unidex?(1=yes 0=no 2=Unsure)" << std::endl; //disentangle the meaning of 0,0 readout
	  Userin(Home);

	  if (Home == 1) //the UNIDEX HAS BEEN SENT HOME AND THUS 0,0 IS HOME
	    {
	      std::ofstream mod;
	      mod.open("Hist.loc");
	      mod << "0  0" << std::endl;
	      mod.close();
	      mod.open("Absolute.loc");
	      mod << "0  0" << std::endl;
	      mod.close();
	      CleanXY(Dev);
	    }
	  else if (Home == 0)//the machine has most likely just been turned on and the origin is found in the Absolute.loc file....do nothing
	    {

	    }
	  else //someone or something may have messed with the position safeguard the 0,0 by making histX,histY -1,-1 (a point it can never reach by natural means) and deleting the Absolute.loc file
	    {
	      std::remove("Absolute.loc");
	      std::remove("Hist.loc");
	      std::remove("TotalDisp.loc");
	      histX = -1;
	      histY = -1;
	    }

	}
      else //something has gone wrong (odds are someone has moved the unidex!) to keep things proper report the error histX, histY = -1,-1 and deleting  Absolute.loc
	{
	  std::cout << "*****************************************************************************" << std::endl; dout << "*****************************************************************************" << std::endl;
	  std::cout << "WARNING! ABSOLUTE CHECKSUM FAILURE: UNIDEX HAS BEEN MOVED MANUALLY!!" << std::endl; dout << "WARNING! UNIDEX HAS BEEN MOVED MANUALLY!!" << std::endl;
	  std::cout << "DELETING LOCATION AND RESETTING" << std::endl; dout << "DELETING LOCATION AND RESETTING" << std::endl;
	  std::cout << "*****************************************************************************" << std::endl; dout << "*****************************************************************************" << std::endl;
	  std::cout << std::endl << std::endl; dout << std::endl << std::endl;
	  std::remove("Absolute.loc");
	  std::remove("Hist.loc");
	  std::remove("TotalDisp.loc");
	  histX = -1;
	  histY = -1;
	}
    }
  if (histX != -1 && histY != -1) //if in error the initial position is the absolute position
    {
      std::vector<double> initAbsPos = getAbsPosition(Dev);
      absPosX = initAbsPos[0];
      absPosY = initAbsPos[1];
    }




  bool LoadDispFile = false;
  float initSpeed = 1000;
  float XSpeed = 1000;
  float YSpeed = 1000;



  int tsteps = 0;//number of iterations to run
  double startX, startY, dispX, dispY;


  //USER input///////////////////////////////////////////////////
  std::cout << "total number of iterations: " << std::endl; dout << "total number of iterations: " << std::endl;
  Userin(tsteps);
  dout << tsteps << std::endl;


  std::cout << std::endl; dout << std::endl;
  std::cout << "starting X displacement (steps): " << std::endl; dout << "starting X displacement (steps): " << std::endl;
  Userin(startX); //the initial X displacement in step that occurs BEFORE any run of Alibava
  dout << startX << std::endl;

  std::cout << std::endl; dout << std::endl;
  std::cout << "starting Y displacement (steps): " << std::endl; dout << "starting Y displacement (steps): " << std::endl;
  Userin(startY); //the initial Y displacement in step that occurs BEFORE any run of Alibava
  dout << startY << std::endl;


  std::cout << std::endl; dout << std::endl;
  std::cout << "Load displacement file?(1=yes 0=no)" << std::endl;
  Userin(LoadDispFile); //the Disp.cnfg file should be used where Disp.cnfg is a plain text file in which there are 3 columns with each line being read in sequence

  //[number of times to do the following displacement]   [XDisp in um]  [YDisp in um]



  if (!LoadDispFile)//if the user is not using the Disp.cnfg it will perform each displacement below after each run for tsteps iterations 
    {
      std::cout << std::endl; dout << std::endl;
      std::cout << "X displacement (um): " << std::endl; dout << "X displacement (um): " << std::endl;
      Userin(dispX); //a positive value sends the box to the left (laser to the right)
      dout << dispX << std::endl;

      std::cout << std::endl; dout << std::endl;
      std::cout << "Y displacement (um):" << std::endl; dout << "Y displacement (um):" << std::endl;
      Userin(dispY);//a positive value sends the box forward (laser towards the back)
      dout << dispY << std::endl;

      for (int i = 0; i < tsteps; i++)//load the displacement command vecotr
	{
	  coord* disp = new coord;
	  disp->x = dispX;
	  disp->y = dispY;
	  DispVec.push_back(*disp);
	}
      system("clear");
    }
  else
    {
      system("clear");
      int total = 0;
      int time;
      double xval, yval;
      //std::ifstream din;
      //din.open("Disp.cnfg");
      std::ifstream infile("Disp.cnfg");
      //check for valid file here
      while (infile >> time >> xval >> yval)//load the vector
	{
	  total += time;
	  for (int i = 0; i < time; i++)
	    {
	      coord* disp = new coord;
	      disp->x = xval;
	      disp->y = yval;
	      DispVec.push_back(*disp);
	      //std::cout << xval << " " << yval << " " << DispVec.size() << std::endl;
	    }
	}
      if (tsteps != total + 1)//Disp.cnfg is law! tsteps will be ignored in this mode but this warning serves as a diagnostic
	{
	  std::cout << "*****************************************************************************" << std::endl; dout << "*****************************************************************************" << std::endl;
	  std::cout << "WARNING! FILE DOES NOT CONTAIN THE SAME NUMBER OF COMMANDS AS ITERATIONS" << std::endl; dout << "WARNING! FILE DOES NOT CONTAIN THE SAME NUMBER OF COMMANDS AS ITERATIONS" << std::endl;
	  std::cout << "OVERWRITING ITERATIONS WITH TOTAL COMMANDS FOUND IN: Disp.cnfg" << std::endl; dout << "OVERWRITING ITERATIONS WITH TOTAL COMMANDS FOUND IN: Disp.cnfg" << std::endl;
	  std::cout << "*****************************************************************************" << std::endl; dout << "*****************************************************************************" << std::endl;
	  std::cout << std::endl << std::endl; dout << std::endl << std::endl;
	}
      tsteps = total + 1;

      PrintDispVec(DispVec, dout);
    }

  ///////////////////////////////////////////////////////////////////////////
  //std::string cmdString;

  //LOAD THE DISPS INTO A VECTOR

  resetGPIBDevice(Dev); //must be done first to avoid issues with input buffer not clearing

  //std::thread t1(XYHome);
  //t1.join();
  if (histX == -1 || histY == -1) //if it does not remember where it is at send it home to reset
    {
      std::cout << "RESETTING XY POSITION" << std::endl; dout << "RESETTING XY POSITION" << std::endl;
      XYHome(Dev);//Sends the XY stage to the home position
      CleanXY(Dev);
    }
  if (startX != 0 || startY != 0)
    {

      std::cout << "MOVING TO INITIAL POSITION" << std::endl; dout << "MOVING TO INITIAL POSITION" << std::endl;
      DisplaceSteps(Dev, absPosX, absPosY, startX, startY, XSpeed, YSpeed);//takes the speed and initial coordinates and displaces the XY stage that number of steps at the given speed; also updates positions
      WriteAbsPos(Dev);
      WriteUniPos(Dev);
      std::vector<double> AbsPos = getAbsPosition(Dev);
      absPosX = AbsPos[0];
      absPosY = AbsPos[1];
    }
  std::cout << "Aliscan will now perform " << tsteps << " iterations..." << std::endl; dout << "Aliscan will now perform " << tsteps << " iterations..." << std::endl;
		

  //ALIBAVA RUN BELOW
  //std::cout<<"Please click reconnect and ensure the alibava board is talking to this program properly"<<std::endl;

  Alibava client;
  const char *uri = "http://localhost:10000";
  client.connect(uri);

  int resetStat = client.Reset();
  if(resetStat) {
    std::cerr << "Error resetting Alibava soap client, code = " << resetStat << std::endl;
    return resetStat;
  }

  ns1__Status status;
  client.getStatus(status);
  std::cout << "Status:" << std::endl;
  std::cout << status << std::endl;

  for (int i = 0; i < tsteps; i++)
    {
      //RESET THE BOARD HERE
      //myBoard.reset(1);
      double currentXdisp, currentYdisp;
      if (i != tsteps - 1)
	{

	  currentXdisp = DispVec[i].x;
	  currentYdisp = DispVec[i].y;
	}
      std::cout << std::endl << std::endl; dout << std::endl << std::endl;

      std::string runname = "log" + ToString(int((i + 1))) + ".dat";

      std::cout << "### SCAN ITERATION: " << i + 1 << std::endl; dout << "### SCAN ITERATION: " << i + 1 << std::endl;
      std::cout << "approx. position(steps): " << absPosX << " , " << absPosY << std::endl; dout << "approx. position(steps): " << absPosX << " , " << absPosY << std::endl;


      std::ofstream myFile;
      std::string FileName = runDirectory + runname; //given the run directory/run name construct the log file's name
      myFile.open(FileName.c_str()); //create/open the log file for writing
      myFile.close();


      std::string file = " --out=";
      file = file + FileName;
      std::cout << file << std::endl; //logging of log location (logception)
      dout << file << std::endl;

      AlibavaRun(client, FileName, 10000, 1,"default.ini"); //file,nevt,buff
      //usleep(10000);

      if (i != tsteps - 1) //for all but the last step 
	{
	  //Play(2);
	  //				std::cout << "Reset the Alibava board and hit any key to continue" << std::endl; dout << "Reset the Alibava board and hit any key to continue" << std::endl;
	  //	  std::cout<<"beginning sync" << std::endl;			  	
	  //std::cin.sync();
	  //	  std::cout<<"end sync" << std::endl;
	  //	int x;
	  //	std::cin>>x;
	  /*int pause = 0;
	    while (!pause)  //gating the reset of Alibava and the beginning of the next iteration
	    {
	    pause =kbhit();
	    }
	    getch();//manual gating for board reset
	    //displace!
	    */
	  Displaceum(Dev, absPosX, absPosY, currentXdisp, currentYdisp, XSpeed, YSpeed); //displace by x,y in microns before next iteration
	  WriteAbsPos(Dev);//update location
	  WriteUniPos(Dev);
	  std::vector<double> AbsPos = getAbsPosition(Dev);//update location
	  absPosX = AbsPos[0];
	  absPosY = AbsPos[1];
	}
      // usleep(100000);
    }
  //Play(3);
  dout.close();
	
  std::vector < double > FinalPos = getAbsPosition(Dev);//final position recording
  dout.open("Hist.loc");
  dout << FinalPos[0] << "  " << FinalPos[1];
  dout.close();
  dout.open("Absolute.loc");
  dout << FinalPos[0] << "  " << FinalPos[1];
  dout.close();
	
  return 0;
}

template<typename tocon> std::string ToString(tocon x)
{
	std::ostringstream result;
	result << x;
	return result.str();
}

void AlibavaRun(Alibava & client, std::string outputFile, int nevents, int evtBuffer,std::string cnfg)//run Alibava with options both fixed and user provided
{

  client.setDataFile(outputFile.c_str());
  int ret = client.startLaserRun(2000, false,512,-512,512,1);
  ns1__Status status;
    client.getStatus(status);
    std::cout << "Status:" << std::endl;
    std::cout << status << std::endl;

	std::stringstream stream;
	stream	<< "alibava-gui"
		//////////////////ENTER ALIBAVA OPTIONS HERE/////////////////////
		///////////////////////////////////////////////////////////////// 
		//<< " --fifo"							//run the true DAQ (rather than the emulator)
		<< " --no-gui"							//run without the GUI
		//<< " --emulator"						//starts the emulator. Remove if running the true DAQ)
		<< " --nevts=" << ToString(nevents)		//total # events
		<< " --sample=" << ToString(evtBuffer)	//buffer size (#events stored in MB)
		<< " --out=" << outputFile				//output file name
		<< " --laser"						//sets the run type
		<< " "
		<< cnfg;

	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
	//system(stream.str().c_str());
	usleep(10000);
}
//attempts at forcing a wait until XY stage is in position to proceed without user intervention (initialization)
void GPIBCleanup(int ud, const char * ErrorMsg)
{
//  printf("Error : %s\nibsta = 0x%x iberr = %d (%s)\n",ErrorMsg, ibsta(), iberr(),ErrorMnemonic[iberr()]);
	if (ud != -1)
	{
		printf("Cleanup: Taking device offline\n");
		ibonl(ud, 0);
	}
}
double XmmToSteps(double Xmm) //converts mms in the Xdirection into steps in the X direction
{
	return 996.445*Xmm;//wtf!?----> ((.02205183*pow(Xmm, 2)) + (9.951674*pow(10, 2)*Xmm) + 154.5733);
}
double YmmToSteps(double Ymm)//converts mms in the Ydirection into steps in the Y direction
{
	return 998.05*Ymm;//wtf?!----> ((-.0190008*pow(Ymm, 2) + 1002.2436*Ymm + 19.19316));
}
void Displaceum(int Dev, double& Xpos, double& Ypos, double dispX, double dispY, float XSpeed, float YSpeed)//displaces the stage x,y (in micrometers)
{	std::cout<< "Begin DisplaceUM" << std::endl;
	/*string is "I X F[XSPEED] D[XDISP] Y F[YSPEED] D[YDISP]*" */
	std::vector<bool> ValidMotion=LimitCheck(Xpos, Ypos, dispX, dispY);
	std::string cmdString;
	cmdString = "I X F";
	cmdString = cmdString + ToString(XSpeed) + " " + "D";
	
	if (dispX != 0 && ValidMotion[0])
	{
		cmdString = cmdString + ToString(XmmToSteps(dispX / 1000));
	}
	else
		cmdString = cmdString + "0";

	cmdString = cmdString + " Y F" + ToString(YSpeed) + " D";

	if (dispY != 0 && ValidMotion[1])
	{
		cmdString = cmdString + ToString(YmmToSteps(dispY / 1000)) + "*";
	}
	else
		cmdString = cmdString + "0*";
	std::cout<<"reading bit"<<std::endl;
	ibwrt(Dev,(cmdString.data()), cmdString.length() + 1);
	std::cout<<"done reading bit" << std::endl;
	
	while (isMoving(Dev, 1));//wait to continue until it is stopped moving


}
void DisplaceSteps(int Dev, double& Xpos, double& Ypos, double dispX, double dispY, float XSpeed, float YSpeed)//displaces the stage x,y (in steps)
{
	/*string is "I X F[XSPEED] D[XDISP] Y F[YSPEED] D[YDISP]*" */
	std::vector<bool> ValidMotion = LimitCheck(Xpos, Ypos, dispX, dispY);
	std::string cmdString;
	cmdString = "I X F";
	cmdString = cmdString + ToString(XSpeed) + " " + "D";

	if (dispX != 0 && ValidMotion[0])
	{
		cmdString = cmdString + ToString(dispX);
	}
	else
		cmdString = cmdString + "0";

	cmdString = cmdString + " Y F" + ToString(YSpeed) + " D";

	if (dispY != 0 && ValidMotion[1])
	{

		cmdString = cmdString + ToString(dispY) + "*";
	}
	else
		cmdString = cmdString + "0*";

	ibwrt(Dev, (cmdString.data()), cmdString.length() + 1);

	
	while (isMoving(Dev, 0));
}
std::vector <bool> LimitCheck(double Xpos, double Ypos, double Xdisp, double Ydisp)//checks to ensure that the motion about to be performed is still within boundary
{
	std::vector<bool> ValidMotion;

	if (Xpos + XmmToSteps(Xdisp / 1000) >= 0 && Xpos + XmmToSteps(Xdisp / 1000) <= 170000)
		ValidMotion.push_back(true);
	else
		ValidMotion.push_back(false);

	if (Ypos + YmmToSteps(Ydisp / 1000) >= 0 && Ypos + YmmToSteps(Ydisp / 1000) <= 180000)
		ValidMotion.push_back(true);
	else
		ValidMotion.push_back(false);

	return ValidMotion;

}
void resetGPIBDevice(int dev) //resets the unidex necessary to avoid errors
{
	//std::thread t1(Play,1);
	//t1.detach();
	std::string cmdString = "7F";
	ibwrt(dev, (cmdString.data()), cmdString.length() + 1);
}
void XYHome(int Dev) //send XY stage home
{
	std::string cmdString = "I H XY*"; 
  //std::cout<<cmdString.data()<<std::endl;

  // int x;
  //std::cin>>x;
  
	ibwrt(Dev, (cmdString.data()), cmdString.length() + 1);

	while (isMoving(Dev,0));
}
void GPIBcommand(int Dev, std::string command)//generic command input to the unidex see programming manual for valid strings
{
	ibwrt(Dev, (char*)(command.data()), command.length() + 1);
}
bool isMoving(int Dev, bool stddisp) //read the status of the unidex....but actually is only done provided no other command is executing....
{

	if(!stddisp)
{
	uint8_t ReadBufferBits[100];
	std::string cmdString = "PS";

	ibwrt(Dev, (cmdString.data()), cmdString.length() + 1);
	ibrd(Dev, ReadBufferBits, 100);
	for (int i = 0; i < 15; i++)
	{
		std::bitset<8> value;
		value = ReadBufferBits[i];
	}

	return 0; //oh look the above ran.....it must not be moving....
}
else
{
 usleep(1000000);
 return 0;
}
}
std::vector<double> getAbsPosition(int Dev)//,double histX,double histY)
{
	double histX = -1, histY = -1;
	//std::cout << "GETTING POSITION" << std::endl;
	std::vector<double> AbsPosition;
	std::vector<double> UniPosition=ReadUnidexPosition(Dev);
	std::ifstream histin;
	histin.open("Hist.loc");
	if (histin.good())
	{
		histin >> histX >> histY;
	}
	if (histX != -1 && histY != -1)
	{
		AbsPosition.push_back(UniPosition[0] + histX);//unidex reports relative displacement....so we add that displacement to the origin histX,histY
		AbsPosition.push_back(UniPosition[1] + histY);
	}
	else
	{
		AbsPosition.push_back(UniPosition[0]);//unidex reports relative displacement....so we add that displacement to the origin histX,histY
		AbsPosition.push_back(UniPosition[1]);
	}
	histin.close();
	return AbsPosition;
}
std::vector<double> ReadUnidexPosition(int Dev)//reading the position off unidex
{
	//std::cout << "GETTING POSITION" << std::endl;
	std::vector<double> Position;
	std::string cmdString = "Px";
	char outputx[100];
	char outputy[100];

	for (int i = 0; i < 100; i++)
	{
		outputx[i] = 'a';
		outputy[i] = 'a';
	}

	std::cout<<"ReadUnidex Write 1" <<std::endl;
	ibwrt(Dev, (cmdString.data()), cmdString.length() + 1);
	std::cout<<"ReadUnidex Read 1" <<std::endl;
	ibrd(Dev, outputx, 10000);
	std::cout<<"ReadUnidex Done Read1" <<std::endl;

	std::string xpos = "";

	int j = 0;
	while (outputx[j] != 'a')
	{
		xpos = xpos + outputx[j];
		j++;
	}

	cmdString = "Py";
	std::cout<<"ReadUnidex Write 2" <<std::endl;
	ibwrt(Dev, (cmdString.data()), cmdString.length() + 1);
	std::cout<<"ReadUnidex Read 2" <<std::endl;	
	ibrd(Dev, outputy, 10000);

	std::string ypos = "";

	j = 0;
	while (outputy[j] != 'a')
	{
		ypos = ypos + outputy[j];
		j++;
	}
	double x = strtod(xpos.c_str(),NULL);
	double y = strtod(ypos.c_str(),NULL);

	Position.push_back(x);
	Position.push_back(y);

	return Position;
}
template<typename Uin> void Userin(Uin& x) //std::cin replacement
{
	Uin input;
		bool valid = false;
		do
		{
			std::cin >> input;
			if (std::cin.good())
			{
				//everything went well, we'll get out of the loop and return the value
				valid = true;
			}
			else
			{
				//something went wrong, we reset the buffer's state to good
				std::cin.clear();
				//and empty it
				
				std::cin.ignore(99999999999999999, '\n');
				std::cout << "Invalid input; please re-enter." << std::endl;
			}
		} while (!valid);

		x = input;
	
}
void PrintDispVec(std::vector<coord> Vec, std::ofstream& dout) //prints the full Displace vector
{
	std::cout << std::setw(11) << "iter" << std::setw(11) << "Xdisp(um)" << std::setw(11) << "Ydisp(um)" << std::endl; dout << std::setw(11) << "iter" << std::setw(11) << "Xdisp(um)" << std::setw(11) << "Ydisp(um)" << std::endl;
	for (int i = 0; i < int(Vec.size()); i++)
	{
		std::cout << std::setw(11) << i + 1 << std::setw(11) << Vec[i].x << std::setw(11) << Vec[i].y << std::endl;  dout << std::setw(5) << i + 1 << std::setw(11) << Vec[i].x << std::setw(11) << Vec[i].y << std::endl;
	}

	std::cout << std::endl << std::endl << std::endl; dout << std::endl << std::endl << std::endl;
}
void WriteAbsPos(int Dev)//write absolute location
{	std::cout << "WriteAbsPos Start" << std::endl;
	std::ofstream absposout;
	absposout.open("Absolute.loc");

	std::vector<double> AbsPos=getAbsPosition(Dev);

	absposout << AbsPos[0] << "  " << AbsPos[1] << std::endl;

	absposout.close();
}
void CleanXY(int Dev)
{
	GPIBcommand(Dev, "C*");
	//Sleep(150);
	usleep(150000);	
	resetGPIBDevice(Dev);
	GPIBcommand(Dev, "ATN*");
	resetGPIBDevice(Dev);
	WriteUniPos(Dev);
}
void WriteUniPos(int Dev)//write unidex location
{	std::cout << "WriteUniPos Start" << std::endl;
	std::ofstream uniposout;
	uniposout.open("NetDisp.loc");

	std::vector<double> UniPos = ReadUnidexPosition(Dev);

	uniposout << UniPos[0] << "  " << UniPos[1] << std::endl;

	uniposout.close();
}

